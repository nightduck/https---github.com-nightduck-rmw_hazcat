This is an rmw wrapper for the [Hazcat](https://github.com/nightduck/hazcat) zero-copy middleware. More information on the project can be found in that repository.

# Installation

To install from source, add rmw_hazcat and hazcat into your ROS2 workspace

    cd ~/ros2_ws/src/ros2
    git clone git@github.com:nightduck/rmw_hazcat.git
    cd ../
    git clone git@github.com:nightduck/hazcat.git

Install dependencies and build the workspace as usual

    cd ~/ros2_ws
    rosdep udpate
    rosdep install --from-paths src --ignore-src --rosdistro LATEST_ROS_VERSION -y
    colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF

Limitations
===========

rmw_hazcat is still extremely experimental. Below are the features currently supported.

| ROS 2 command/feature | Status              |
|-----------------------|---------------------|
| `ros2 run`            | :heavy_check_mark:  |
| `ros2 topic list`     | :x:                 |
| `ros2 topic echo`     | :x:                 |
| `ros2 topic type`     | :x:                 |
| `ros2 topic info`     | :x:                 |
| `ros2 topic hz`       | :x:                 |
| `ros2 topic bw`       | :x:                 |
| `ros2 node list`      | :x:                 |
| `ros2 node info`      | :x:                 |
| `ros2 interface *`    | :x:                 |
| `ros2 service *`      | :x:                 |
| `ros2 param list`     | :x:                 |
| `ros2 bag`            | :x:                 |
| RMW Pub/Sub Events    | :x:                 |
