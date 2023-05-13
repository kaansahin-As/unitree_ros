#include <nav_msgs/msg/detail/odometry__struct.hpp>
#include <unitree_ros/unitree_ros.hpp>

#include "unitree_ros/serializers.hpp"

UnitreeRosNode::UnitreeRosNode() : Node("unitree_ros_node"), unitree_driver() {
    read_parameters();
    init_subscriptions();
    init_publishers();
    init_timers();
    tf_broadcaster = std::make_unique<tf2_ros::TransformBroadcaster>(this);

    RCLCPP_INFO(get_logger(), "Unitree ROS node initialized!");
}

UnitreeRosNode::~UnitreeRosNode() {
    RCLCPP_INFO(get_logger(), "Shutting down Unitree ROS node...");
    unitree_driver.stop();
}

void UnitreeRosNode::read_parameters() {
    RCLCPP_INFO(get_logger(), "Reading ROS parameters...");

    // Robot params
    declare_parameter<std::string>("robot_ip", robot_ip);
    declare_parameter<int>("robot_target_port", robot_target_port);
    // --------------------------------------------------------
    get_parameter("robot_ip", robot_ip);
    get_parameter("robot_target_port", robot_target_port);

    // Namespace
    declare_parameter<std::string>("ns", ns);
    // --------------------------------------------------------
    get_parameter("ns", ns);

    // Topic Names
    declare_parameter<std::string>("cmd_vel_topic_name", cmd_vel_topic_name);
    declare_parameter<std::string>("imu_topic_name", imu_topic_name);
    declare_parameter<std::string>("odom_topic_name", odom_topic_name);
    declare_parameter<std::string>("bms_state_topic_name", bms_topic_name);
    // --------------------------------------------------------
    get_parameter("cmd_vel_topic_name", cmd_vel_topic_name);
    get_parameter("odom_topic_name", odom_topic_name);
    get_parameter("imu_topic_name", imu_topic_name);
    get_parameter("bms_state_topic_name", bms_topic_name);

    // Frame Ids
    declare_parameter<std::string>("imu_frame_id", imu_frame_id);
    declare_parameter<std::string>("odom_frame_id", odom_frame_id);
    declare_parameter<std::string>("odom_child_frame_id", odom_child_frame_id);
    // --------------------------------------------------------
    get_parameter("odometry_frame_id", odom_frame_id);
    get_parameter("odometry_child_frame_id", odom_child_frame_id);
    get_parameter("imu_frame_id", imu_frame_id);

    apply_namespace_to_topic_names();
    RCLCPP_INFO(get_logger(), "Finished reading ROS parameters!");
}

void UnitreeRosNode::apply_namespace_to_topic_names() {
    cmd_vel_topic_name = ns + cmd_vel_topic_name;
    odom_topic_name = ns + odom_topic_name;
    imu_topic_name = ns + imu_topic_name;
    bms_topic_name = ns + bms_topic_name;

    imu_frame_id = ns + '/' + imu_frame_id;
    odom_frame_id = ns + '/' + odom_frame_id;
    odom_child_frame_id = ns + '/' + odom_child_frame_id;
}

void UnitreeRosNode::init_subscriptions() {
    RCLCPP_INFO(get_logger(), "Initializing ROS subscriptions...");

    rclcpp::QoS qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    cmd_vel_sub = this->create_subscription<geometry_msgs::msg::Twist>(
        cmd_vel_topic_name,
        qos,
        std::bind(&UnitreeRosNode::cmd_vel_callback, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(), "Finished initializing ROS subscriptions!");
}

void UnitreeRosNode::init_publishers() {
    RCLCPP_INFO(get_logger(), "Initializing ROS publishers...");

    rclcpp::QoS qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
    odom_pub = this->create_publisher<nav_msgs::msg::Odometry>(odom_topic_name, qos);
    imu_pub = this->create_publisher<sensor_msgs::msg::Imu>(imu_topic_name, qos);
    bms_pub = this->create_publisher<unitree_ros::msg::BmsState>(bms_topic_name, qos);

    RCLCPP_INFO(get_logger(), "Finished initializing ROS publishers!");
}

void UnitreeRosNode::init_timers() {
    RCLCPP_INFO(get_logger(), "Initializing ROS timers...");

    robot_state_timer = this->create_wall_timer(
        2ms, std::bind(&UnitreeRosNode::robot_state_callback, this));

    cmd_vel_reset_timer = this->create_wall_timer(
        1ms, std::bind(&UnitreeRosNode::cmd_vel_reset_callback, this));

    RCLCPP_INFO(get_logger(), "Finished initializing ROS timers!");
}

void UnitreeRosNode::cmd_vel_callback(const geometry_msgs::msg::Twist::UniquePtr msg) {
    unitree_driver.walk_w_vel(msg->linear.x, msg->linear.y, msg->angular.z);
    prev_cmd_vel_sent = clock.now();
}

void UnitreeRosNode::robot_state_callback() {
    publish_odom();
    publish_imu();
    publish_bms();
    publish_odom_tf();
}

void UnitreeRosNode::cmd_vel_reset_callback() {
    auto delta_t = clock.now() - prev_cmd_vel_sent;
    if (delta_t >= 400ms && delta_t <= 402ms) {
        unitree_driver.walk_w_vel(0, 0, 0);
    }
}

void UnitreeRosNode::publish_odom() {
    nav_msgs::msg::Odometry odom_msg;
    odom_msg.header.stamp = this->get_clock()->now();
    odom_msg.header.frame_id = odom_frame_id;
    odom_msg.child_frame_id = odom_child_frame_id;
    serialize(odom_msg, unitree_driver.get_odom());
    odom_pub->publish(odom_msg);
}

void UnitreeRosNode::publish_imu() {
    sensor_msgs::msg::Imu imu_msg;
    imu_msg.header.stamp = this->get_clock()->now();
    imu_msg.header.frame_id = imu_frame_id;
    serialize(imu_msg, unitree_driver.get_imu());
    imu_pub->publish(imu_msg);
}

void UnitreeRosNode::publish_bms() {
    unitree_ros::msg::BmsState bms_msg;
    serialize(bms_msg, unitree_driver.get_bms());
    bms_pub->publish(bms_msg);
}

void UnitreeRosNode::publish_odom_tf() {
    geometry_msgs::msg::TransformStamped transform;
    odom_t odom = unitree_driver.get_odom();

    transform.header.stamp = this->get_clock()->now();
    transform.header.frame_id = odom_frame_id;
    transform.child_frame_id = odom_child_frame_id;

    transform.transform.translation.x = odom.pose.position.x;
    transform.transform.translation.y = odom.pose.position.y;
    transform.transform.translation.z = odom.pose.position.z;

    tf2::Quaternion q;
    q.setRPY(odom.pose.orientation.x, odom.pose.orientation.y, odom.pose.orientation.z);
    transform.transform.rotation.x = q.x();
    transform.transform.rotation.y = q.y();
    transform.transform.rotation.z = q.z();
    transform.transform.rotation.w = q.w();

    tf_broadcaster->sendTransform(transform);
}