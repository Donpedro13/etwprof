Usage
==========

Please see "Usage" on the [Readme page](../README.md) first.

```
etwprof

  Usage:
    etwprof profile --target=<PID_or_name> (--output=<file_path> | --outdir=<dir_path>) [--mdump [--mflags]] [--compress=<mode>] [--cswitch] [--rate=<profile_rate>] [--nologo] [--verbose] [--debug]
    etwprof profile --emulate=<ETL_path> --target=<PID> (--output=<file_path> | --outdir=<dir_path>) [--compress=<mode>] [--cswitch] [--nologo] [--verbose] [--debug]
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

Examples
----------

* `etwprof profile -t=notepad.exe -o=D:\temp\mytrace.etl`
* `etwprof profile -t=17816 --outdir=%TMP% --cswitch --compress=7z`
* `etwprof profile -v --nologo -t=notepad.exe --outdir=D:\temp -m --rate=100`
* `etwprof profile @"D:\folder\some parameters.txt"`