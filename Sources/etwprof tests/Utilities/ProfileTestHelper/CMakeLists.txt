SET(util_sources
		OperationRegistrar.hpp
		OperationRegistrar.cpp
		ProfileTestHelper.cpp
		Utility.hpp
		Utility.cpp

		"${CMAKE_CURRENT_SOURCE_DIR}/ETW providers/Manifest based/MB.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ETW providers/Manifest based/MB.rc"
		
		${CMAKE_CURRENT_SOURCE_DIR}/Operations/DoNothingOperation.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/Operations/RegUnRegETWOperation.cpp

		"${CMAKE_CURRENT_SOURCE_DIR}/Test cases/BurnCPU5sCase.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Test cases/ETWCases.cpp"
		)

# Manifest files are needed runtime in order to register the providers
SET(manifest_target_location
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/MB.man)

ADD_CUSTOM_COMMAND(OUTPUT ${manifest_target_location} COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/ETW providers/Manifest based/MB.man" ${manifest_target_location})

SET(manifest_files
		${manifest_target_location})

ADD_EXECUTABLE(ProfileTestHelper ${util_sources} ${manifest_files})

SOURCE_GROUP(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${util_sources})

TARGET_INCLUDE_DIRECTORIES(ProfileTestHelper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})