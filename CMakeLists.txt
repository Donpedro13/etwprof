# SOURCE_GROUP is used with TREE (avaliable since 3.8.0.)
# The /diagnostics:caret cl option is used (avaliable since 3.11.0.)
# Visual Studio 2019 support needed (avaliable since 3.14.0.)
CMAKE_MINIMUM_REQUIRED(VERSION 3.14.0 FATAL_ERROR)

# Check platform
IF(NOT WIN32)
	MESSAGE(FATAL_ERROR "etwprof can only be built on Windows!")
ENDIF()

# Check for VS 2019 generator
IF(NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio 16 2019")
    MESSAGE(FATAL_ERROR "Only Visual Studio 2019 projects are supported!")
ENDIF()

# Check for target bitness
IF(NOT "${CMAKE_GENERATOR_PLATFORM}" MATCHES "x64")
    MESSAGE(FATAL_ERROR "Currently, only 64-bit builds are supported!")
ENDIF()

PROJECT(etwprof)

# Global CMake configurations
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
SET_PROPERTY(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT etwprof)

# Build configurations
SET(CMAKE_CONFIGURATION_TYPES
	Debug_StaticCRT
	Release_StaticCRT
	Debug_DynamicCRT
	Release_DynamicCRT
	Analyze_Debug
	Analyze_Release)

SET(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
    "Configurations"
    FORCE)

# Overwrite "default" C++ compiler flags, set new ones
# (No need to do the same for linker flags, as on Windows
#  they contain the architecture only)
STRING(CONCAT CXX_FLAGS_IMPL
			  "/MP "				# Use parallel compilation
			  "/W4 "				# Warning level 4
			  "/WX "				# Treat warnings as errors
			  "/EHsc "				# Enable exceptions (w/o extern "C")
			  "/Zi "				# Debug info format is PDB
			  "/diagnostics:caret "	# Why is this not the default?
			  "/permissive- "		# More conformance
			  "/std:c++latest "		# Support for C++17+ features
			  )
SET(CMAKE_CXX_FLAGS	${CXX_FLAGS_IMPL})

# Add default linker flags
STRING(CONCAT EXE_LINKER_FLAGS_IMPL
			  "/DEBUG"	# Always generate PDBs
			  )
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXE_LINKER_FLAGS_IMPL}")

# Global definitions
ADD_DEFINITIONS(-DUNICODE -D_UNICODE)
ADD_DEFINITIONS(-DWINVER=0x601 -DWIN32_WINNT=0x601)	# Minimum supported version is Windows 7
IF("${CMAKE_GENERATOR_PLATFORM}" MATCHES "x64")
    ADD_DEFINITIONS(-DETWP_64BIT)
ELSE()
	ADD_DEFINITIONS(-DETWP_32BIT)
ENDIF()

# Compile flags for all debug-like builds
STRING(CONCAT BASE_DEBUG_FLAGS_IMPL
			  "/Od "			# Disable optimization
			  "/D _DEBUG "
			  "/D ETWP_DEBUG "
			  )
SET(BASE_DEBUG_FLAGS ${BASE_DEBUG_FLAGS_IMPL})

# Compile flags for all release-like builds
STRING(CONCAT BASE_RELEASE_FLAGS_IMPL
			  "/O2 " # Optimize for speed
			  "/D NDEBUG "
			  "/D ETWP_RELEASE "
			  )
SET(BASE_RELEASE_FLAGS ${BASE_RELEASE_FLAGS_IMPL})

# Configuration specific compiler and linker options
SET(CMAKE_CXX_FLAGS_DEBUG_STATICCRT "${BASE_DEBUG_FLAGS} /MTd")				# Static, debug CRT
SET(CMAKE_CXX_FLAGS_RELEASE_STATICCRT "${BASE_RELEASE_FLAGS} /MT")			# Static CRT
SET(CMAKE_CXX_FLAGS_DEBUG_DYNAMICCRT "${BASE_DEBUG_FLAGS} /MDd")			# Dynamic, debug CRT
SET(CMAKE_CXX_FLAGS_RELEASE_DYNAMICCRT "${BASE_RELEASE_FLAGS} /MD")			# Dynamic CRT
SET(CMAKE_CXX_FLAGS_ANALYZE_DEBUG "${BASE_DEBUG_FLAGS} /analyze /MDd")		# /analyze
SET(CMAKE_CXX_FLAGS_ANALYZE_RELEASE "${BASE_RELEASE_FLAGS} /analyze /MD")	# /analyze

SET(CMAKE_EXE_LINKER_FLAGS_DEBUG_STATICCRT "")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE_STATICCRT "")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG_DYNAMICCRT "")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE_DYNAMICCRT "")
SET(CMAKE_EXE_LINKER_FLAGS_ANALYZE_DEBUG "")
SET(CMAKE_EXE_LINKER_FLAGS_ANALYZE_RELEASE "")

# Set global directory for build output
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Binaries")
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Binaries")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Binaries")

ADD_SUBDIRECTORY(Sources)