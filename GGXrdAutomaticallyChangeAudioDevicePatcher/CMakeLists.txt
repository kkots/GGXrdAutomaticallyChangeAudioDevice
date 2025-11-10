# this CMakeLists.txt is for Linux compilation
cmake_minimum_required(VERSION "${MIN_VER_CMAKE}" FATAL_ERROR)
project(GGXrdAutomaticallyChangeAudioDevicePatcher)
set(CMAKE_CXX_STANDARD 14)
include_directories(${PROJECT_SOURCE_DIR})
add_executable(GGXrdAutomaticallyChangeAudioDevicePatcher
	GGXrdAutomaticallyChangeAudioDevicePatcherCommon.cpp
	InjectorCommonOut.cpp)
target_compile_definitions(GGXrdAutomaticallyChangeAudioDevicePatcher PRIVATE "-DFOR_LINUX=\"1\"")

# compile instructions
# cd into the directory with the CMakeLists.txt
#
# cmake -DCMAKE_BUILD_TYPE=Release .
# make
#
# The executable named "GGXrdAutomaticallyChangeAudioDevicePatcher" appears in the current directory.
# To launch, use:
#
# ./GGXrdAutomaticallyChangeAudioDevicePatcher