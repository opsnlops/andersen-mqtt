cmake_minimum_required(VERSION 3.26)
project(andersen_mqtt)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(PkgConfig REQUIRED)


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


add_executable(andersen_mqtt main.cpp)

target_link_libraries(andersen_mqtt
        PUBLIC
        fmt::fmt
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)