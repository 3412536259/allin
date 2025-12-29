#include "idevice_manager.h"
#include "device_manager.h"
#include "config_parser.h"
#include "task.h"
#include "JobScheduler.h"
#include "my_mqtt_callback.h"
#include "task_result_publisher.h"
#include "mqtt_service.h"
#include "ai_model_service.h"
#include "ai_recognize.h"
#include <memory>
#include <thread>
#include "WebService.h"
#include "device_status_reporter.h"
#include "mqtt_topics.h"
// const std::string MODELPATH = "/home/ztl/workspace/allin/allin/model/yolov8n3576_i8.rknn";
// const std::string CONFIGPATH = "/home/ztl/workspace/allin/allin/include/common/config/config.json";
const std::string MODELPATH = "/home/ztl/workspace/allin/model/yolov8n_3568_i8.rknn";
const std::string CONFIGPATH = "/home/ztl/workspace/allin/include/common/config/config.json";
int main()
{
    av_log_set_level(AV_LOG_QUIET);
    ConfigParser::getInstance().loadFromFile(CONFIGPATH);
    std::shared_ptr<IDeviceManager> ideviceManager = std::make_shared<DeviceManager>();

    JobScheduler jobscheduler(8,ideviceManager.get());
    MqttCommandDispatcher cmdDispatcher(jobscheduler);  //根据接收的主题来选择调用的处理任务，需要依赖jobscheduler的接口提交任务
    // MqttService mqtt("tcp://10.65.4.132:7883", "edge-box", &cmdDispatcher); //需要依赖cmdDispatcher分发相应任务
    MqttService mqtt("mqtt://broker.emqx.io:1883", "edge-box", &cmdDispatcher); 
    MqttPublisher mqttPublisher(&mqtt);
    HttpPublisher httpPublisher("http://127.0.0.1:8080/report");
    jobscheduler.setMqttPublisher(&mqttPublisher); //依赖publisher的唯一原因是需要将publisher传入Taskcontext供具体task调用
    jobscheduler.setHttpPublisher(&httpPublisher);
    // start HTTP service
    WebService ws(CONFIGPATH, 8081, ideviceManager.get(), &jobscheduler);
    ws.start();
    
    // AI ---------------------------
    auto model = std::make_unique<AIModelService>(MODELPATH);
    AIRecognizer ai(std::move(model),ideviceManager.get(),&httpPublisher);
    ai.start();    
    
    
    //定时上报设备状态启动
    DeviceStatusReporter reporter(ideviceManager.get(),&mqttPublisher);
    DeviceStatusReporter reporter2(ideviceManager.get(),&httpPublisher);

    reporter.startAutoReport(RESULT_GET_ALL_DEVICE_STATUS_TOPIC,15);


    std::cout << "System running..." << std::endl;
    while (true) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

}
