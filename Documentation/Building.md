Building
==========

You need the following prerequisites to build etwprof:
* [CMake](https://cmake.org/) (at least version 3.14)
* Visual Studio 2019 (or the same version of the [VS Build Tools](http://landinghub.visualstudio.com/visual-cpp-build-tools))

If you have both installed, the easiest way to generate a solution is to run `GenerateVSSolution.bat` in the root folder.

The official builds are made using the `Release_StaticCRT` and `Debug_StaticCRT` configurations.