#include "hikrobot_camera/camera_node.hpp"
#include "MvCameraControl.h"
#include <iostream>
#include "rclcpp/rclcpp.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"








namespace hikrobot_camera
{

CameraNode::CameraNode(const rclcpp::NodeOptions & options)
: Node("hikrobot_camera", options)
{
  


// 1. 初始化 SDK
int nRet = MV_CC_Initialize();
if (MV_OK != nRet) {
    RCLCPP_ERROR(this->get_logger(), "初始化 SDK 失败! 错误码: 0x%x", nRet);
    return;
}

// 2. 枚举设备
MV_CC_DEVICE_INFO_LIST stDeviceList;
memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
if (MV_OK != nRet || stDeviceList.nDeviceNum == 0) {
    RCLCPP_ERROR(this->get_logger(), "未找到相机设备!");
    return;
}
RCLCPP_INFO(this->get_logger(), "找到 %d 个设备，尝试连接第 1 个...", stDeviceList.nDeviceNum);

// 3. 创建句柄
nRet = MV_CC_CreateHandle(&handle_, stDeviceList.pDeviceInfo[0]);
if (MV_OK != nRet) {
    RCLCPP_ERROR(this->get_logger(), "创建句柄失败!");
    return;
}

// 注册参数回调，当你在终端修改参数时，会触发 parameters_callback 函数
param_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&CameraNode::parameters_callback, this, std::placeholders::_1));




// 4. 打开设备
nRet = MV_CC_OpenDevice(handle_);
if (MV_OK == nRet) {
    RCLCPP_INFO(this->get_logger(), "相机连接成功！");
    
    
        // 1. 创建图像发布者
    image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("image_raw", 10);
    
    // 2. 开启取流
    int nRetGrab = MV_CC_StartGrabbing(handle_);
    if (MV_OK == nRetGrab) {
        RCLCPP_INFO(this->get_logger(), "开始取流成功！");
        // 3. 创建定时器，30毫秒触发一次
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(30),
            std::bind(&CameraNode::timer_callback, this));
    }
    
    
    
    
    
    
    
    
    
    
     







} else {
    RCLCPP_ERROR(this->get_logger(), "打开相机失败!");
}





}




CameraNode::~CameraNode()
{
    RCLCPP_INFO(this->get_logger(), "节点退出，正在断开相机连接...");
    if (handle_ != nullptr) {
        MV_CC_StopGrabbing(handle_);
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
    }
}


void CameraNode::timer_callback()
{
    // 👇 新增：如果没连上，直接调用 reconnect 逻辑尝试重连
    if (handle_ == nullptr) {
        RCLCPP_WARN(this->get_logger(), "相机未连接，正在尝试重连...");
        // 重新调用连接逻辑
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK == nRet && stDeviceList.nDeviceNum > 0) {
            if (MV_OK == MV_CC_CreateHandle(&handle_, stDeviceList.pDeviceInfo[0])) {
                if (MV_OK == MV_CC_OpenDevice(handle_)) {
                    MV_CC_StartGrabbing(handle_);
                    // 重连后恢复参数
                    MV_CC_SetFloatValue(handle_, "ExposureTime", current_exposure_);
                    MV_CC_SetFloatValue(handle_, "Gain", current_gain_);
                    RCLCPP_INFO(this->get_logger(), "相机重连成功！");
                } else {
                    handle_ = nullptr; // 重连失败，清空句柄
                }
            }
        }
        return; // 不管重连成没成功，本次回调先退出
    
// --- 下面保留你原本的图像抓取和发布代码 ---
MV_FRAME_OUT stImageInfo = {0};
memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT));


    if (MV_OK == nRet)
    {
        // 将海康的原始数据转为 OpenCV 图像 (假设是 BGR8 格式)
        cv::Mat img(stImageInfo.stFrameInfo.nHeight, 
                    stImageInfo.stFrameInfo.nWidth, 
                    CV_8UC3, 
                    stImageInfo.pBufAddr);

        // 转换为 ROS 2 的 Image 消息并发布
        std_msgs::msg::Header header;
        header.stamp = this->now();
        auto msg = cv_bridge::CvImage(header, "bgr8", img).toImageMsg();
        image_pub_->publish(*msg);

        // 释放缓冲区
        MV_CC_FreeImageBuffer(handle_, &stImageInfo);
    }

      // 你没截到的部分：在 if (MV_OK == nRet) 的 else 分支里
