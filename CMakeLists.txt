cmake_minimum_required(VERSION 4.0)
project(ocrit)

if (NOT WIN32 OR (NOT MSVC AND NOT MINGW))
    message(FATAL_ERROR "Not support: ${CMAKE_SYSTEM_NAME} ${CMAKE_CXX_COMPILER_ID}")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MINGW)
    set(CMAKE_CXX_EXTENSIONS ON)
    add_compile_options(-finput-charset=utf-8 -fexec-charset=utf-8)
    add_link_options(-mwindows -municode)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
                -O2
                -DNDEBUG
                -ftree-vectorize
                -ffunction-sections
                -fdata-sections
                -fvisibility=hidden
                $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>
        )
        add_link_options(
                "LINKER:-O2"
                "LINKER:--as-needed"
                "LINKER:--strip-all"
                "LINKER:--gc-sections"
                "LINKER:--no-insert-timestamp"
        )
    endif ()
endif ()

if (MSVC)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    add_compile_options(/utf-8 /EHsc)
    add_link_options(/SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
                /O2
                /GL
                /GS-
                /DNDEBUG
        )
        add_link_options(
                /BREPRO
                /NOCOFFGRPINFO
                /EMITTOOLVERSIONINFO:NO
                /DEBUG:NONE
                /PDB:NONE
                /LTCG
                /OPT:REF,ICF
                /INCREMENTAL:NO
                /DYNAMICBASE:NO
        )
    endif ()
endif ()

set(OpenCV_STATIC ON)
find_package(OpenCV 4.13.0 CONFIG REQUIRED)

file(GLOB_RECURSE OCRIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${OCRIT_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/onnxruntime/include")
target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/onnxruntime/lib")

if (MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
endif ()
target_link_libraries(${PROJECT_NAME} PRIVATE msimg32 ${OpenCV_LIBS} onnxruntime)

if(MSVC)
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
elseif(MINGW)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/app.rc "1 RT_MANIFEST \"${CMAKE_CURRENT_SOURCE_DIR}/app.manifest\"")
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()