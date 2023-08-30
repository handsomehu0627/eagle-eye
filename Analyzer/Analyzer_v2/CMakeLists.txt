cmake_minimum_required(VERSION 3.16.0)
project(Analyzer_v2 VERSION 2.0)

#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread ")


set(CMAKE_BUILD_TYPE DEBUG)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")


set(INCLUDE_DIR /usr/local/include)
set(LIB_DIR /usr/local/lib)

include_directories(${INCLUDE_DIR})
link_directories(${LIB_DIR})


find_package(OpenCV REQUIRED)

set(source
        Core/Analyzer.cpp
        Core/AvPullStream.cpp
        Core/AvPushStream.cpp
        Core/Config.cpp
        Core/ControlExecutor.cpp
        Core/GenerateAlarm.cpp
        Core/GenerateVideo.cpp
        Core/Scheduler.cpp
        Core/Server.cpp
        Core/Utils/Request.cpp
        main.cpp
        )

add_executable(Analyzer_v2 ${source})
target_link_libraries(Analyzer_v2 ${OpenCV_LIBS})
target_link_libraries(Analyzer_v2 event curl jsoncpp avformat avcodec avutil swscale swresample )





