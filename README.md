# Hikrobot Camera ROS 2 节点

本项目是一个基于 **海康机器人 MVS SDK** 开发的 ROS 2 功能包（Package）。该节点能够自动发现并连接海康工业相机，采集图像数据，并将其发布为标准的 ROS 2 话题，支持断线重连和动态参数配置。

## 一、配置说明

### 1. 基础环境
*   **操作系统**：Ubuntu 22.04
*   **ROS 2 版本**：Humble

### 2. ROS 2 依赖
本节点已在 `package.xml` 中声明了依赖，您可以通过以下命令自动安装：
```bash
rosdep install --from-paths src --ignore-src -r -y
```

3. 海康 MVS SDK 配置

本节点依赖海康机器人 MVS SDK。该 SDK 不在 rosdep 规则内，不能通过 rosdep 或 apt 自动安装。

· 下载：请从海康机器人官网下载 Linux x86_64 版本的 MVS SDK（本机测试版本为 V5.1.0）。
· 安装：解压下载的压缩包，安装其中的 .deb 文件。
· 路径配置：默认安装在 /opt/MVS 目录下。本项目的 CMakeLists.txt 已固定配置以下路径：
  · 头文件路径：/opt/MVS/include
  · 库文件路径：/opt/MVS/lib/64

二、编译说明

在工程根目录下打开终端，执行以下命令：

```bash
colcon build --packages-select hikrobot_camera
source install/setup.bash
```

三、运行说明

1. 启动相机节点

推荐使用 Launch 文件一键启动，它会自动加载 config/camera.yaml 中的默认参数：

```bash
ros2 launch hikrobot_camera camera.launch.py
```

2. 在 Rviz2 中查看图像

节点启动后，会发布 /image_raw 话题。可以通过 Rviz2 稳定地查看：

1. 另开一个终端，输入 rviz2。
2. 点击左下角 Add -> 选择 By topic。
3. 找到 /image_raw，选择 Image 并点击 OK。

3. 动态参数配置

节点运行期间，可以动态调整参数。设置参数时节点会自动校验范围并调用底层 SDK，设置失败会返回明确原因。

· 设置曝光时间 (Exposure Time)：
  ```bash
  ros2 param set /hikrobot_camera exposure_time 10000.0
  ```
· 设置增益 (Gain)：
  ```bash
  ros2 param set /hikrobot_camera gain 15.0
  ```
· 设置帧率 (Frame Rate)：
  ```bash
  ros2 param set /hikrobot_camera frame_rate 30.0
  ```
· 设置图像格式 (Pixel Format)：
  ```bash
  ros2 param set /hikrobot_camera pixel_format 17301505  # 示例：PixelType_Gvsp_BGR8_Packed
  ```

```