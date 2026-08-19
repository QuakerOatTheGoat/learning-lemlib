#include "main.h"
#include "autons.hpp"
#include "pal/auto.h"
#include "subsystems.hpp"
#include "ui.hpp"

int page = 0;

void pre_auto(auto_color_t color, auto_pos_t pos) {
    (void)color;
    (void)pos;
    pros::delay(500);
}

void auton_left(auto_color_t color, auto_pos_t pos) {
    (void)color;
    (void)pos;
    basic_auton();
}

void auton_right(auto_color_t color, auto_pos_t pos) {
    (void)color;
    (void)pos;
    chassis.setPose(0, 0, 0);
    chassis.moveToPoint(0, 48, 1500);
}

void auton_test(auto_color_t color, auto_pos_t pos) {
    (void)color;
    (void)pos;
    chassis.setPose(0, 0, 0);
    chassis.moveToPoint(0, 24, 1500, {}, false);
    chassis.turnToHeading(90, 1000, {}, false);
    chassis.moveToPoint(24, 24, 1500, {}, false);
}

const auto_routine_t auto_list[] = {
    {auton_left, AUTO_POS_1, "Left Auton"},
    {auton_right, AUTO_POS_2, "Right Auton"},
    {auton_left, AUTO_POS_SKILLS, "Skills"},
};

void initialize() {
    pros::lcd::initialize();
    chassis.calibrate();
    team_ui::initialize();

    pros::Task screen_task([&]() {
        while (true) {
            switch (page) {
                case 0:
                    pros::lcd::print(0, "X: %f", chassis.getPose().x);
                    pros::lcd::print(1, "Y: %f", chassis.getPose().y);
                    pros::lcd::print(2, "Theta: %f", chassis.getPose().theta);
                    break;
                case 1:
                    pros::lcd::print(0, "Auton Selected", nullptr);
                    break;
            }
            pros::delay(50);
        }
    });
}

void disabled() {}

void competition_initialize() {}

void run_selected_auton() {
    switch (team_ui::selected_auton()) {
        case 0:
            auton_left(team_ui::alliance() == team_ui::Alliance::RED ? AUTO_COLOR_RED : AUTO_COLOR_BLUE,
                       AUTO_POS_1);
            break;
        case 1:
            auton_right(team_ui::alliance() == team_ui::Alliance::RED ? AUTO_COLOR_RED : AUTO_COLOR_BLUE,
                        AUTO_POS_2);
            break;
        case 2:
            auton_left(team_ui::alliance() == team_ui::Alliance::RED ? AUTO_COLOR_RED : AUTO_COLOR_BLUE,
                       AUTO_POS_SKILLS);
            break;
        case 3:
            pros::delay(20);
            break;
        case 4:
            auton_test(team_ui::alliance() == team_ui::Alliance::RED ? AUTO_COLOR_RED : AUTO_COLOR_BLUE,
                       AUTO_POS_1);
            break;
        default:
            pros::delay(20);
            break;
    }
}

void autonomous() {
    team_ui::wait_for_lock();
    run_selected_auton();
}

pros::Controller controller(pros::E_CONTROLLER_MASTER);

void opcontrol() {
    auto_clean();

    while (true) {
        if (team_ui::consume_manual_start_request() && team_ui::is_locked()) {
            run_selected_auton();
            chassis.arcade(0, 0);
            continue;
        }
        int rightY = controller.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_Y);
        int leftX = controller.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_X);
        chassis.arcade(rightY, leftX);
        pros::delay(20);
    }
}