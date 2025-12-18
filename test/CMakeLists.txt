cmake_minimum_required(VERSION 3.15)
project(mqtt_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找MQTT库
find_package(Threads REQUIRED)

# 添加可执行文件
add_executable(mqtt_test ztl.cpp)

# 包含头文件目录
target_include_directories(mqtt_test PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/mqtt
    ${CMAKE_SOURCE_DIR}/third_party/json
)

# 链接依赖库
target_link_libraries(mqtt_test PUBLIC
    paho-mqttpp3
    paho-mqtt3as
    Threads::Threads
    dl
    m
)