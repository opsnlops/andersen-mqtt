cmake_minimum_required(VERSION 3.26)
project(andersen_mqtt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(PkgConfig REQUIRED)

# Find Boost
find_package(Boost REQUIRED COMPONENTS system log)

# fmt
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 11.1.2
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


set(MQTT_STD_VARIANT 1)
set(MQTT_USE_LOG 1)
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


# Set up a concurrent queue
set(MOODYCAMEL_DIR "${CMAKE_SOURCE_DIR}/lib/moodycamel")

FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

include_directories(
        src/
        ${MOODYCAMEL_DIR}
)

add_executable(andersen_mqtt
        src/main.cpp
        src/mqtt/mqtt.cpp
        src/mqtt/mqtt.h
        src/namespace-stuffs.h
        src/mqtt/log_wrapper.cpp
        src/mqtt/log_wrapper.h
        src/window/window.h
        src/window/window.cpp
)

target_include_directories(andersen_mqtt PRIVATE ${MQTT_CPP_INCLUDE})

target_link_libraries(andersen_mqtt
        PUBLIC
        fmt::fmt
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
        Boost::system
        Boost::log
        nlohmann_json::nlohmann_json
)