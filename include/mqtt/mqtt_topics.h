
#ifndef MQTT_TOPICS_H  // 头文件保护：防止重复包含导致编译错误
#define MQTT_TOPICS_H
#include "config_parser.h"
#include "config_info.h"
#define BOX_ID  ConfigParser::getInstance().getConfig().boxId  
// 替换const std::string为宏定义（字符串宏需用双引号包裹）
#define GET_REAL_IMAGE_TOPIC                   BOX_ID + "/device/camera/getRealImage"
#define OPERATE_PLC_TOPIC                      BOX_ID + "/device/plc/operate"
#define UPDATE_CONFIG_TOPIC                    BOX_ID + "/device/config/update"
#define GET_SENSOR_DATA_TOPIC                  BOX_ID + "/device/sensor/status"
#define GET_ALL_DEVICE_STATUS_TOPIC            BOX_ID + "/device/status/getall"
#define OPERATE_CAR_TOPIC                      BOX_ID + "/device/carControl"

#define RESULT_GET_REAL_IMAGE_TOPIC            BOX_ID + "/device/camera/getRealImage/result"
#define RESULT_OPERATE_PLC_TOPIC               BOX_ID + "/device/plc/operate/result"
#define RESULT_UPDATE_CONFIG_TOPIC             BOX_ID + "/device/config/update/result"
#define RESULT_GET_SENSOR_DATA_TOPIC           BOX_ID + "/device/sensor/status/result"
#define RESULT_GET_ALL_DEVICE_STATUS_TOPIC     BOX_ID + "/device/status/getall/result"
#define RESULT_OPERATE_CAR_TOPIC               BOX_ID + "/device/carControl/result"

#endif // DEVICE_TOPICS_H