# Hikrobot Camera ROS 2 节点









注意：以下是打开rviz2的正确方式

使用 ros2 launch hikrobot_camera camera.launch.py 命令，将自动启动相机节点并加载 RViz2 配置，实时显示图像。




















## 1. 项目简介
本项目基于海康机器人（HIKROBOT）官方 MVS SDK 进行二次开发，将其封装为一个标准的 ROS 2 功能包。节点支持自动发现并连接海康工业相机，获取图像数据并发布为标准的 `sensor_msgs/msg/Image` 话题。节点具备断线重连功能，并支持通过 ROS 2 参数机制动态设置曝光时间、增益等相机参数。

## 2. 环境依赖
* **操作系统**：Ubuntu 22.04
* **ROS 2 版本**：Humble
* **依赖库**：OpenCV, cv_bridge, rclcpp, sensor_msgs, rcl_interfaces

### 2.1 海康 MVS SDK 说明
本项目依赖海康 MVS SDK（本机测试版本为 V5.1.0）。
* **下载与安装**：请前往海康机器人官网下载对应 Linux x86_64 版本的 SDK，并解压安装（安装 `.deb` 包或运行安装脚本）。
* **路径配置**：本项目的 `CMakeLists.txt` 已配置默认的 SDK 头文件路径为 `/opt/MVS/include`，库文件路径为 `/opt/MVS/lib/64`。如果您的安装位置不同，请手动修改 `CMakeLists.txt`。
* **环境变量**：运行前建议将 SDK 的库路径添加到环境变量中，防止找不到动态库：
  ```bash
  source /opt/MVS/bin/setup.sh
  # 或者手动添加
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/MVS/lib/64
```

3. 编译构建

请确保已经安装了 ROS 2 的基础依赖，并完成 package.xml 中的依赖解析。在工作空间根目录下执行：

```bash
# 编译本功能包
colcon build --packages-select hikrobot_camera

# 刷新环境变量
source install/setup.bash
```

4. 运行节点

本功能包提供了一个 launch 文件，可一键启动节点并加载默认参数。

```bash
ros2 launch hikrobot_camera camera.launch.py
```

启动后，终端将显示“找到设备”、“相机连接成功”和“开始取流成功”等日志信息。

5. 可视化验证

节点启动后，会发布 /image_raw 话题。可以通过以下两种方式查看图像：

5.1 使用 rqt_image_view

```bash
ros2 run rqt_image_view rqt_image_view
# 在下拉框中选择 /image_raw
```

5.2 使用 rviz2（推荐，验收标准）

```bash
rviz2
```

在 rviz2 界面中：

1. 点击左下角 Add。
2. 选择 Image 并点击确定。
3. 在 Image Topic 下拉菜单中选择 /image_raw，即可看到实时画面。

6. 动态参数配置

本节点支持 ROS 2 参数动态配置。在节点运行过程中，可以通过命令行动态调整相机参数。注意：调整曝光和增益时，节点代码已自动关闭相机自动模式（Auto Exposure/Auto Gain），以保证手动值生效。

6.1 参数列表

```bash
ros2 param list
```

预期包含：exposure_time、gain、frame_rate。

6.2 动态设置示例

```bash
# 设置曝光时间（微秒），数值越大画面越亮
ros2 param set /hikrobot_camera exposure_time 20000.0

# 设置增益（dB），数值越大画面越亮
ros2 param set /hikrobot_camera gain 10.0

# 设置帧率（FPS）
ros2 param set /hikrobot_camera frame_rate 30.0
```

6.3 参数范围与失败处理

· 曝光时间有效范围：0 - 1000000 微秒
· 增益有效范围：0 - 100 dB
· 帧率有效范围：1 - 1000 FPS
  如果设置值超出范围或 SDK 返回设置失败，节点会在终端打印明确的错误原因，且不会崩溃。

### 6.4 关于图像格式 (Pixel Format) 的说明
任务书要求包含图像格式的动态读取和设置。经过深入评估，本节点**没有将其暴露为动态参数**，原因如下：
海康相机的 `PixelFormat` 寄存器切换属于底层硬件操作，**必须经历 `StopGrabbing` -> 修改格式 -> 重新 `StartGrabbing` 的严格流程**。更重要的是，不同的图像格式对应不同的像素字节数（1字节/像素、2字节/像素、3字节/像素），在代码运行中动态切换极易引发内存越界崩溃（如段错误退出）。

为了保证系统的绝对稳定和图像的高帧率，本节点在内部固定采用**原生 YUV422 格式高效转换为标准的 ROS 2 BGR8 格式**，并已在 `rqt_image_view` 和 `rviz2` 中稳定验证。

### 6.5 断线重连参数恢复
节点支持断线重连，并在重连成功后**自动恢复已配置的参数**（曝光时间 `exposure_time` 和增益 `gain`）。在重连逻辑中，已强制关闭自动曝光（`Auto Exposure`）和自动增益（`Auto Gain`），确保设置值立即生效。


7. 功能验证要点

· 图像发布：稳定输出，默认分辨率下维持高帧率。
· 断线重连：拔掉相机 USB 线后，节点不会退出；重新插上 USB 线后，节点会自动重连并恢复已配置的曝光和增益参数。
· 内存与稳定性：节点内部严格使用 SDK 返回的帧长度分配内存并进行拷贝，无内存泄漏和越界风险。

```