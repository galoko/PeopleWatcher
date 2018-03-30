cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_SYSTEM_NAME  Android)

set(CMAKE_SYSTEM_VERSION 1)

set(TOOLCHAIN $ENV{HOME}/Components/Android/toolchain)

SET(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_C_COMPILER ${TOOLCHAIN}/bin/arm-linux-androideabi-clang)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} -Ofast -funwind-tables -Wl,--no-merge-exidx-entries")

set(CMAKE_CXX_COMPILER ${TOOLCHAIN}/bin/arm-linux-androideabi-clang++)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -Ofast -funwind-tables -Wl,--no-merge-exidx-entries -std=c++11")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -std=c++11")

set(PREBUILT_DIR ${CMAKE_SOURCE_DIR}/../prebuilt)

add_library(engine SHARED
    src/main/cpp/EngineJNI.cpp
    src/main/cpp/Engine.cpp
    src/main/cpp/ffmpegUtils.cpp
    src/main/cpp/exceptionUtils.cpp
    src/main/cpp/imageUtils.cpp
    src/main/c/coffeecatch.c
    src/main/c/coffeejni.c)

target_include_directories(engine PRIVATE
                           ${PREBUILT_DIR}/include
                           ./src/main/cpp
                           ./src/main/c)

target_link_libraries(engine
    ${PREBUILT_DIR}/lib/libswscale.a
    ${PREBUILT_DIR}/lib/libavutil.a
    ${PREBUILT_DIR}/lib/libavcodec.a
    ${PREBUILT_DIR}/lib/libavfilter.a
    ${PREBUILT_DIR}/lib/libavformat.a
    ${PREBUILT_DIR}/lib/libcpufeatures.a
    ${PREBUILT_DIR}/lib/libfreetype.a
    ${PREBUILT_DIR}/lib/liblibprotobuf.a
    ${PREBUILT_DIR}/lib/libopencv_bgsegm.a
    ${PREBUILT_DIR}/lib/libopencv_calib3d.a
    ${PREBUILT_DIR}/lib/libopencv_core.a
    ${PREBUILT_DIR}/lib/libopencv_features2d.a
    ${PREBUILT_DIR}/lib/libopencv_flann.a
    ${PREBUILT_DIR}/lib/libopencv_hfs.a
    ${PREBUILT_DIR}/lib/libopencv_highgui.a
    ${PREBUILT_DIR}/lib/libopencv_imgcodecs.a
    ${PREBUILT_DIR}/lib/libopencv_imgproc.a
    ${PREBUILT_DIR}/lib/libopencv_optflow.a
    ${PREBUILT_DIR}/lib/libopencv_video.a
    ${PREBUILT_DIR}/lib/libopencv_ximgproc.a
    ${PREBUILT_DIR}/lib/libopencv_xphoto.a
    ${PREBUILT_DIR}/lib/libtegra_hal.a
    ${PREBUILT_DIR}/lib/libx264.a
    )

target_link_libraries(engine
                      log)