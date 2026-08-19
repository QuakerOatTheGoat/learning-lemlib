#pragma once
#include "../include/lemlib/api.hpp"
#include "api.h"
#include "lemlib/chassis/trackingWheel.hpp"
#include "pros/rotation.hpp"
#include <vector>
#include <functional>

inline pros::MotorGroup left_motors({1, -2, -5}, pros::MotorGearset::blue);
inline pros::MotorGroup right_motors({8, 9, -10}, pros::MotorGearset::blue); 

inline lemlib::Drivetrain drivetrain(&left_motors, &right_motors, 11.5, lemlib::Omniwheel::NEW_325, 480, 2);

inline pros::IMU imu(13);
inline pros::Rotation vert(-16);
inline pros::Rotation horiz(7);

inline lemlib::TrackingWheel Vert(&vert, lemlib::Omniwheel::NEW_2, .5);
inline lemlib::TrackingWheel Horiz(&horiz, lemlib::Omniwheel::NEW_275, -0.5);

inline lemlib::OdomSensors sensors(nullptr, // vertical tracking wheel 1, set to null
                            nullptr, // vertical tracking wheel 2, set to nullptr as we are using IMEs
                            nullptr, // horizontal tracking wheel 1
                            nullptr, // horizontal tracking wheel 2, set to nullptr as we don't have a second one
                            &imu // inertial sensor
);

// lateral PID controller
inline lemlib::ControllerSettings lateral_controller(6, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              20, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in inches
                                              100, // small error range timeout, in milliseconds
                                              3, // large error range, in inches
                                              500, // large error range timeout, in milliseconds
                                              20 // maximum acceleration (slew)
);

// angular PID controller
inline lemlib::ControllerSettings angular_controller(4, // proportional gain (kP)
                                              0, // integral gain (kI)
                                              33, // derivative gain (kD)
                                              3, // anti windup
                                              1, // small error range, in degrees
                                              100, // small error range timeout, in milliseconds
                                              3, // large error range, in degrees
                                              500, // large error range timeout, in milliseconds
                                              0 // maximum acceleration (slew)
);

inline lemlib::Chassis chassis(drivetrain, // drivetrain settings
                        lateral_controller, // lateral PID settings
                        angular_controller, // angular PID settings
                        sensors // odometry sensors
);

//void run_auton(int auton_number);

//inline std::vector<std::function<void(void)>> autons;