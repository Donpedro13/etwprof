SET(sources
		TraceInfoDumper.cs
		TraceData.cs
		TraceDataXmlWriter.cs
		)


ADD_EXECUTABLE(TraceInfoDumper ${sources})

SOURCE_GROUP(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${sources})

SET_PROPERTY(TARGET TraceInfoDumper PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8")
SET_PROPERTY(TARGET TraceInfoDumper PROPERTY VS_PACKAGE_REFERENCES "Microsoft.Windows.EventTracing.Processing.All _1.7.4")
SET_PROPERTY(TARGET TraceInfoDumper PROPERTY VS_DOTNET_REFERENCES "System.Xml")

TARGET_COMPILE_OPTIONS(TraceInfoDumper PRIVATE -debug)