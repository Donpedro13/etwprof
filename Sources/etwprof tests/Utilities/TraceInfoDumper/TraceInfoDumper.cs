/* This tool is a small utility used in the testing of etwprof. It takes an input ETL file path parameter, extracts 
 * some data, and dumps that to an a JSON file. For processing ETL files it utilizes the TraceProcessor library by
 * Microsoft (that's why it's written in C#, and not C++).
 */

using Microsoft.Windows.EventTracing;
using Microsoft.Windows.EventTracing.Processes;

using System;
using System.IO;

namespace TID
{
    class TraceInfoDumperApp
    {
        static (string input, string output) GetArgs(string[] args)
        {
            if (args.Length != 2)
                throw new ArgumentOutOfRangeException(nameof(args));

            return (input: args[0], output: args[1]);
        }

        static void FailWithMessage(string message)
        {
            Console.WriteLine($"ERROR: {message}!");

            Environment.Exit(-1);
        }

        static void PrintUsage()
        {
            Console.WriteLine($"Usage: TraceInfoDumper.exe [input ETL path] [output JSON path]");
        }

        static bool IsPathValid(string path)
        {
            try
            {
                Path.GetFullPath(path);

                return true;
            }
            catch
            {
                return false;
            }
        }

        static void Main(string[] args)
        {
            string input = null, output = null;
            try
            {
                (input, output) = GetArgs(args);
            }
            catch (ArgumentOutOfRangeException)
            {
                PrintUsage();
                FailWithMessage("Invalid arguments");
            }

            // Some best effort validation on arguments
            if (!File.Exists(input) || !Path.GetExtension(input).Equals(".etl", StringComparison.InvariantCultureIgnoreCase))
                FailWithMessage("Invalid input file argument");

            if (!IsPathValid(output) || !Path.GetExtension(output).Equals(".json", StringComparison.InvariantCultureIgnoreCase))
                FailWithMessage("Invalid output file argument");

            TraceData traceData = null;
            try
            {
                traceData = new TraceData(input);
            }
            catch (Exception e)
            {
                FailWithMessage($"Unable to gather data from the ETL file: {e.Message}");
            }

            try
            {
                TraceDataJsonWriter.Write(traceData, output);
            }
            catch (Exception e)
            {
                FailWithMessage($"Unable to write result JSON file: {e.Message}");
            }
        }
    }
}