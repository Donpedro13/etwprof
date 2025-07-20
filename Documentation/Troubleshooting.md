Troubleshooting
==========

In general, if you encounter any errors, it's worth increasing the verbosity of etwprof's output with either `-v` or `-d` (the latter is more verbose; see [Usage](./Usage.md) for details).

Crashes
----------

If you encounter any crashes, please create a minidump (you can instruct Windows Error Reporting to [automatically create a dump for you](https://msdn.microsoft.com/en-us/library/windows/desktop/bb787181(v=vs.85).aspx)), open a [GitHub issue](https://github.com/Donpedro13/etwprof/issues), and attach the dump with potential repro steps included.

Invalid/unusable output `.etl` file
----------

If you believe the result `.etl` file etwprof produced is incorrect, check if [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)/[xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)) handles your scenario correctly. It's best to run [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)/[xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)) and etwprof *in parallel*:
* Start tracing with [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)/[xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10))
* Start tracing with etwprof
* Perform your scenario
* Stop tracing with both tools
* Compare the two .etl files

If you don't see your specific problem in the trace recorded by [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)/[xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)), then chances are there is a bug in etwprof. Please file a [GitHub issue](https://github.com/Donpedro13/etwprof/issues), and attach a trace of your scenario recorded with **[xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10))**. We might be able to repro the problem with `--emulate` (see [Usage](./Usage.md) for details). As traces may contain personal information, it's best not to upload them to a publicly accessible location.