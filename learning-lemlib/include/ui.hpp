#pragma once

#include <cstdint>

namespace team_ui {

enum class Alliance : std::uint8_t {
    RED,
    BLUE,
};

void initialize();
Alliance alliance();
int selected_auton();
bool is_locked();
void wait_for_lock();
bool consume_manual_start_request();

}  // namespace team_ui
