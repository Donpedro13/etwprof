Limitations and known issues
==========

Windows version
----------

Only 64-bit OSes are supported at this time (profiling of 32-bit *processes* a.k.a. WoW64 is supported, however).

### Windows 7

* etwprof relies heavily on a COM class called [`TraceRelogger`](https://msdn.microsoft.com/en-us/library/windows/desktop/hh706657(v=vs.85).aspx). This was added to Windows 7 as part of an update. Because of this, if your system is not up to date, etwprof might not work. Make sure your system is up to date, or alternatively, install the required update's [standalone version](https://support.microsoft.com/en-us/help/2882822/update-adds-itracerelogger-interface-support-to-windows-embedded-stand).
* Only one kernel ETW session can exist at once. This means that you can't run multiple etwprof instances in parallel (or combined with other tools that use a kernel session, such as [xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)) or [WPR](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder)).
* Collecting events from user providers (`--enable`) is not supported

### Windows Server

Server editions are unsupported at this time. etwprof probably works, but it's untested on Server versions.

General limitations
----------

* Administrative privileges are required to use etwprof. This is needed because an ETW kernel session is used to collect sampled profile data.
* Your user must possess the "System Profile Privilege" (`SeSystemProfilePrivilege`). This is needed to turn on sampled profile data collection in the ETW kernel session. Administrators are usually granted this privilege, so chances are you don't have to worry about this.
* Sampled profiling in an ETW kernel session is interrupt based. This means that only threads actually running on some CPU will be sampled. For example, if your target application has all of its threads blocked (not running) during profiling, nothing will get sampled. In this scenario (potential non-busy hang) a minidump or a regular ETW trace with context switch information will be more useful.
* If the process to be profiled will be started by etwprof (command line specified after `--`), then:
  * it will be started with the same privileges etwprof has
  * if the profilee is a console application, a new console window will be created
  * the "normal" command line arguments of etwprof should not contain double dashes