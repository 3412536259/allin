cmake_minimum_required(VERSION 3.15)
project(hzj_project)

# 全局配置：C++标准与目标SOC（RK3576）
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TARGET_SOC "rk3576" CACHE STRING "Target SOC for RKNN")
set(CMAKE_SYSTEM_NAME "Linux" CACHE STRING "Target system")

find_package(OpenCV REQUIRED COMPONENTS 
    core 
    imgproc 
    imgcodecs 
    videoio  # 包含 VideoCapture
    highgui  # 包含 imshow
)
if(OpenCV_FOUND)
    message(STATUS "找到系统 OpenCV: ${OpenCV_VERSION}")
    message(STATUS "OpenCV 头文件: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV 库: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "❌ 未找到系统 OpenCV")
endif()

# add_subdirectory(rknn/3rdparty)
# add_subdirectory(rknn/utils)
add_subdirectory(src)

# 可选：安装配置
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Install path")
install(PROGRAMS ${LIBRKNNRT} ${RGA_PATH}/Linux/${TARGET_LIB_ARCH}/librga.so DESTINATION lib)