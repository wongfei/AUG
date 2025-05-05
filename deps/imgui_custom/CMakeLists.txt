# BURN IN HELL CMAKE FIND PACKAGE AND FKN INVENTOR OF THIS CANCER

cmake_minimum_required(VERSION 3.15)

project(imgui VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

get_filename_component(DEP_DIR "${PROJECT_SOURCE_DIR}/../imgui/" ABSOLUTE)

set(IMGUI_INCLUDE_DIRS 
	"${DEP_DIR}" 
	"${DEP_DIR}/backends"
	"${CMAKE_PREFIX_PATH}/include"
	"${CMAKE_PREFIX_PATH}/include/SDL3"
)

set(IMGUI_INCLUDE_FILES 
	"${DEP_DIR}/imconfig.h" 
	"${DEP_DIR}/imgui.h" 
	"${DEP_DIR}/imgui_internal.h" 
	"${DEP_DIR}/backends/imgui_impl_sdl3.h"
	"${DEP_DIR}/backends/imgui_impl_opengl3.h"
)

set(IMGUI_SOURCES 
	"${DEP_DIR}/imgui.cpp"
	"${DEP_DIR}/imgui_draw.cpp"
	"${DEP_DIR}/imgui_tables.cpp"
	"${DEP_DIR}/imgui_widgets.cpp"
	"${DEP_DIR}/imgui_demo.cpp"
	"${DEP_DIR}/backends/imgui_impl_sdl3.cpp"
	"${DEP_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui STATIC "${IMGUI_SOURCES}")
target_include_directories(imgui PUBLIC "${IMGUI_INCLUDE_DIRS}")

install(FILES ${IMGUI_INCLUDE_FILES} DESTINATION "${CMAKE_PREFIX_PATH}/include/imgui")
install(TARGETS imgui ARCHIVE DESTINATION "${CMAKE_PREFIX_PATH}/lib")
