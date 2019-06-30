Usage
==========

Please see "Usage" on the [Readme page](../README.md) first.

```
etwprof

  Usage:
    etwprof profile --target=<PID_or_name> (--output=<file_path> | --outdir=<dir_path>) [--mdump [--mflags]] [--compress=<mode>] [--enable=<args>] [--cswitch] [--rate=<profile_rate>] [--nologo] [--verbose] [--debug]
    etwprof profile --emulate=<ETL_path> --target=<PID> (--output=<file_path> | --outdir=<dir_path>) [--compress=<mode>] [--enable=<args>] [--cswitch] [--nologo] [--verbose] [--debug]
    etwprof --help

  Options:
    -h --help        Show this screen
    -v --verbose     Enable verbose output
    -t --target=<t>  Target (PID or exe name)
    -o --output=<o>  Output file path
    -d --debug       Turn on debug mode (even more verbose logging, preserve intermediate files, etc.)
    -m --mdump       Write a minidump of the target process at the start of profiling
    --mflags=<f>     Dump type flags for minidump creation in hex format [default: 0x0 aka. MiniDumpNormal]
    --outdir=<od>    Output directory path
    --nologo         Do not print logo
    --rate=<r>       Sampling rate (in Hz) [default: use current global rate]
    --compress=<c>   Compression method used on output file ("off", "etw", or "7z") [default: "etw"]
    --enable=<args>  Format: (<GUID>|<RegisteredName>|*<Name>)[:KeywordBitmask[:MaxLevel['stack']]][+...]
    --cswitch        Collect context switch events as well (beta feature)
    --emulate=<f>    Debugging feature. Do not start a real time ETW session, use an already existing ETL file as input
```

Command line reference
----------

The following parameters might need further explanation:
* `-t --target`  
If you specify the executable's name (instead of the process ID), etwprof will fail if there are more than one instances of the target process running.
* `-d --debug`  
Provides even more verbose output than `-v --verbose`. Intermediate results are retained (e.g. the unmerged `.etl` output).
* `--mflags`  
See the [documentation](https://msdn.microsoft.com/en-us/library/windows/desktop/ms680519(v=vs.85).aspx) for possible values.
* `--outdir`  
With this option, etwprof will choose a convenient output filename for you.
* `--outdir`, `--output`  
Environment variables are explicitly expanded in these paths, so you can use them (such as `%USERPROFILE%`, `%TMP%`, etc.)
* `--rate`  
The profiler rate is **global**, and persistent until reboot.
* `--cswitch`  
Does not work reliably (see [Limitations and known issues](./Limitations_known_issues.md) for details).
* `--emulate`  
Debugging feature. You can feed an already existing `.etl` file to etwprof with this, it will be filtered the same way as a real-time kernel session. Useful for reproducing bugs. Works with [xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)) traces only.
* `--compress`  
Using the built-in compression (`etw`, the default) is convenient, as there is no manual decompression needed. Using 7-Zip (`7z`) results in smaller result files, but requires decompression before trace analysis.
* `--enable`  
Collects events from the specified user providers (filtered to the target process). The syntax is very similar to xperf's [`-on`]() switch. You can specify one or more providers by name, GUID, or prefixing the provider name with an astersik. The latter will infer the GUID using the [standard algorithm](https://blogs.msdn.microsoft.com/dcook/2015/09/08/etw-provider-names-and-guids/). You can filter events by keyword and level, and also request stack traces to be collected. It's best to have a look at some examples below.

Examples
----------

* `etwprof profile -t=notepad.exe -o=D:\temp\mytrace.etl`  
Profiles notepad.exe, writes result to the specified etl file.
* `etwprof profile -t=17816 --outdir=%TMP% --cswitch --compress=7z`  
Profiles process with PID 17816, writes result to the temporary directory, collects context switch events, utilizes 7-zip compression.
* `etwprof profile -v --nologo -t=notepad.exe --outdir=D:\temp -m --rate=100`  
Profiles with a sample rate of 100 Hz, creates a minidump, outputs verbose diagnostic messages.
* `etwprof profile @"D:\folder\some parameters.txt"`  
Reads command-line parameters from the specified command file.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-RPC`  
Collects all events from the `Microsoft-Windows-RPC` provider.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=*MyTraceloggerProvider:0xff`
Collects events from TraceLogging provider named `MyTraceloggerProvider`, but only those with a keyword that matches bitmask `0xff`.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-Win32k:~0x0200000010000000:4'stack'`  
Collects events from `Microsoft-Windows-Win32k`, but ignores those **not** matching the specified keyword, and retains events with level 4 and below only. Also collects stack traces.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-Win32k:~0x10000000:'stack'+Microsoft-Windows-RPC+37f342ed-e45c-4b26-b3fe-450af9817fcd:0xff:0`  
Enables various providers with various options.