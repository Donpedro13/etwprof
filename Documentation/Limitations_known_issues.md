Limitations and known issues
==========

Windows version
----------

Only 64-bit OSes are supported at this time (profiling of 32-bit *processes* a.k.a. WoW64 is supported, however).

General limitations
----------

* Administrative privileges are required to use etwprof. This is needed because an ETW kernel session is used to collect sampled profile data.
* Your user must possess the "System Profile Privilege" (`SeSystemProfilePrivilege`). This is needed to turn on sampled profile data collection in the ETW kernel session. Administrators are usually granted this privilege, so chances are you don't have to worry about this.
* Sampled profiling in an ETW kernel session is interrupt based. This means that only threads actually running on some CPU will be sampled. For example, if your target application has all of its threads blocked (not running) during profiling, nothing will get sampled. In this scenario (potential non-busy hang) a minidump or a regular ETW trace with context switch information will be more useful.
* If the process to be profiled will be started by etwprof (command line specified after `--`), then:
  * it will be started with the same privileges etwprof has
  * if the profilee is a console application, a new console window will be created
  * the "normal" command line arguments of etwprof should not contain double dashes
* When using the `--waitchildren` parameter, profiling might not stop automatically when all child processes exit. Keep this in mind when running the tool unattended. This is due to a race condition related to how etwprof waits for target processes and the reuse of process IDs by the operating system.