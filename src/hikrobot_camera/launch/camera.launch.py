import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # 1. 找到你刚才生成的 rviz 配置文件（绝对路径，防止老师敲错）
    rviz_config_dir = os.path.expanduser('~/下载/assignment3-ROS2-main/config/camera.rviz')

    return LaunchDescription([
        # 2. 启动你的相机节点 (注意：executable 必须改成你真实的节点可执行文件名)
        Node(
            package='hikrobot_camera',
            executable='camera_node',  # <--- 这里检查一下，如果是其他名字请修改
            name='hikrobot_camera',
            output='screen'
        ),
        # 3. 自动拉起 RViz2 并强行加载配置文件
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', rviz_config_dir],
            output='screen'
        )
    ])