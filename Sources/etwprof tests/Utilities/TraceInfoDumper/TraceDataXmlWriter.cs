using System;
using System.IO;
using System.Xml;

namespace TID
{
    class TraceDataXmlWriter
    {
        private static void WriteProcesses(XmlWriter writer, TraceData traceData)
        {
            writer.WriteStartElement("processes");
            
            foreach(TraceData.Process process in traceData.Processes)
            {
                writer.WriteStartElement("process");

                    writer.WriteAttributeString("imageName", process.ImageName);
                
                    writer.WriteStartAttribute("pid");
                    writer.WriteValue(process.Id);
                    writer.WriteEndAttribute();

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        private static void WriteImages(XmlWriter writer, TraceData traceData)
        {
            writer.WriteStartElement("images");

            foreach (var item in traceData.Images)
            {
                writer.WriteStartElement("process");

                    writer.WriteAttributeString("imageName", item.Key.ImageName);
                
                    writer.WriteStartAttribute("pid");
                    writer.WriteValue(item.Key.Id);
                    writer.WriteEndAttribute();

                foreach (TraceData.Image image in item.Value)
                {
                    writer.WriteStartElement("image");

                        writer.WriteAttributeString("name", image.ImageName);

                    writer.WriteEndElement();
                }

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        private static void WriteThreads(XmlWriter writer, TraceData traceData)
        {
            writer.WriteStartElement("threads");

            foreach (var item in traceData.Threads)
            {
                writer.WriteStartElement("process");

                    writer.WriteAttributeString("imageName", item.Key.ImageName);

                    writer.WriteStartAttribute("pid");
                    writer.WriteValue(item.Key.Id);
                    writer.WriteEndAttribute();

                foreach (TraceData.Thread thread in item.Value)
                {
                    writer.WriteStartElement("thread");

                        writer.WriteStartAttribute("tid");
                        writer.WriteValue(thread.Id);
                        writer.WriteEndAttribute();

                    writer.WriteEndElement();
                }

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        private static void WriteSampledProfileCounts(XmlWriter writer, TraceData traceData)
        {
            writer.WriteStartElement("sampledProfileCounts");

            foreach (var item in traceData.SampledProfileCounts)
            {
                writer.WriteStartElement("process");

                    writer.WriteAttributeString("imageName", item.Key.ImageName);

                    writer.WriteStartAttribute("pid");
                    writer.WriteValue(item.Key.Id);
                    writer.WriteEndAttribute();
                
                    writer.WriteStartElement("count");
                    writer.WriteValue(item.Value);
                    writer.WriteEndElement();

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        private static void WriteContextSwitchCounts(XmlWriter writer, TraceData traceData)
        {
            writer.WriteStartElement("contextSwitchCounts");

            foreach (var item in traceData.ContextSwitchCounts)
            {
                writer.WriteStartElement("process");

                writer.WriteAttributeString("imageName", item.Key.ImageName);

                writer.WriteStartAttribute("pid");
                writer.WriteValue(item.Key.Id);
                writer.WriteEndAttribute();

                    writer.WriteStartElement("count");
                    writer.WriteValue(item.Value);
                    writer.WriteEndElement();

                writer.WriteEndElement();
            }

            writer.WriteEndElement();
        }

        public static void Write(TraceData data, string outputPath)
        {
            if (!Path.GetExtension(outputPath).Equals(".xml", StringComparison.CurrentCultureIgnoreCase))
                throw new ArgumentException("Output must be an xml file");

            XmlWriterSettings settings = new XmlWriterSettings();
            settings.Indent = true;

            using (XmlWriter writer = XmlWriter.Create(outputPath, settings))
            {
                writer.WriteStartDocument();

                    writer.WriteStartElement("traceData");

                        writer.WriteAttributeString("etlPath", data.EtlPath);

                        WriteProcesses(writer, data);
                        WriteImages(writer, data);
                        WriteThreads(writer, data);
                        WriteSampledProfileCounts(writer, data);
                        WriteContextSwitchCounts(writer, data);

                    writer.WriteEndElement();

                writer.WriteEndDocument();
            }
        }
    }
}
