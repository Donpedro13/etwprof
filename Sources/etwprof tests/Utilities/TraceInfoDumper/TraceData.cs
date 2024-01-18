using Microsoft.Windows.EventTracing;
using Microsoft.Windows.EventTracing.Cpu;
using Microsoft.Windows.EventTracing.Events;
using Microsoft.Windows.EventTracing.Processes;
using Microsoft.Windows.EventTracing.Symbols;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TID
{
    class EventStatisticsCreatorConsumer : IEventConsumer
    {
        Dictionary<int, Dictionary<Tuple<Guid, int>, int>> eventStatistics = new Dictionary<int, Dictionary<Tuple<Guid, int>, int>>();
        public Dictionary<int, Dictionary<Tuple<Guid, int>, int>> EventStatistics
        {
            get { return eventStatistics; }
        }
        public void Process(EventContext eventContext)
        {
            var e = eventContext.Event;

            // We don't have known process IDs for every event, and we try to handle these gracefully. It's quite a
            // mess, to be honest. The reasons are manifold:
            //   1.): Some events do not have an associated process ID. I guess this is becasue some (classic) events
            //        do not have a process ID field as part of their payload, but rather just a thread ID (e.g. sampled
            //        profile). It should be possible to "derive" the process ID from the thread ID (by maintaining a
            //        process -> threads "registry", and also tracking thread creation/destruction times), but that
            //        would be way too much work... Maybe TraceProcessor provides such functionality, but I couldn't
            //        find anything like that in the documentation. So if we encounter such an event, we simply
            //        aggregate them using a dummy value of zero.
            //   2.): Events from the "TraceEvent" provider are associated with the process that was (supposedly?)
            //        the Trace Controller (xperf.exe, WPRUI.exe, etc.). etwprof's filtered traces do not inlcude these
            //        processes. Since we do not know which process ID the Controller has, we will take care of these
            //        later, when we "swap" the plain process IDs to first-class process structs later
            //   3.): For some events, if there is no associated Process Id, sometimes the ProcessId property is null,
            //        and sometimes it's zero. That's a pity, because 0 is used for both the Idle, and Unknown processes.

            int processId = e.ProcessId ?? 0;
            // Events are ID'd by TraceEvent.Id (Id for classic, and Opcode for manifest-based events). However, for
            //   TraceLogging events, Id is always 0. To overcome this peculiarity, we use the Opcode for TraceLogging
            //   events, so etwprof's tests can function properly.
            int eventId = e.IsTraceLogging ? (int)e.Opcode : e.Id;
            var providerAndIdTuple = Tuple.Create(e.ProviderId, eventId);

            if (!eventStatistics.ContainsKey(processId))
            {
                eventStatistics.Add(processId, new Dictionary<Tuple<Guid, int>, int>());
                eventStatistics[processId].Add(providerAndIdTuple, 1);
            }
            else
            {
                var processDict = eventStatistics[processId];
                if (!processDict.ContainsKey(providerAndIdTuple))
                {
                    processDict.Add(providerAndIdTuple, 1);
                }
                else
                {
                    ++processDict[providerAndIdTuple];
                }
            }
        }
    }
    class TraceData
    {
        #region Trace data structs
        public struct Process
        {
            public Process(int id, string imageName)
            {
                Id = id;
                ImageName = imageName ?? "";
            }
            public int Id { get; }
            public string ImageName { get; }
        }

        public struct ProcessLifetimeInfo
        {
            public ProcessLifetimeInfo(long? startTime, long? endTime, int? exitCode)
            {
                StartTime = startTime;
                EndTime = endTime;
                ExitCode = exitCode;
            }
            public long? StartTime { get; }
            public long? EndTime { get; }
            public int? ExitCode { get; }
        }

        public struct Image
        {
            public Image(string imageName)
            {
                ImageName = imageName;
            }
            public string ImageName { get; }
        }

        public struct Thread
        {
            public Thread(int id)
            {
                Id = id;
            }
            public int Id { get; }
        }

        public struct ContextSwitchStatistic
        {
            public ContextSwitchStatistic(Process process, int count)
            {
                Process = process;
                Count = count;
            }
            public Process Process { get; }
            public int Count { get; }
        }

        public struct SampledProfileStatistic
        {
            public SampledProfileStatistic(Process process, int count)
            {
                Process = process;
                Count = count;
            }
            public Process Process { get; }
            public int Count { get; }
        }
        #endregion

        public TraceData(string etlPath)
        {
            if (!Path.GetExtension(etlPath).Equals(".etl", StringComparison.CurrentCultureIgnoreCase))
                throw new ArgumentException("Input must be an ETL file");

            EtlPath = etlPath;

            GatherTraceData();
        }

        private void GatherTraceData()
        {
            using (ITraceProcessor trace = TraceProcessor.Create(EtlPath))
            {
                IPendingResult<IProcessDataSource> pendingProcessData = trace.UseProcesses();
                IPendingResult<IThreadDataSource> pendingThreadData = trace.UseThreads();
                IPendingResult<ICpuSampleDataSource> pendingCpuSampleData = trace.UseCpuSamplingData();
                IPendingResult<ICpuSchedulingDataSource> pendingCpuSchedulingData = trace.UseCpuSchedulingData();
                IPendingResult<IStackDataSource> pendingStackData = trace.UseStacks();
                IPendingResult<IStackEventDataSource> pendingStackEventData = trace.UseStackEvents();

                // For "general" event enumeration, TraceProcessor does not provide Process objects, but raw
                // process Ids. So we collect statistics in two steps: first, we gather all events of all processes,
                // and associate them with process Ids. Then, we "swap" the process Ids for "first-class" process
                // structs.
                EventStatisticsCreatorConsumer eventStatisticsCreator = new EventStatisticsCreatorConsumer();
                trace.Use(eventStatisticsCreator);

                trace.Process();

                GatherProcessData(pendingProcessData.Result);
                GatherThreadData(pendingThreadData.Result);
                GatherCpuSampleData(pendingCpuSampleData.Result);
                GatherCpuSchedulingData(pendingCpuSchedulingData.Result);
                GatherStackData(pendingStackData.Result, pendingStackEventData.Result);
                GatherGeneralEventStatisticsData(eventStatisticsCreator.EventStatistics);
            }
        }

        private void GatherProcessData(IProcessDataSource processDataSource)
        {
            foreach (IProcess process in processDataSource.Processes)
            {
                Process ownProcess = new Process(process.Id, process.ImageName);
                ProcessesByPID.Add(process.Id, ownProcess);

                long? startTimeMsStamp = process.CreateTime?.DateTimeOffset.ToUnixTimeMilliseconds();
                long? endTimeMsStamp = process.ExitTime?.DateTimeOffset.ToUnixTimeMilliseconds();
                ProcessLifetimeInfo lifetimeInfo = new ProcessLifetimeInfo(startTimeMsStamp, endTimeMsStamp, process.ExitCode);
                ProcessLifetimesByProcess.Add(ownProcess, lifetimeInfo);

                foreach (IImage image in process.Images)
                {
                    if (!ImagesByProcess.ContainsKey(ownProcess))
                        ImagesByProcess.Add(ownProcess, new List<Image>());

                    ImagesByProcess[ownProcess].Add(new Image(image.FileName));
                }
            }
        }

        private void GatherThreadData(IThreadDataSource threadDataSource)
        {
            foreach (IThread thread in threadDataSource.Threads)
            {
                Process process = new Process(thread.Process.Id, thread.Process.ImageName);

                if (!ThreadsByProcess.ContainsKey(process))
                {
                    ThreadsByProcess.Add(process, new List<Thread>());
                }

                Thread t = new Thread(thread.Id);

                ThreadsByProcess[process].Add(t);
            }
        }

        private void GatherCpuSampleData(ICpuSampleDataSource cpuSampleDataSource)
        {
            foreach (ICpuSample sample in cpuSampleDataSource.Samples)
            {
                Process process = new Process(sample.Process.Id, sample.Process.ImageName);
                if (!SampledProfileCountsByProcess.ContainsKey(process))
                    SampledProfileCountsByProcess.Add(process, 1);
                else
                    ++SampledProfileCountsByProcess[process];
            }
        }

        private void GatherCpuSchedulingData(ICpuSchedulingDataSource cpuSchedulingDataSource)
        {
            void ProcessContextSwitch(IProcess etlProcess)
            {
                Process process = new Process(etlProcess.Id, etlProcess.ImageName);
                if (!ContextSwitchCountsByProcess.ContainsKey(process))
                    ContextSwitchCountsByProcess.Add(process, 1);
                else
                    ++ContextSwitchCountsByProcess[process];
            }

            foreach (IContextSwitch cswitch in cpuSchedulingDataSource.ContextSwitches)
            {
                // If we have a cswitch when the same process's thread is switched, do not count that as 2 context switches
                if (cswitch.SwitchIn.Process == cswitch.SwitchOut.Process && cswitch.SwitchIn.Process != null)
                {
                    ProcessContextSwitch(cswitch.SwitchIn.Process);
                }
                else
                {
                    if (cswitch.SwitchIn.Process != null)
                        ProcessContextSwitch(cswitch.SwitchIn.Process);

                    if (cswitch.SwitchOut.Process != null)
                        ProcessContextSwitch(cswitch.SwitchOut.Process);
                }
            }

            foreach (IReadyThreadEvent readyThread in cpuSchedulingDataSource.ReadyThreadEvents)
            {
                IProcess iProcess = readyThread.ReadiedThread.Process;
                Process process = new Process(iProcess.Id, iProcess.ImageName);

                if (!ReadyThreadCountsByProcess.ContainsKey(process))
                    ReadyThreadCountsByProcess.Add(process, 1);
                else
                    ++ReadyThreadCountsByProcess[process];
            }
        }

        private void GatherStackData(IStackDataSource stackDataSource, IStackEventDataSource stackEventDataSource)
        {
            foreach (IStackSnapshot stack in stackDataSource.Stacks)
            {
                Process process = new Process(stack.Process.Id, stack.Process.ImageName);
                IStackEvent origEvent = stack.GetEvent(stackEventDataSource);

                if (origEvent == null)
                    continue;

                // Note: unfortunately, IStackEvent (and friends) cannot distuingish TraceLogging events. Therefore,
                //   all TraceLogging events will have 0 as their Id...

                if (!StackCountsByProcessAndProviderAndId.ContainsKey(process))
                {
                    StackCountsByProcessAndProviderAndId.Add(process, new Dictionary<Tuple<Guid, int>, int>());
                    StackCountsByProcessAndProviderAndId[process].Add(Tuple.Create(origEvent.ProviderId, origEvent.Id), 1);
                }
                else
                {
                    var processDict = StackCountsByProcessAndProviderAndId[process];
                    var providerOpcodeTuple = Tuple.Create(origEvent.ProviderId, origEvent.Id);
                    if (!processDict.ContainsKey(providerOpcodeTuple))
                    {
                        processDict.Add(providerOpcodeTuple, 1);
                    }
                    else
                    {
                        ++processDict[providerOpcodeTuple];
                    }
                }
            }
        }

        private static Process unknownProcess = new Process(0, null);
        private void GatherGeneralEventStatisticsData(Dictionary<int, Dictionary<Tuple<Guid, int>, int>> eventStatistics)
        {
            foreach (var kvp in eventStatistics)
            {
                // The Trace Controller process won't be in ProcessesByPID, so we have to "convert" to the Unknown
                //   process. See the comment in EventStatisticsCreatorConsumer for details.
                if (!ProcessesByPID.ContainsKey(kvp.Key))
                {
                    if (EventCountsByProcessAndProviderAndId.ContainsKey(unknownProcess))
                    {
                        EventCountsByProcessAndProviderAndId[unknownProcess] =
                            EventCountsByProcessAndProviderAndId[unknownProcess].
                            Concat(kvp.Value).
                            GroupBy(x => x.Key).
                            ToDictionary(x => x.Key, x => x.Sum(y => y.Value));

                    }
                    else
                    {
                        EventCountsByProcessAndProviderAndId.Add(unknownProcess, kvp.Value);
                    }

                }
                else
                {
                    var process = ProcessesByPID[kvp.Key];
                    // In general, there should not be "duplicate" keys. Except when we "converted" the Trace Controller
                    //   process to the Unknown process. If that's the case, we aggregate counts.
                    if (EventCountsByProcessAndProviderAndId.ContainsKey(process))
                    {
                        EventCountsByProcessAndProviderAndId[process] =
                            EventCountsByProcessAndProviderAndId[process].
                            Concat(kvp.Value).
                            GroupBy(x => x.Key).
                            ToDictionary(x => x.Key, x => x.Sum(y => y.Value));
                    }
                    else
                    {
                        EventCountsByProcessAndProviderAndId.Add(process, kvp.Value);
                    }
                }
            }
        }

        public string EtlPath { get; }

        public Dictionary<int, Process> ProcessesByPID { get; } = new Dictionary<int, Process>();
        public Dictionary<Process, ProcessLifetimeInfo> ProcessLifetimesByProcess { get; } = new Dictionary<Process, ProcessLifetimeInfo>();
        public Dictionary<Process, List<Image>> ImagesByProcess { get; } = new Dictionary<Process, List<Image>>();
        public Dictionary<Process, List<Thread>> ThreadsByProcess { get; } = new Dictionary<Process, List<Thread>>();
        public Dictionary<Process, int> SampledProfileCountsByProcess { get; } = new Dictionary<Process, int>();
        public Dictionary<Process, int> ContextSwitchCountsByProcess { get; } = new Dictionary<Process, int>();
        public Dictionary<Process, int> ReadyThreadCountsByProcess { get; } = new Dictionary<Process, int>();
        public Dictionary<Process, Dictionary<Tuple<Guid, int>, int>> StackCountsByProcessAndProviderAndId { get; } = new Dictionary<Process, Dictionary<Tuple<Guid, int>, int>>();
        public Dictionary<Process, Dictionary<Tuple<Guid, int>, int>> EventCountsByProcessAndProviderAndId { get; } = new Dictionary<Process, Dictionary<Tuple<Guid, int>, int>>();
    }
}