else
{
    RCLCPP_WARN(this->get_logger(), "抓图失败，相机可能已断线！");
    // 核心动作：直接把 handle_ 清空，让下次回调触发重连
    handle_ = nullptr; 
}



}
}


rcl_interfaces::msg::SetParametersResult CameraNode::parameters_callback(
    const std::vector<rclcpp::Parameter> &parameters)
{
    

    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    result.reason = "success";

    for (const auto & param : parameters) {
        // ===== 1. 曝光时间 (Exposure Time) =====
        if (param.get_name() == "exposure_time") {
            double exposure = param.as_double();
            
            // 验收标准：校验范围
            if (exposure < 0.0 || exposure > 1000000.0) {
                result.successful = false;
                result.reason = "曝光时间超出有效范围 (0 - 1000000 us)";
                return result;
            }

            if (handle_ != nullptr) {
                MV_CC_SetEnumValue(handle_, "ExposureAuto", 0); // 关闭自动曝光
                int nRet = MV_CC_SetFloatValue(handle_, "ExposureTime", exposure);
                if (MV_OK != nRet) {
                    result.successful = false;
                    result.reason = "SDK设置曝光失败，错误码: " + std::to_string(nRet);
                    return result;
                }
            }
            RCLCPP_INFO(this->get_logger(), "动态设置曝光: %f", exposure);
        }
        // ===== 2. 增益 (Gain) =====
        else if (param.get_name() == "gain") {
            double gain = param.as_double();
            
            if (gain < 0.0 || gain > 100.0) {
                result.successful = false;
                result.reason = "增益超出有效范围 (0 - 100 dB)";
                return result;
            }

            if (handle_ != nullptr) {
                MV_CC_SetEnumValue(handle_, "GainAuto", 0); // 关闭自动增益
                int nRet = MV_CC_SetFloatValue(handle_, "Gain", gain);
                if (MV_OK != nRet) {
                    result.successful = false;
                    result.reason = "SDK设置增益失败，错误码: " + std::to_string(nRet);
                    return result;
                }
            }
            RCLCPP_INFO(this->get_logger(), "动态设置增益: %f", gain);
        }
        // ===== 3. 帧率 (Frame Rate) =====
        else if (param.get_name() == "frame_rate") {
            double frame_rate = param.as_double();
            
            if (frame_rate < 1.0 || frame_rate > 1000.0) {
                result.successful = false;
                result.reason = "帧率超出有效范围 (1 - 1000 FPS)";
                return result;
            }

            if (handle_ != nullptr) {
                int nRet = MV_CC_SetBoolValue(handle_, "AcquisitionFrameRateEnable", true);
                if (MV_OK == nRet) {
                    nRet = MV_CC_SetFloatValue(handle_, "AcquisitionFrameRate", frame_rate);
                }
                if (MV_OK != nRet) {
                    result.successful = false;
                    result.reason = "SDK设置帧率失败，错误码: " + std::to_string(nRet);
                    return result;
                }
            }
            RCLCPP_INFO(this->get_logger(), "动态设置帧率: %f", frame_rate);
        }
        // ===== 4. 图像格式 (Pixel Format) =====
        else if (param.get_name() == "pixel_format") {
            int format = param.as_int();
            if (handle_ != nullptr) {
                MV_CC_StopGrabbing(handle_);
                int nRet = MV_CC_SetEnumValue(handle_, "PixelFormat", format);
                MV_CC_StartGrabbing(handle_);
                
                if (MV_OK != nRet) {
                    result.successful = false;
                    result.reason = "SDK设置图像格式失败，错误码: " + std::to_string(nRet);
                    return result;
                }
            }
            RCLCPP_INFO(this->get_logger(), "动态设置图像格式: %d", format);
        }
    }
    return result;

}
















}  // namespace hikrobot_camera
