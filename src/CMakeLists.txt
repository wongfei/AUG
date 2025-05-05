# BURN IN HELL CMAKE FIND PACKAGE AND FKN INVENTOR OF THIS CANCER

cmake_minimum_required(VERSION 3.15)

project(AUG VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(AUG_CXX_FLAGS_BASE "/MD /DNDEBUG /fp:fast /W4 /wd4100 /D_CRT_SECURE_NO_WARNINGS")
set(AUG_CXX_FLAGS_DEBUG "${AUG_CXX_FLAGS_BASE} /Od /Ob0 /GF /RTC1 /Gy /Zi")
set(AUG_CXX_FLAGS_RELEASE "${AUG_CXX_FLAGS_BASE} /O2 /Ob2 /Oi /Ot /GF /GS- /Gy")
set(AUG_CXX_FLAGS_RELWITHDEBINFO "${AUG_CXX_FLAGS_RELEASE} /Zi")

set(CMAKE_CXX_FLAGS_DEBUG "${AUG_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${AUG_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${AUG_CXX_FLAGS_RELWITHDEBINFO}")

set(CMAKE_C_FLAGS_DEBUG "${AUG_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "${AUG_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${AUG_CXX_FLAGS_RELWITHDEBINFO}")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF")

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

get_filename_component(AUG_ROOT_DIR "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
message(AUG_ROOT_DIR="${AUG_ROOT_DIR}")

# AUG

file(GLOB AUG_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")
file(GLOB AUG_HEADERS "${PROJECT_SOURCE_DIR}/*.h")

add_executable(AUG WIN32 
	"${AUG_HEADERS}" 
	"${AUG_SOURCES}" 
)

source_group("AUG/App" FILES 
	"${PROJECT_SOURCE_DIR}/main.cpp"
	"${PROJECT_SOURCE_DIR}/AUG.cpp"
	"${PROJECT_SOURCE_DIR}/AUG.h"
	"${PROJECT_SOURCE_DIR}/AUGCore.h"
)

source_group("AUG/Processors" FILES 
	"${PROJECT_SOURCE_DIR}/ImageToText.cpp"
	"${PROJECT_SOURCE_DIR}/ImageToText.h"
	"${PROJECT_SOURCE_DIR}/SpeechToText.cpp"
	"${PROJECT_SOURCE_DIR}/SpeechToText.h"
	"${PROJECT_SOURCE_DIR}/SoundConverter.cpp"
	"${PROJECT_SOURCE_DIR}/SoundConverter.h"
	"${PROJECT_SOURCE_DIR}/Assistant.cpp"
	"${PROJECT_SOURCE_DIR}/Assistant.h"
)

source_group("AUG/Platform/Windows" FILES 
	"${PROJECT_SOURCE_DIR}/HotkeyWindow.cpp"
	"${PROJECT_SOURCE_DIR}/HotkeyWindow.h"
	"${PROJECT_SOURCE_DIR}/CurtainWindow.cpp"
	"${PROJECT_SOURCE_DIR}/CurtainWindow.h"
	"${PROJECT_SOURCE_DIR}/FakeCursor.cpp"
	"${PROJECT_SOURCE_DIR}/FakeCursor.h"
	"${PROJECT_SOURCE_DIR}/ScreenCapture.cpp"
	"${PROJECT_SOURCE_DIR}/ScreenCapture.h"
	"${PROJECT_SOURCE_DIR}/SoundRecorder.cpp"
	"${PROJECT_SOURCE_DIR}/SoundRecorder.h"
)

source_group("AUG/Utils" FILES 
	"${PROJECT_SOURCE_DIR}/log.cpp"
	"${PROJECT_SOURCE_DIR}/log.h"
	"${PROJECT_SOURCE_DIR}/RawArray.h"
	"${PROJECT_SOURCE_DIR}/WaveCore.cpp"
	"${PROJECT_SOURCE_DIR}/WaveCore.h"
	"${PROJECT_SOURCE_DIR}/IniFile.cpp"
	"${PROJECT_SOURCE_DIR}/IniFile.h"
)

source_group("ImGui" FILES 
	"${PROJECT_SOURCE_DIR}/imgui_utils.h"
	"${PROJECT_SOURCE_DIR}/TextEditor.cpp"
	"${PROJECT_SOURCE_DIR}/TextEditor.h"
)

target_compile_definitions(AUG PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(AUG PUBLIC BS_THREAD_POOL_NATIVE_EXTENSIONS)
target_compile_definitions(AUG PUBLIC CURL_STATICLIB)
target_compile_definitions(AUG PUBLIC BOOST_REGEX_NO_LIB BOOST_REGEX_STANDALONE)

target_include_directories(AUG PUBLIC "${PROJECT_SOURCE_DIR}")

# header only deps
target_include_directories(AUG PUBLIC "${AUG_ROOT_DIR}/deps/xxHash")
target_include_directories(AUG PUBLIC "${AUG_ROOT_DIR}/deps/fmtlog")
target_include_directories(AUG PUBLIC "${AUG_ROOT_DIR}/deps/thread-pool/include")
target_include_directories(AUG PUBLIC "${AUG_ROOT_DIR}/deps/rapidjson/include")

# installed deps
target_include_directories(AUG PUBLIC "${CMAKE_PREFIX_PATH}/include")
target_include_directories(AUG PUBLIC "${CMAKE_PREFIX_PATH}/include/fmt")
target_include_directories(AUG PUBLIC "${CMAKE_PREFIX_PATH}/include/SDL3")
target_include_directories(AUG PUBLIC "${CMAKE_PREFIX_PATH}/include/imgui")
target_include_directories(AUG PUBLIC "${CMAKE_PREFIX_PATH}/include/opencv2")

file(GLOB_RECURSE AUG_LIBS "${CMAKE_PREFIX_PATH}/*.lib")
target_link_libraries(AUG PUBLIC "${AUG_LIBS}")
target_link_libraries(AUG PUBLIC "opengl32" "Winmm" "Setupapi" "Cfgmgr32" "Version" "Ws2_32" "Wldap32")
