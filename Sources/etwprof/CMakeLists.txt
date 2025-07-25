SET(etwprof_sources
		PCH.hpp

		Main.cpp

		${CMAKE_CURRENT_SOURCE_DIR}/Application/Compat.manifest
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Version.rc

		${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Arguments.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Arguments.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Error.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/Error.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/ProgressFeedback.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Application/ProgressFeedback.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/Log/Logging.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Log/Logging.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWConstants.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWConstants.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWGUIDImpl.inl
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWSessionCommon.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWSessionCommon.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWSessionInterfaces.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/ETWSessionInterfaces.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/TraceRelogger.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/TraceRelogger.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/Utils.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/Utils.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/CombinedETWSession.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/ETW/CombinedETWSession.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/FileSystem/Utility.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/FileSystem/Utility.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/Minidump.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/Minidump.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessLifetimeEventSource.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessLifetimeEventSource.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessLifetimeObserver.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessLifetimeObserver.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessRef.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessRef.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessList.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/ProcessList.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/Utility.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/Utility.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/WaitableProcessGroup.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Process/WaitableProcessGroup.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/OStreamManipulators.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/OStreamManipulators.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/ConsoleOStream.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/ConsoleOStream.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/GlobalStreams.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/GlobalStreams.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Stream/StreamCommon.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/Synchronization/CriticalSection.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Synchronization/CriticalSection.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Synchronization/Event.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Synchronization/Event.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Synchronization/LockableGuard.hpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/OSTypes.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/ProfileInterruptRate.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/ProfileInterruptRate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/Time.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/Time.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/Win32Utils.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/Win32Utils.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/WinInternal.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Utility/WinInternal.cpp

		${CMAKE_CURRENT_SOURCE_DIR}/OS/Version/WinVersion.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/OS/Version/WinVersion.cpp

		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ETWProfiler.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ETWProfiler.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ETLReloggerProfiler.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ETLReloggerProfiler.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/IETWBasedProfiler.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/IETWBasedProfiler.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/IProfiler.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/IProfiler.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ProfilerCommon.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Profiler/ProfilerCommon.cpp

		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Asserts.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Asserts.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Exception.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Exception.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/EnumFlags.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/GUID.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/GUID.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Macros.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/OnExit.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/OnExit.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/ResponseFile.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/ResponseFile.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/Result.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/StringUtils.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/Utility/StringUtils.cpp
		)

# 3rd party binaries that need to be copied to the configuration's binary directory
SET(kerneltracecontrol_target_location
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/kerneltracecontrol.dll)
SET(7zr_target_location
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/7zr.exe)

if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(kerneltracecontrol_dll_name "kerneltracecontrol_x64.dll")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(kerneltracecontrol_dll_name "kerneltracecontrol_arm64.dll")
endif()

SET(kerneltracecontrol_source_location
        ${PROJECT_SOURCE_DIR}/3rdparty/${kerneltracecontrol_dll_name})

ADD_CUSTOM_COMMAND(OUTPUT ${kerneltracecontrol_target_location} COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/3rdparty/${kerneltracecontrol_dll_name} ${kerneltracecontrol_target_location})
ADD_CUSTOM_COMMAND(OUTPUT ${7zr_target_location} COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/3rdparty/7zr.exe ${7zr_target_location})

SET(3rdparty_binaries
		${kerneltracecontrol_target_location}
		${7zr_target_location})

ADD_EXECUTABLE(etwprof ${etwprof_sources} ${3rdparty_binaries})

# PCH
TARGET_PRECOMPILE_HEADERS(etwprof PRIVATE PCH.hpp)
# Work around limitation (?) introduced in VS 17.6 (/std:c++latest and CMake PCHs are not friends right now...)
SET_PROPERTY(TARGET etwprof PROPERTY VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/NoSTDModules.props")

SOURCE_GROUP(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${etwprof_sources})

TARGET_INCLUDE_DIRECTORIES(etwprof PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable CFG
TARGET_COMPILE_OPTIONS(etwprof PRIVATE "/guard:cf")
# Also require Windows 10 or later, so etwprof will fail to even launch on previous versions
SET_PROPERTY(TARGET etwprof APPEND PROPERTY LINK_FLAGS "/guard:cf /SUBSYSTEM:CONSOLE,10")	# There is no TARGET_LINK_OPTIONS...

# Version
SET(major_version 0)
SET(minor_version 4)
SET(patch_version 0)
TARGET_COMPILE_DEFINITIONS(etwprof PRIVATE -DETWP_MAJOR_VERSION=${major_version} -DETWP_MINOR_VERSION=${minor_version} -DETWP_PATCH_VERSION=${patch_version})

# Build info macros
EXECUTE_PROCESS(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

EXECUTE_PROCESS(
        COMMAND git name-rev --name-only ${GIT_COMMIT_HASH}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

TARGET_COMPILE_DEFINITIONS(etwprof PRIVATE 
    -DETWP_GIT_COMMIT_HASH=L"${GIT_COMMIT_HASH}"
    -DETWP_GIT_REV_NAME=L"${GIT_REV}"
)

TARGET_COMPILE_DEFINITIONS(etwprof
    PRIVATE
        -DETWP_BUILD_CONFIG=L\"$<CONFIG>\"
)

TARGET_LINK_LIBRARIES(etwprof dbghelp tdh)