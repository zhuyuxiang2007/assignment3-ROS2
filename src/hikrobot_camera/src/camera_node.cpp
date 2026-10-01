#include "hikrobot_camera/camera_node.hpp"
#include "MvCameraControl.h"
#include <iostream>
#include "rclcpp/rclcpp.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"



double current_exposure_ = 50000.0;
double current_gain_ = 0.0;




namespace hikrobot_camera
{

CameraNode::CameraNode(const rclcpp::NodeOptions & options)
: Node("hikrobot_camera", options)
{
this->declare_parameter("exposure_time", 10000.0);
this->declare_parameter("gain", 0.0);
this->declare_parameter("frame_rate", 30.0);


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
    // 关闭触发模式，设置为连续采集
    MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
    
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
 // ================= 1. 断线重连逻辑 =================
if (handle_ == nullptr) {
    RCLCPP_WARN(this->get_logger(), "相机未连接，正在尝试重连...");
    
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    
    if (MV_OK == nRet && stDeviceList.nDeviceNum > 0) {
        if (MV_OK == MV_CC_CreateHandle(&handle_, stDeviceList.pDeviceInfo[0])) {
            if (MV_OK == MV_CC_OpenDevice(handle_)) {
                MV_CC_StartGrabbing(handle_);
                
                // 1. 极其重要：重连后，相机可能会恢复自动曝光，必须先强制关闭自动模式！
                MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
                MV_CC_SetEnumValue(handle_, "GainAuto", 0);

                // 2. 恢复参数，并且打印结果，看看有没有设置成功！
                int retExp = MV_CC_SetFloatValue(handle_, "ExposureTime", current_exposure_);
                int retGain = MV_CC_SetFloatValue(handle_, "Gain", current_gain_);
                
                if (retExp != MV_OK || retGain != MV_OK) {
                    RCLCPP_ERROR(this->get_logger(), "恢复参数失败! Exp:0x%x, Gain:0x%x", retExp, retGain);
                } else {
                    RCLCPP_INFO(this->get_logger(), "恢复参数成功: 曝光=%.1f, 增益=%.1f", current_exposure_, current_gain_);
                }
                RCLCPP_INFO(this->get_logger(), "相机重连成功!");
            } else {
                handle_ = nullptr;
            }
        } else {
            handle_ = nullptr;
        }
    } else {
        handle_ = nullptr;
    }
    
    // 【关键修复】：重连逻辑执行完毕后，必须直接 return 退出本次回调，绝不能往下走！
    return; 
} // <--- 这里必须闭合 if (handle_ == nullptr)！

// ================= 2. 图像抓取与发布逻辑 =================
// 注意：以下代码必须放在 if (handle_ == nullptr) 的外面！正常情况才会执行到这里！
    // ================= 2. 图像抓取与发布逻辑 =================
    // 注意：以下代码必须放在 if (handle_ == nullptr) 的外面！
    MV_FRAME_OUT stImageInfo = {0};
    memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT));

    // 真正去获取一帧图像
    int nRet = MV_CC_GetImageBuffer(handle_, &stImageInfo, 1000);
    //RCLCPP_INFO(this->get_logger(), "当前相机像素格式: 0x%lx", stImageInfo.stFrameInfo.enPixelType);
    
    if (nRet != MV_OK) {
    RCLCPP_WARN(this->get_logger(), "获取图像失败, 错误码: 0x%x", nRet);
    
    // 【关键修复】：如果是掉线，必须主动断开旧连接，释放句柄，
    // 否则下一次没法进入 if (handle_ == nullptr) 的重连逻辑。
    // 加 try/catch 或者判断一下，保证即使已经掉线，调用清理函数也不会崩溃
    if (handle_ != nullptr) {
        MV_CC_StopGrabbing(handle_);
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        RCLCPP_WARN(this->get_logger(), "相机已掉线，已清理句柄，等待重连...");
    }
    return;
}

// 1. 安全校验：指针为空则跳过
// 1. 安全校验：指针为空则跳过
if (stImageInfo.pBufAddr == nullptr || stImageInfo.stFrameInfo.nFrameLen == 0) {
    RCLCPP_WARN(this->get_logger(), "图像指针为空或数据无效，跳过本次发布");
    MV_CC_FreeImageBuffer(handle_, &stImageInfo);
    return;
}

// 2. 获取真实的宽、高
int width = stImageInfo.stFrameInfo.nWidth;
int height = stImageInfo.stFrameInfo.nHeight;

// 3. 使用 OpenCV 将海康的真实格式 YUV422 转换为标准的 BGR！
// 根据之前的十六进制报错 (nFrameLen = 宽*高*2)，相机是每像素2字节的 YUV422
cv::Mat yuv_img(height, width, CV_8UC2, stImageInfo.pBufAddr);
cv::Mat bgr_img;
cv::cvtColor(yuv_img, bgr_img, cv::COLOR_YUV2BGR_YUYV);

// 4. 手动构建 ROS 2 消息 (完全避免 cv_bridge 导致的崩溃)
auto msg = std::make_shared<sensor_msgs::msg::Image>();
msg->header.stamp = this->now();
msg->height = height;
msg->width = width;
msg->encoding = "bgr8"; 
msg->is_bigendian = false;

// 5. 严格按照 ROS 2 规范填充 step 和 data，保证数学上完美匹配！
msg->step = width * 3; // BGR8 是每像素3字节
size_t data_size = msg->step * height; // 这里的 data_size 绝对等于 msg->step * height
msg->data.resize(data_size);

// 6. 拷贝转换后的数据并发布
memcpy(&msg->data[0], bgr_img.data, data_size);
image_pub_->publish(*msg);

// 7. 释放缓冲区
MV_CC_FreeImageBuffer(handle_, &stImageInfo);
} // <<<< 这是 timer_callback 函数的结束括号（你截图里第170行那个紫色括号）



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

                current_exposure_=exposure;
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
            current_gain_=gain;
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
