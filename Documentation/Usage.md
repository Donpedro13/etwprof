Usage
==========

Please see "Usage" on the [Readme page](../README.md) first.

```
etwprof

  Usage:
    etwprof profile --target=<PID_or_name> (--output=<file_path> | --outdir=<dir_path>) [--mdump [--mflags]] [--compress=<mode>] [--enable=<args>] [--cswitch] [--rate=<profile_rate>] [--nologo] [--verbose] [--debug] [--scache] [--children [--waitchildren]]
    etwprof profile (--output=<file_path> | --outdir=<dir_path>) [--compress=<mode>] [--enable=<args>] [--cswitch] [--rate=<profile_rate>] [--nologo] [--verbose] [--debug] [--scache] [--children [--waitchildren]] -- <process_path> [<process_args>...]
    etwprof profile --emulate=<ETL_path> --target=<PID> (--output=<file_path> | --outdir=<dir_path>) [--compress=<mode>] [--enable=<args>] [--cswitch] [--nologo] [--verbose] [--debug] [--children]
    etwprof --help
    etwprof --version

  Options:
    -h --help        Show this screen
    -v --verbose     Enable verbose output
    -t --target=<t>  Target (PID or exe name)
    -o --output=<o>  Output file path
    -d --debug       Turn on debug mode (even more verbose logging, preserve intermediate files, etc.)
    -m --mdump       Write a minidump of the target process(es) at the start of profiling
    --version        Show version information
    --mflags=<f>     Dump type flags for minidump creation in hex format [default: 0x0 aka. MiniDumpNormal]
    --children       Profile child processes, as well
    --waitchildren   Profiling waits for child processes as well, transitively
    --outdir=<od>    Output directory path
    --nologo         Do not print logo
    --rate=<r>       Sampling rate (in Hz) [default: use current global rate]
    --compress=<c>   Compression method used on output file ("off", "etw", or "7z") [default: "etw"]
    --enable=<args>  Format: (<GUID>|<RegisteredName>|*<Name>)[:KeywordBitmask[:MaxLevel['stack']]][+...]
    --scache         Enable ETW stack caching
    --cswitch        Collect context switch events as well
    --emulate=<f>    Debugging feature. Do not start a real time ETW session, use an already existing ETL file as input
```

Command line reference
----------

The following parameters might need further explanation:
* `-t --target`  
If you specify the executable's name (instead of the process ID), and there are more than one processes running with that name, etwprof will profile all of them.
* `-d --debug`  
Provides even more verbose output than `-v --verbose`. Intermediate results are retained (e.g. the unmerged `.etl` output).
* `--mflags`  
See the [documentation](https://msdn.microsoft.com/en-us/library/windows/desktop/ms680519(v=vs.85).aspx) for possible values.
* `--children`  
Child processes are included transitively. Profiling stops when the original target processes exit, even if there are still child processes running (see `--waitchildren`). Because ETW process start/end events are used for this feature, if ETW events are dropped, etwprof might miss child processes.
* `--outdir`  
With this option, etwprof will choose a convenient output filename for you.
* `--outdir`, `--output`  
Environment variables are explicitly expanded in these paths, so you can use them (such as `%USERPROFILE%`, `%TMP%`, etc.)
* `--rate`  
The profiler rate is **global**, and persistent until reboot.
* `--compress`  
Using the built-in compression (`etw`, the default) is convenient, as there is no manual decompression needed. Using 7-Zip (`7z`) results in smaller result files, but requires decompression before trace analysis.
* `--enable`  
Collects events from the specified user providers (filtered to the target processes). The syntax is very similar to xperf's [`-on`](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/start) switch. You can specify one or more providers by name, GUID, or prefixing the provider name with an astersik. The latter will infer the GUID using the [standard algorithm](https://blogs.msdn.microsoft.com/dcook/2015/09/08/etw-provider-names-and-guids/). You can filter events by keyword and level, and also request stack traces to be collected. It's best to have a look at some examples below.
* `--scache`  
Turns on ETW's stack caching feature. Using this option might reduce the result `.etl` file's size given enough duplicated call stacks. Use this if the profiled program has lots of hot spots and/or traced events with call stacks (e.g. user providers) are emitted from a limited variety of locations. Consumes up to 40 MBs of non-paged pool while profiling.
* `--emulate`  
Debugging feature. You can feed an already existing `.etl` file to etwprof with this, it will be filtered the same way as a real-time ETW session. Useful for reproducing bugs. Works with [xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)) traces only.

Examples
----------

* `etwprof profile -t=notepad.exe -o=D:\temp\mytrace.etl`
Profiles all running notepad.exe processes, writes result to the specified etl file.
* `etwprof profile --outdir=D:\ -- C:\Windows\System32\cmd.exe /C exit`
Starts cmd.exe with the specified command line arguments, and profiles it
* `etwprof profile -t=17816 --outdir=%TMP% --cswitch --compress=7z`
Profiles process with PID 17816, writes result to the temporary directory, collects context switch events, utilizes 7-zip compression.
* `etwprof profile -v --nologo -t=notepad.exe --outdir=D:\temp -m --rate=100 --children`
Profiles with a sample rate of 100 Hz, profiles child processes, creates a minidump, outputs verbose diagnostic messages.
* `etwprof profile @"D:\folder\some parameters.txt"`
Reads command-line parameters from the specified command file.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-RPC`
Collects all events from the `Microsoft-Windows-RPC` provider.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=*MyTraceloggerProvider:0xff`
Collects events from TraceLogging provider named `MyTraceloggerProvider`, but only those with a keyword that matches bitmask `0xff`.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-Win32k:~0x0200000010000000:4'stack'`
Collects events from `Microsoft-Windows-Win32k`, but ignores those **not** matching the specified keyword, and retains events with level 4 and below only. Also collects stack traces.
* `etwprof profile -t=notepad.exe --outdir=%TMP% --enable=Microsoft-Windows-Win32k:~0x10000000:'stack'+Microsoft-Windows-RPC+37f342ed-e45c-4b26-b3fe-450af9817fcd:0xff`
Enables various providers with various options.