etwprof
==========

[![Build Status](https://dev.azure.com/donpedro901/etwprof/_apis/build/status/Donpedro13.etwprof?branchName=master)](https://dev.azure.com/donpedro901/etwprof/_build/latest?definitionId=2&branchName=master)

etwprof is a lightweight, self-contained sampling profiler for native applications on Windows. It's based on the [Event Tracing for Windows](https://msdn.microsoft.com/en-us/library/windows/desktop/bb968803(v=vs.85).aspx) (ETW) framework.

This profiler has the following design goals:
* No installable dependencies of any sort
* Lightweight
* Processor architecture and model independent
* It must be viable to be used as a technical support tool

Unlike Microsoft provided ETW-based performance profilers (such as [xperf](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-8.1-and-8/hh162920(v=win.10)), [Windows Performance Recorder](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder), etc.), etwprof performs filtering, so sampled profile data relevant only to the target processes are saved. This results in much smaller `.etl` output files compared to other ETW-based tools.

Usage
----------

etwprof is a command line tool. To profile (already running) processes, launch etwprof as an Administrator, and pass either the PID or the name of the executable that you'd like to profile. The following example will profile `notepad.exe`, write the resulting `.etl` file to the user's home folder, and will also create one minidump at the beginning:

`etwprof profile -t=notepad.exe --outdir=%USERPROFILE% -m`

While profiling is in progress, you should see a colorful, spinning progress indicator:

![Profile feedback](Documentation/profile_feedback.png "Profile feedback")

Profiling stops either when you stop it explicitly by pressing `CTRL+C`, or when the target processes exit.

The resulting `.etl` file can be opened with a tool of your choice, such as [Windows Performance Analyzer](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer). If you open the trace with WPA, there's a small quirk: CPU usage not belonging to the target processes will show up attributed to "Unknown (0)". This can be worked around easily with filtering:

![WPA](Documentation/wpa.png "WPA")

For a list of command line options and more examples, see the dedicated documentation page about [Usage](Documentation/Usage.md). If you have any questions, please consult the [FAQ](Documentation/FAQ.md) page first.

System requirements
----------

etwprof requires no 3rd party installations (no Visual Studio redistributable, no Windows SDK, etc.).

At least Windows 10 (TODO any specific version?) is required for this program to operate.

Only 64-bit OSes are supported at this time (profiling of 32-bit *processes* a.k.a. WoW64 is supported, however).

Download
------------

The latest version of etwprof is __0.3__ (released 2024-04-27). Binary distributions are available on the [Releases](https://github.com/Donpedro13/etwprof/releases) page.

For a list of fixes and improvements, check [CHANGELOG.txt](Misc/CHANGELOG.txt).

Documentation
------------

More detailed documentation is available in the "Documentation" folder:
* [Building](Documentation/Building.md)
* [FAQ](Documentation/FAQ.md)
* [Limitations and known issues](Documentation/Limitations_known_issues.md)
* [Releases](Documentation/Releases.md)
* [Theory of operation](Documentation/Theory_of_operation.md)
* [Troubleshooting](Documentation/Troubleshooting.md)
* [Usage](Documentation/Usage.md)

License
------------

This project is released under the MIT License. Please see [LICENSE.txt](LICENSE.txt) for details.
