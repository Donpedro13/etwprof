Frequently Asked Questions
==========

__Q:__ __Great, another open source sampling profiler for Windows...__  
__A:__ That's not a question.

__Q:__ __Fine. We already have plenty of profilers for Windows, why create a new one?__  
__A:__ Because there weren't any existing ones with the design goals mentioned on the [Readme page](../README.md).

__Q:__ __This profiler is based on ETW, but so are Windows Performance Recorder and xperf. How is this different from those tools?__  
__A:__ The key feature of etwprof is filtering: the result `.etl` file contains data only for the target process. Thus, the `.etl` file will be substantially smaller than a regular one. This makes processing and analyzing traces much more convenient.

__Q:__ __I need systemwide tracing. Can etwprof do that?__  
__A:__ No. Use [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder) or [xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)).

__Q:__ __Why does etwprof need administrative privileges?__  
__A:__ A so-called ETW kernel session is required for collecting sampled profile data. In order to create and control a kernel session, you have to run as an administrator.

__Q:__ __Requiring administrative privileges violates the design goal of being used as a support tool. What if an end user's account is not an admin (e.g. enterprise environments)?__  
__A:__ You are right. However, a kernel ETW session requires administrative privileges, there is no way around that. Not using ETW would violate other design goals, so we have to live with this. If an end user's account is not an admin, he/she should reach out to the IT department for assistance (asking for over-the-shoulder elevation). 

__Q:__ __Profiling fails with a message complaining about the failure of creating some `TraceRelogger` instance. What's the problem?__  
__A:__ If you are on Windows 7, this probably means that your system is out of date. Install updates, or alternatively, install the required update package manually (see [Limitations and known issues](./Limitations_known_issues.md) for details).

__Q:__ __When I open the result trace with [WPA](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer), it displays a lengthy warning dialog about dropped events. What's up with this?__  
__A:__ The real-time ETW kernel session etwprof uses generates events on the fly, and the system stores them in buffers. These buffers are flushed to the `.etl` file, however, if events are generated more quickly than they can be flushed, they might be discarded (dropped). If you encounter this warning, your trace might be unfit for analysis (depending on the type and number of lost events).

There are several ways to work around this:
* Decrease the frequency of events (e.g. decrease the sampling rate)
* Stop IO-intensive programs
* Use a faster drive for the result `.etl` file
* Use a different drive that of your target process is using