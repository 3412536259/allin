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
    testDemo/taskTest.cpp
    device/device_manager.cpp
    plc/serial_plc_connector.cpp
    plc/gateway_tcp_connector.cpp
    plc/plc_manager.cpp

    sensor/custom_sensor.cpp
    sensor/gpio_sensor.cpp
    sensor/modbus_sensor.cpp
    sensor/sensor_manager.cpp

    camera/camera_manager.cpp
    camera/camera.cpp
    camera/dao/video_capture.cpp
    camera/dao/video_storage.cpp
    ../include/common/config/config_parser.cpp
    common/logger/logger.cpp
    common/base64/base64.cpp
    common/plc_utils/plc_common_utils.cpp
    common/yolo8/postprocess.cc
    common/yolo8/yolov8.cc
    common/imageprocess/image_processor.cpp
    common/timestamp/SegmentManager.cpp
    common/FileUtils/FileUtils.cpp
    task/JobScheduler.cpp
    task/task.cpp
    mqtt/mqtt_command_dispatcher.cpp
    mqtt/mqtt_service.cpp
    http/WebService.cpp
    http/WebController.cpp
    ai/ai_recognize.cpp
    ai/ai_model_service.cpp
    carControl/car_control.cpp
    carControl/car_control_manager.cpp
)

# -------------------------- 2. 生成主程序可执行文件 --------------------------
add_executable(receive ${SRC_FILES})  # 可执行文件名：ai_recognition
target_compile_definitions(receive PUBLIC
    DISABLE_RGA
)
# -------------------------- 3. 配置头文件路径 --------------------------
target_include_directories(receive PUBLIC
    ${CMAKE_SOURCE_DIR}/include/device
    ${CMAKE_SOURCE_DIR}/include/plc
    ${CMAKE_SOURCE_DIR}/include/sensor
    ${CMAKE_SOURCE_DIR}/include/camera
    ${CMAKE_SOURCE_DIR}/include/task
    ${CMAKE_SOURCE_DIR}/include/mqtt
    ${CMAKE_SOURCE_DIR}/include/http
    ${CMAKE_SOURCE_DIR}/include/camera/dao
    ${CMAKE_SOURCE_DIR}/include/ai
    ${CMAKE_SOURCE_DIR}/include/carControl
    ${CMAKE_SOURCE_DIR}/include/common/FileUtils
    ${CMAKE_SOURCE_DIR}/include/common/timestamp
    ${CMAKE_SOURCE_DIR}/include/common/base64
    ${CMAKE_SOURCE_DIR}/include/common/imageprocess
    ${CMAKE_SOURCE_DIR}/include/common/logger
    ${CMAKE_SOURCE_DIR}/include/common/config
    ${CMAKE_SOURCE_DIR}/include/common/threads
    ${CMAKE_SOURCE_DIR}/include/common/yolo8
    ${CMAKE_SOURCE_DIR}/include/common/plc_utils
    ${CMAKE_SOURCE_DIR}/third_party/mqtt

    ${CMAKE_SOURCE_DIR}/third_party/json
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
    ${STB_INCLUDES}
    ${LIBJPEG_INCLUDES}
    ${LIBRKNNRT_INCLUDES}
    ${LIBRGA_INCLUDES}
    ${LIBSNDFILE_INCLUDES}
    ${CMAKE_SOURCE_DIR}/rknn/utils
    ${OpenCV_INCLUDE_DIRS}
)
# link_directories(
#     ${PROJECT_SOURCE_DIR}/lib  # 告诉编译器从项目 lib 目录找库
# )

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
# -------------------------- 4. 链接依赖库 --------------------------
target_link_libraries(receive PUBLIC
    fileutils
    imagedrawing
    imageutils
    audioutils
    ${LIBJPEG}
    ${LIBRKNNRT}
    ${LIBRGA}
    ${LIBSNDFILE}
    ${LIBFFTW}
    ${OpenCV_LIBS}
    # FFmpeg 库
    avformat
    avcodec
    swscale
    avutil
    
    #mqtt库
    paho-mqttpp3
    paho-mqtt3as

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