cmake_minimum_required(VERSION 3.21)

project(rast LANGUAGES C)

set(RAST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(
    GLOB RAST_SRC
    "${RAST_SRC_DIR}/*.c"
)

add_executable(rast ${RAST_SRC})

find_package(PkgConfig REQUIRED)
pkg_check_modules(glib2 REQUIRED IMPORTED_TARGET glib-2.0)

find_package(SDL3 REQUIRED)
target_link_libraries(
    rast PRIVATE
    m
    SDL3::SDL3
    PkgConfig::glib2
)
