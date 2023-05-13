#include <iostream>
#include <unitree_ros/unitree_driver.hpp>

#include "unitree_ros/unitree_data.hpp"

UnitreeDriver::UnitreeDriver(std::string ip_addr_, int target_port_)
    : udp_connection_(UNITREE_LEGGED_SDK::HIGHLEVEL,
                      local_port,
                      ip_addr_.c_str(),
                      target_port_) {
    // Check if the connection is established
    if (!is_connection_established_()) {
        throw std::runtime_error("Connection to the robot could not be established!");
    }

    illuminate_foot_led({0, 255, 0});

    // Initialize the high level command and state
    udp_connection_.InitCmdData(high_cmd);

    stand_up();
}

UnitreeDriver::~UnitreeDriver() { stand_down(); }

// -----------------------------------------------------------------------------
// -                                 Getters                                   -
// -----------------------------------------------------------------------------

position_t UnitreeDriver::get_position() {
    return {high_state.position[0], high_state.position[1], high_state.position[2]};
}

orientation_t UnitreeDriver::get_orientation() {
    return {high_state.imu.rpy[0], high_state.imu.rpy[1], high_state.imu.rpy[2], 0};
}

velocity_t UnitreeDriver::get_velocity() {
    return {high_state.velocity[0], high_state.velocity[1], high_state.yawSpeed};
}

odom_t UnitreeDriver::get_odom() {
    recv_high_state_();
    position_t position = get_position();
    orientation_t orientation = get_orientation();
    velocity_t velocity = get_velocity();
    pose_t pose = {position, orientation};
    return {pose, velocity};
}

UNITREE_LEGGED_SDK::IMU UnitreeDriver::get_imu() { return high_state.imu; }

UNITREE_LEGGED_SDK::BmsState UnitreeDriver::get_bms() { return high_state.bms; }

// -----------------------------------------------------------------------------
// -                                  Setters                                  -
// -----------------------------------------------------------------------------

void UnitreeDriver::set_mode(mode_enum mode) { curr_mode = mode; }

void UnitreeDriver::set_gaitype(gaitype_enum gait_type) { curr_gait_type = gait_type; }

// -----------------------------------------------------------------------------
// -                             Robot Functions                               -
// -----------------------------------------------------------------------------

void UnitreeDriver::stand_down() {
    walk_w_vel(0, 0, 0);
    set_gaitype(gaitype_enum::GAITYPE_IDDLE);
    set_mode(mode_enum::STAND_DOWN);
    send_high_cmd_();
}

void UnitreeDriver::stand_up() {
    walk_w_vel(0, 0, 0);
    set_mode(mode_enum::STAND_UP);
    set_gaitype(gaitype_enum::TROT);
    send_high_cmd_();
}

void UnitreeDriver::walk_w_vel(float x, float y, float yaw) {
    high_cmd.mode = mode_enum::WALK_W_VEL;
    high_cmd.gaitType = curr_gait_type;
    high_cmd.velocity[0] = x;
    high_cmd.velocity[1] = y;
    high_cmd.yawSpeed = yaw;
    send_high_cmd_();
}

void UnitreeDriver::walk_w_pos(position_t position, orientation_t orientation) {
    high_cmd.mode = mode_enum::WALK_W_POS;
    high_cmd.gaitType = curr_gait_type;
    high_cmd.position[0] = position.x;
    high_cmd.position[1] = position.y;
    high_cmd.euler[0] = orientation.x;
    high_cmd.euler[1] = orientation.y;
    high_cmd.euler[2] = orientation.z;
    send_high_cmd_();
}

void UnitreeDriver::illuminate_foot_led(UNITREE_LEGGED_SDK::LED led) {
    high_cmd.led[0] = led;
    high_cmd.led[1] = led;
    high_cmd.led[2] = led;
    high_cmd.led[3] = led;
    send_high_cmd_();
}

void UnitreeDriver::damping_mode() {
    recv_high_state_();
    if (high_state.mode == mode_enum::STAND_DOWN) {
        high_cmd.mode = mode_enum::DAMPING_MODE;
        send_high_cmd_();
    } else {
        std::cout << "Robot is not in STAND_DOWN mode. Make sure to stand down the "
                     "robot first"
                  << std::endl;
    }
}

void UnitreeDriver::stop() {
    walk_w_vel(0, 0, 0);
    stand_down();
    damping_mode();
}

// -----------------------------------------------------------------------------
// -                             Private Functions                             -
// -----------------------------------------------------------------------------

bool UnitreeDriver::is_connection_established_() { return true; }

void UnitreeDriver::send_high_cmd_() {
    udp_connection_.SetSend(high_cmd);
    udp_connection_.Send();
}

void UnitreeDriver::recv_high_state_() {
    udp_connection_.Send();
    udp_connection_.Recv();
    udp_connection_.GetRecv(high_state);
}