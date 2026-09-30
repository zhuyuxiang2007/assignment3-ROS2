cmake_minimum_required(VERSION 3.8)
project(hikrobot_camera)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)

# 告诉编译器去哪里找海康的头文件
include_directories(/opt/MVS/include)

# 告诉编译器去哪里找海康的库文件
link_directories(/opt/MVS/lib/64)




add_executable(camera_node
  
  src/main.cpp
  src/camera_node.cpp

)

target_include_directories(camera_node PRIVATE include)





# 把海康的库链接到你的节点上
target_link_libraries(camera_node MvCameraControl cv_bridge::cv_bridge sensor_msgs::sensor_msgs)

target_compile_features(camera_node PRIVATE cxx_std_17)
ament_target_dependencies(camera_node rclcpp sensor_msgs cv_bridge)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(camera_node PRIVATE -Wall -Wextra -Wpedantic)
endif()

# TODO(student): Declare and link the dependencies used by your implementation,
# including the vendor SDK. The starter does not discover or link MVS.

install(TARGETS camera_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

ament_package()
