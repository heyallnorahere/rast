cmake_minimum_required(VERSION 3.21)

project(rast LANGUAGES C CXX)

find_package(SDL3 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(glib2 REQUIRED IMPORTED_TARGET glib-2.0)

set(RAST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(
    GLOB RAST_SRC
    "${RAST_SRC_DIR}/*.c"
)

set(CIMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cimgui")
set(IMGUI_DIR "${CIMGUI_DIR}/imgui")

set(IMGUI_BACKENDS sdl3)
set(IMGUI_LIBRARIES SDL3::SDL3)

set(IMGUI_INCLUDES
    ${CIMGUI_DIR}
    ${IMGUI_DIR}
    "${IMGUI_DIR}/backends"
)

set(IMGUI_SOURCE
    # cimgui
    "${CIMGUI_DIR}/cimgui.cpp"
    "${CIMGUI_DIR}/cimgui_impl.cpp"

    # imgui
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
)

set(IMGUI_DEFINES
    "-DIMGUI_USER_CONFIG=\"../cimconfig.h\""
    "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1"
    "-DIMGUI_IMPL_API=extern \"C\""
)

foreach(BACKEND_NAME ${IMGUI_BACKENDS})
    set(BACKEND_SOURCE "${IMGUI_DIR}/backends/imgui_impl_${BACKEND_NAME}.cpp")
    list(APPEND IMGUI_SOURCE ${BACKEND_SOURCE})

    string(TOUPPER ${BACKEND_NAME} NAME_UPPER)
    list(APPEND IMGUI_DEFINES "-DCIMGUI_USE_${NAME_UPPER}")
endforeach()

add_library(rast_imgui STATIC ${IMGUI_SOURCE})
target_compile_definitions(rast_imgui PUBLIC ${IMGUI_DEFINES})
target_link_libraries(rast_imgui PUBLIC ${IMGUI_LIBRARIES})
target_include_directories(rast_imgui PUBLIC ${IMGUI_INCLUDES})

add_executable(rast ${RAST_SRC})
target_link_libraries(
    rast PRIVATE
    m
    rast_imgui
    SDL3::SDL3
    PkgConfig::glib2
)
