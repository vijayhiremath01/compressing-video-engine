cmake_minimum_required(VERSION 3.16)
project(video_compressor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

add_executable(video_compressor src/main.cpp)
add_executable(version_test src/version_test.cpp)
add_executable(decoder src/decoder.cpp)
add_executable(encoder src/encoder.cpp)
add_executable(compressor src/compressor.cpp)

target_include_directories(video_compressor PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(video_compressor PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(video_compressor PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(video_compressor PRIVATE ${FFMPEG_CFLAGS_OTHER})

target_include_directories(version_test PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(version_test PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(version_test PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(version_test PRIVATE ${FFMPEG_CFLAGS_OTHER})

target_include_directories(decoder PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(decoder PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(decoder PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(decoder PRIVATE ${FFMPEG_CFLAGS_OTHER})

target_include_directories(encoder PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(encoder PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(encoder PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(encoder PRIVATE ${FFMPEG_CFLAGS_OTHER})

target_include_directories(compressor PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(compressor PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(compressor PRIVATE ${FFMPEG_LIBRARIES})
target_compile_options(compressor PRIVATE ${FFMPEG_CFLAGS_OTHER})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})