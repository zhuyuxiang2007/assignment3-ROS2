#ifndef HIKROBOT_CAMERA_CAMERA_NODE_HPP_
#define HIKROBOT_CAMERA_CAMERA_NODE_HPP_

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp" // 参数配置必备
#include <opencv2/opencv.hpp> // OpenCV 用于处理图像
#include <cv_bridge/cv_bridge.h> // 用于转换图像格式
#include "MvCameraControl.h" // 海康 SDK 头文件

namespace hikrobot_camera
{

class CameraNode : public rclcpp::Node
{
public:
    explicit CameraNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
    ~CameraNode(); // 👈 析构函数：用于程序退出时断开相机连接、释放资源

private:
    void* handle_; // 海康相机句柄
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_; // 图像发布者
    rclcpp::TimerBase::SharedPtr timer_; // 定时器
    void timer_callback(); // 定时器回调函数

    // ===== 新增：断线重连和参数相关 =====
    bool is_camera_connected_ = false; // 标记相机是否连接
    int fail_count_ = 0;               // 连续抓图失败次数
    double current_exposure_ = 1000.0; // 记住当前的曝光值
    double current_gain_ = 0.0;        // 记住当前的增益值

    void disconnectCamera();           // 断开连接辅助函数
    bool connectCamera();              // 连接辅助函数
    void restoreParameters();          // 断线重连后恢复参数
















    // 参数动态配置相关 (任务书要求)
    rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_callback_handle_;
    rcl_interfaces::msg::SetParametersResult parameters_callback(const std::vector<rclcpp::Parameter> &parameters);
};

} // namespace hikrobot_camera

#endif // HIKROBOT_CAMERA_CAMERA_NODE_HPP_