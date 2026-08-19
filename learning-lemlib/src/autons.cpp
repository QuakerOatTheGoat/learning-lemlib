#include "subsystems.hpp"
#include "autons.hpp"

void basic_auton() {
    // set position to x:0, y:0, heading:0
    chassis.setPose(0, 0, 0);
    // move 36" forwards
    chassis.moveToPoint(0, 36, 1000, {.minSpeed = 60, .earlyExitRange = 5});
    chassis.turnToPoint(24, 72, 2000, {.minSpeed = 60, .earlyExitRange = 15});
    chassis.moveToPoint(24, 72, 1000, {.minSpeed = 60, .earlyExitRange = 5});
    chassis.moveToPoint(0, 96, 1000, {.minSpeed = 60, .earlyExitRange = 5});
    chassis.moveToPoint(-24, 72, 1000, {.minSpeed = 60, .earlyExitRange = 5});
    chassis.moveToPoint(0, 32, 1000, {.minSpeed = 60, .earlyExitRange = 5});
    chassis.turnToPoint(0, 0, 2000, {.minSpeed = 60, .earlyExitRange = 15});
    chassis.moveToPoint(0, 0, 1000);
}