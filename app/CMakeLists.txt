cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_SYSTEM_NAME  Android)

set(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_BUILD_TYPE Debug)
#SET(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} -Ofast -funwind-tables -Wl,--no-merge-exidx-entries")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Ofast -funwind-tables -Wl,--no-merge-exidx-entries")

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -Ofast -funwind-tables -Wl,--no-merge-exidx-entries -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -funwind-tables -Wl,--no-merge-exidx-entries -std=c++11")

set(PREBUILT_DIR ${CMAKE_SOURCE_DIR}/../prebuilt)

add_library(engine SHARED
    src/main/cpp/EngineJNI.cpp
    src/main/cpp/Engine.cpp
    src/main/cpp/exceptionUtils.cpp
    src/main/c/imageUtils.c
    src/main/c/coffeecatch.c
    src/main/cpp/coffeejni.cpp
    src/main/cpp/FFmpegUtils.cpp
    src/main/c/generalUtils.c
    src/main/cpp/Encoder.cpp
    src/main/cpp/AsyncIO.cpp
    src/main/cpp/MotionDetector.cpp
    src/main/c/thpool.c)

target_include_directories(engine PRIVATE
                           ${PREBUILT_DIR}/include
                           ./src/main/cpp
                           ./src/main/c)

target_link_libraries(engine
                      mediandk
                      log
                      z
                      ${PREBUILT_DIR}/lib/libswscale.a
                      ${PREBUILT_DIR}/lib/libavfilter.a
                      ${PREBUILT_DIR}/lib/libavformat.a
                      ${PREBUILT_DIR}/lib/libavcodec.a
                      ${PREBUILT_DIR}/lib/libavutil.a
                      ${PREBUILT_DIR}/lib/libfreetype.a
                      ${PREBUILT_DIR}/lib/libopencv_optflow.a
                      ${PREBUILT_DIR}/lib/libopencv_highgui.a
                      ${PREBUILT_DIR}/lib/libopencv_imgcodecs.a
                      ${PREBUILT_DIR}/lib/libopencv_video.a
                      ${PREBUILT_DIR}/lib/libopencv_ximgproc.a
                      ${PREBUILT_DIR}/lib/libopencv_xphoto.a
                      ${PREBUILT_DIR}/lib/libopencv_bgsegm.a
                      ${PREBUILT_DIR}/lib/libopencv_calib3d.a
                      ${PREBUILT_DIR}/lib/libopencv_features2d.a
                      ${PREBUILT_DIR}/lib/libopencv_flann.a
                      ${PREBUILT_DIR}/lib/libopencv_hfs.a
                      ${PREBUILT_DIR}/lib/libopencv_imgproc.a
                      ${PREBUILT_DIR}/lib/libopencv_core.a
                      ${PREBUILT_DIR}/lib/libtegra_hal.a
                      ${PREBUILT_DIR}/lib/libcpufeatures.a
                      ${PREBUILT_DIR}/lib/liblibprotobuf.a
                      ${PREBUILT_DIR}/lib/libx264.a
                      ${PREBUILT_DIR}/lib/libopenh264.a
                      )