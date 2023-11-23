cmake_minimum_required(VERSION 3.26)
project(andersen_mqtt)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(PkgConfig REQUIRED)

# Find Boost
find_package(Boost REQUIRED COMPONENTS system)

# fmt
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 10.1.1
)
FetchContent_MakeAvailable(fmt)
set(FMT_HEADER_ONLY ON)
set(FMT_LOCALE ON)


# spdlog
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)


FetchContent_Declare(
        mqtt_cpp
        GIT_REPOSITORY https://github.com/redboltz/mqtt_cpp.git
        GIT_TAG v13.2.1
)

FetchContent_GetProperties(mqtt_cpp)
if(NOT mqtt_cpp_POPULATED)
    FetchContent_Populate(mqtt_cpp)
    set(MQTT_CPP_INCLUDE ${mqtt_cpp_SOURCE_DIR}/include)
endif()


file(GLOB programFiles
        src/*
)

add_executable(andersen_mqtt
        ${programFiles}
)

target_include_directories(andersen_mqtt PRIVATE ${MQTT_CPP_INCLUDE})

target_link_libraries(andersen_mqtt
        PUBLIC
        fmt::fmt
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
        Boost::system
)