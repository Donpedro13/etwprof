using Microsoft.Windows.EventTracing;
using Microsoft.Windows.EventTracing.Cpu;
using Microsoft.Windows.EventTracing.Processes;

using System;
using System.Collections.Generic;
using System.IO;

namespace TID
{
    class TraceData
    {
        #region Trace data structs
        public struct Process
        {
            public Process(int id, string imageName)
            {
                Id = id;
                ImageName = imageName;
            }
            public int Id { get; }
            public string ImageName { get; }
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

                trace.Process();

                GatherProcessData(pendingProcessData.Result);
                GatherThreadData(pendingThreadData.Result);
                GatherCpuSampleData(pendingCpuSampleData.Result);
                GatherCpuSchedulingData(pendingCpuSchedulingData.Result);
            }
        }

        private void GatherProcessData(IProcessDataSource processDataSource)
        {
            foreach (IProcess process in processDataSource.Processes)
            {
                Process ownProcess = new Process(process.Id, process.ImageName);
                Processes.Add(ownProcess);

                foreach (IImage image in process.Images)
                {
                    if (!Images.ContainsKey(ownProcess))
                        Images.Add(ownProcess, new List<Image>());

                    Images[ownProcess].Add (new Image(image.FileName));
                }
            }
        }

        private void GatherThreadData(IThreadDataSource threadDataSource)
        {
            foreach (IThread thread in threadDataSource.Threads)
            {
                Process process = new Process(thread.Process.Id, thread.Process.ImageName);

                if(!Threads.ContainsKey(process))
                    Threads.Add(process, new List<Thread>());

                Threads[process].Add(new Thread(thread.Id));
            }
        }

        private void GatherCpuSampleData(ICpuSampleDataSource cpuSampleDataSource)
        {
            foreach (ICpuSample sample in cpuSampleDataSource.Samples)
            {
                Process process = new Process(sample.Process.Id, sample.Process.ImageName);
                if (!SampledProfileCounts.ContainsKey(process))
                    SampledProfileCounts.Add(process, 1);
                else
                    ++SampledProfileCounts[process];
            }
        }

        private void GatherCpuSchedulingData(ICpuSchedulingDataSource cpuSchedulingDataSource)
        {
            void ProcessContextSwitch(IProcess etlProcess)
            {
                Process process = new Process(etlProcess.Id, etlProcess.ImageName);
                if (!ContextSwitchCounts.ContainsKey(process))
                    ContextSwitchCounts.Add(process, 1);
                else
                    ++ContextSwitchCounts[process];
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
        }

        public string EtlPath { get; }

        public List<Process> Processes { get; } = new List<Process>();
        public Dictionary<Process, List<Image>> Images { get; } = new Dictionary<Process, List<Image>>();
        public Dictionary<Process, List<Thread>> Threads { get; } = new Dictionary<Process, List<Thread>>();
        public Dictionary<Process, int> SampledProfileCounts { get; } = new Dictionary<Process, int>();
        public Dictionary<Process, int> ContextSwitchCounts { get; } = new Dictionary<Process, int>();
    }
}
