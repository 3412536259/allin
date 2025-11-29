cmake_minimum_required(VERSION 3.15)
project(src)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MQTT_LIBS
    paho-mqttpp3  # C++ 库（libpaho-mqttpp3.so）
    paho-mqtt3as  # C 库（带 SSL 异步，libpaho-mqtt3as.so）
)



# -------------------------- 1. 收集src目录源文件 --------------------------
set(SRC_FILES
    testDemo/cameraTest.cpp
    camera/camera_manager.cpp
)

# -------------------------- 2. 生成主程序可执行文件 --------------------------
add_executable(receive ${SRC_FILES})  # 可执行文件名：ai_recognition

# -------------------------- 3. 配置头文件路径 --------------------------
target_include_directories(receive PUBLIC
    ${CMAKE_SOURCE_DIR}/include/camera
    # ${CMAKE_SOURCE_DIR}/include/dao
    # ${CMAKE_SOURCE_DIR}/include/service
    # ${CMAKE_SOURCE_DIR}/include/infrastructure
    # ${CMAKE_SOURCE_DIR}/include/third_party
    # ${PROJECT_SOURCE_DIR}/include/third_party/mqtt
    # ${PROJECT_SOURCE_DIR}/utils
    # ${PROJECT_SOURCE_DIR}/entity
    # ${PROJECT_SOURCE_DIR}/utils/yolo8
    # ${PROJECT_SOURCE_DIR}/utils/Imageprocess
    # ${PROJECT_SOURCE_DIR}/utils/config
    # ${PROJECT_SOURCE_DIR}/utils/base64
    # ${PROJECT_SOURCE_DIR}/utils/Frameprocess
    # ${PROJECT_SOURCE_DIR}/utils/FrameUtils
    # ${STB_INCLUDES}
    # ${LIBJPEG_INCLUDES}
    # ${LIBRKNNRT_INCLUDES}
    # ${LIBRGA_INCLUDES}
    # ${LIBSNDFILE_INCLUDES}
    # ${CMAKE_SOURCE_DIR}/rknn/utils
    # ${OpenCV_INCLUDE_DIRS}
)
# link_directories(
#     ${PROJECT_SOURCE_DIR}/lib  # 告诉编译器从项目 lib 目录找库
# )

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
# -------------------------- 4. 链接依赖库 --------------------------
target_link_libraries(receive PUBLIC
    # fileutils
    # imagedrawing
    # imageutils
    # audioutils
    ${LIBJPEG}
    ${LIBRKNNRT}
    ${LIBRGA}
    ${LIBSNDFILE}
    ${LIBFFTW}
    ${OpenCV_LIBS}
    # FFmpeg 库
    avutil
    swscale
    avcodec
    avformat
    
    #mqtt库
    # paho-mqttpp3
    # paho-mqtt3as

    OpenSSL::Crypto
    Threads::Threads


    # 系统库
    dl
    pthread
    m
)

# set_target_properties(main PROPERTIES
#     INSTALL_RPATH "${PROJECT_SOURCE_DIR}/lib"
#     BUILD_WITH_INSTALL_RPATH ON
# )
# -------------------------- 5. 可选：安装主程序 --------------------------
install(TARGETS receive DESTINATION bin)  # 安装到install/bin目录