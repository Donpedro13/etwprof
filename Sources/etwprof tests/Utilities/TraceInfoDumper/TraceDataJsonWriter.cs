using System;
using System.IO;
using Newtonsoft.Json;

namespace TID
{
    class TraceDataJsonWriter
    {
        private static void WriteProcess(JsonWriter writer, TraceData.Process process)
        {
            writer.WriteStartObject();
            writer.WritePropertyName("imageName");
            writer.WriteValue(process.ImageName);
            writer.WritePropertyName("pid");
            writer.WriteValue(process.Id);
            writer.WriteEndObject();
        }
        private static void WriteProcesses(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("processList");
            writer.WriteStartArray();
            foreach (var process in traceData.Processes)
            {
                WriteProcess(writer, process);
            }

            writer.WriteEndArray();
        }

        private static void WriteImages(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("imageLists");
            writer.WriteStartArray();

            foreach (var item in traceData.ImagesByProcess)
            {
                writer.WriteStartObject();
                writer.WritePropertyName("process");
                WriteProcess(writer, item.Key);

                writer.WritePropertyName("imageList");
                writer.WriteStartArray();

                foreach (TraceData.Image image in item.Value)
                {
                    writer.WriteValue(image.ImageName);
                }

                writer.WriteEndArray();

                writer.WriteEndObject();;
            }

            writer.WriteEndArray();
        }

        private static void WriteThreads(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("threadLists");
            writer.WriteStartArray();

            foreach (var item in traceData.ThreadsByProcess)
            {
                writer.WriteStartObject();
                writer.WritePropertyName("process");
                WriteProcess(writer, item.Key);

                writer.WritePropertyName("threadList");
                writer.WriteStartArray();

                foreach (TraceData.Thread thread in item.Value)
                {
                    writer.WriteValue(thread.Id);
                }

                writer.WriteEndArray();

                writer.WriteEndObject(); ;
            }

            writer.WriteEndArray();
        }

        private static void WriteSampledProfileCounts(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("sampledProfileCounts");
            writer.WriteStartArray();

            foreach (var item in traceData.SampledProfileCountsByProcess)
            {
                writer.WriteStartObject();

                writer.WritePropertyName("process");
                WriteProcess(writer, item.Key);

                writer.WritePropertyName("count");
                writer.WriteValue(item.Value);

                writer.WriteEndObject();
            }

            writer.WriteEndArray();
        }

        private static void WriteContextSwitchCounts(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("contextSwitchCounts");
            writer.WriteStartArray();

            foreach (var item in traceData.ContextSwitchCountsByProcess)
            {
                writer.WriteStartObject();

                writer.WritePropertyName("process");
                WriteProcess(writer, item.Key);

                writer.WritePropertyName("count");
                writer.WriteValue(item.Value);

                writer.WriteEndObject();
            }

            writer.WriteEndArray();
        }

        private static void WriteReadyThreadCounts(JsonWriter writer, TraceData traceData)
        {
            writer.WritePropertyName("readyThreadCounts");
            writer.WriteStartArray();

            foreach (var item in traceData.ReadyThreadCountsByProcess)
            {
                writer.WriteStartObject();

                writer.WritePropertyName("process");
                WriteProcess(writer, item.Key);

                writer.WritePropertyName("count");
                writer.WriteValue(item.Value);

                writer.WriteEndObject();
            }

            writer.WriteEndArray();
        }

        public static void Write(TraceData data, string outputPath)
        {
            if (!Path.GetExtension(outputPath).Equals(".json", StringComparison.CurrentCultureIgnoreCase))
                throw new ArgumentException("Output must be a json file");

            using (StreamWriter file = File.CreateText(outputPath))
            using (JsonWriter writer = new JsonTextWriter(file))
            {
                writer.Formatting = Formatting.Indented;

                writer.WriteStartObject();
                    writer.WritePropertyName("etlPath");
                    writer.WriteValue(data.EtlPath);

                    writer.WritePropertyName("data");
                    writer.WriteStartObject();

                        WriteProcesses(writer, data);
                        WriteImages(writer, data);
                        WriteThreads(writer, data);
                        WriteSampledProfileCounts(writer, data);
                        WriteContextSwitchCounts(writer, data);
                        WriteReadyThreadCounts(writer, data);

                    writer.WriteEndObject();
                writer.WriteEndObject();
            }
        }
    }
}
