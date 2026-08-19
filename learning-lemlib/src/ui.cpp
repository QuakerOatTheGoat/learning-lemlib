#include "ui.hpp"

#include "liblvgl/core/lv_obj.h"
#include "liblvgl/core/lv_obj_event.h"
#include "liblvgl/core/lv_obj_pos.h"
#include "liblvgl/core/lv_obj_style.h"
#include "liblvgl/display/lv_display.h"
#include "liblvgl/libs/fsdrv/lv_fsdrv.h"
#include "liblvgl/draw/lv_image_dsc.h"
#include "liblvgl/misc/lv_color.h"
#include "liblvgl/misc/lv_fs.h"
#include "liblvgl/misc/lv_timer.h"
#include "liblvgl/widgets/button/lv_button.h"
#include "liblvgl/widgets/image/lv_image.h"
#include "liblvgl/widgets/label/lv_label.h"
#include "liblvgl/widgets/led/lv_led.h"
#include "liblvgl/widgets/slider/lv_slider.h"
#include "liblvgl/widgets/tabview/lv_tabview.h"
#include "liblvgl/widgets/textarea/lv_textarea.h"
#include "pros/rtos.hpp"
#include "pros/misc.hpp"
#include "pros/imu.hpp"
#include "subsystems.hpp"

#include <atomic>
#include <cstdint>

namespace {

// Team palette: cyan on charcoal, with earthy brown structure.
constexpr std::uint32_t CYAN_HEX = 0x59D9F5;
constexpr std::uint32_t CYAN_DARK_HEX = 0x1A8199;
constexpr std::uint32_t BROWN_HEX = 0x6B4226;
constexpr std::uint32_t BROWN_LIGHT_HEX = 0x9A6741;
constexpr std::uint32_t CHARCOAL_HEX = 0x171412;
constexpr std::uint32_t PANEL_HEX = 0x241C18;
constexpr std::uint32_t TEXT_HEX = 0xEAFBFF;
constexpr std::uint32_t MUTED_HEX = 0xA9B8BA;
constexpr std::uint32_t RED_HEX = 0xE05A5A;
constexpr std::uint32_t BLUE_HEX = 0x397BDB;

constexpr lv_color_t CYAN = LV_COLOR_MAKE(0x59, 0xD9, 0xF5);
constexpr lv_color_t CYAN_DARK = LV_COLOR_MAKE(0x1A, 0x81, 0x99);
constexpr lv_color_t BROWN = LV_COLOR_MAKE(0x6B, 0x42, 0x26);
constexpr lv_color_t BROWN_LIGHT = LV_COLOR_MAKE(0x9A, 0x67, 0x41);
constexpr lv_color_t CHARCOAL = LV_COLOR_MAKE(0x17, 0x14, 0x12);
constexpr lv_color_t PANEL = LV_COLOR_MAKE(0x24, 0x1C, 0x18);
constexpr lv_color_t TEXT = LV_COLOR_MAKE(0xEA, 0xFB, 0xFF);
constexpr lv_color_t MUTED = LV_COLOR_MAKE(0xA9, 0xB8, 0xBA);
constexpr lv_color_t RED = LV_COLOR_MAKE(0xE0, 0x5A, 0x5A);
constexpr lv_color_t BLUE = LV_COLOR_MAKE(0x39, 0x7B, 0xDB);

constexpr std::size_t AUTON_COUNT = 5;
constexpr lv_coord_t FIELD_INSET = 2;
const char* const AUTON_NAMES[AUTON_COUNT] = {
    "Left Side Quals",
    "Right Side Elims",
    "AWP Solo",
    "Do Nothing",
    "Test Auton",
};

std::atomic<int> selected_auton_index{0};
std::atomic<int> alliance_index{0};
std::atomic<bool> auton_locked{false};
std::atomic<int> controller_auton_delta{0};
std::atomic<int> controller_alliance{-1};
std::atomic<int> controller_tab_delta{0};
std::atomic<bool> controller_lock_request{false};
std::atomic<bool> controller_unlock_request{false};
std::atomic<bool> controller_manual_start_request{false};
std::atomic<float> gyro_angle{0.0f};
std::atomic<int> battery_mv{0};

lv_obj_t* auton_buttons[AUTON_COUNT]{};
lv_obj_t* simulation_title = nullptr;
lv_obj_t* angle_label = nullptr;
lv_obj_t* battery_label = nullptr;
lv_obj_t* alignment_led = nullptr;
lv_obj_t* alignment_title = nullptr;
lv_obj_t* console = nullptr;
lv_obj_t* red_button = nullptr;
lv_obj_t* blue_button = nullptr;
lv_obj_t* lock_button = nullptr;
lv_obj_t* picker_status = nullptr;
lv_obj_t* tabview = nullptr;
struct PidControl {
    lv_obj_t* slider;
    lv_obj_t* value_label;
    bool angular;
    char gain;
};
PidControl pid_controls[6]{};
lv_obj_t* field_image = nullptr;
lv_obj_t* path_segments[3]{};
std::size_t path_segment_count = 0;
lv_image_dsc_t field_bitmap{};
std::uint8_t* field_bitmap_pixels = nullptr;

std::uint16_t read_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::uint32_t read_u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
                                       (bytes[3] << 24));
}

bool load_field_bitmap() {
    lv_fs_file_t file{};
    if (lv_fs_open(&file, "S:/field.bmp", LV_FS_MODE_RD) != LV_FS_RES_OK) {
        return false;
    }

    std::uint8_t header[54]{};
    std::uint32_t bytes_read = 0;
    if (lv_fs_read(&file, header, sizeof(header), &bytes_read) != LV_FS_RES_OK ||
        bytes_read != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M' || read_u32(header + 14) < 40) {
        lv_fs_close(&file);
        return false;
    }

    const std::int32_t width = static_cast<std::int32_t>(read_u32(header + 18));
    const std::int32_t signed_height = static_cast<std::int32_t>(read_u32(header + 22));
    const std::uint16_t planes = read_u16(header + 26);
    const std::uint16_t bits_per_pixel = read_u16(header + 28);
    const std::uint32_t compression = read_u32(header + 30);
    const std::int32_t height = signed_height < 0 ? -signed_height : signed_height;
    const bool supported_compression = compression == 0 || (bits_per_pixel == 32 && compression == 3);
    if (width <= 0 || height <= 0 || width > 480 || height > 480 || planes != 1 ||
        (bits_per_pixel != 24 && bits_per_pixel != 32) || !supported_compression) {
        lv_fs_close(&file);
        return false;
    }

    const std::size_t source_stride = ((static_cast<std::size_t>(width) * bits_per_pixel + 31) / 32) * 4;
    const std::size_t output_stride = static_cast<std::size_t>(width) * 3;
    field_bitmap_pixels = static_cast<std::uint8_t*>(std::malloc(output_stride * height));
    if (field_bitmap_pixels == nullptr ||
        lv_fs_seek(&file, read_u32(header + 10), LV_FS_SEEK_SET) != LV_FS_RES_OK) {
        std::free(field_bitmap_pixels);
        field_bitmap_pixels = nullptr;
        lv_fs_close(&file);
        return false;
    }

    std::uint8_t* source_row = static_cast<std::uint8_t*>(std::malloc(source_stride));
    if (source_row == nullptr) {
        std::free(field_bitmap_pixels);
        field_bitmap_pixels = nullptr;
        lv_fs_close(&file);
        return false;
    }
    const bool bottom_up = signed_height > 0;
    for (std::int32_t y = 0; y < height; ++y) {
        bytes_read = 0;
        if (lv_fs_read(&file, source_row, static_cast<uint32_t>(source_stride), &bytes_read) != LV_FS_RES_OK ||
            bytes_read != source_stride) {
            std::free(source_row);
            std::free(field_bitmap_pixels);
            field_bitmap_pixels = nullptr;
            lv_fs_close(&file);
            return false;
        }
        const std::int32_t output_y = bottom_up ? height - 1 - y : y;
        std::uint8_t* output = field_bitmap_pixels + output_y * output_stride;
        for (std::int32_t x = 0; x < width; ++x) {
            const std::uint8_t* pixel = source_row + x * (bits_per_pixel / 8);
            output[x * 3] = pixel[2];
            output[x * 3 + 1] = pixel[1];
            output[x * 3 + 2] = pixel[0];
        }
    }
    std::free(source_row);
    lv_fs_close(&file);

    field_bitmap.header.magic = LV_IMAGE_HEADER_MAGIC;
    field_bitmap.header.cf = LV_COLOR_FORMAT_RGB888;
    field_bitmap.header.w = static_cast<std::uint32_t>(width);
    field_bitmap.header.h = static_cast<std::uint32_t>(height);
    field_bitmap.header.stride = static_cast<std::uint32_t>(output_stride);
    field_bitmap.data_size = static_cast<std::uint32_t>(output_stride * height);
    field_bitmap.data = field_bitmap_pixels;
    return true;
}

lv_color_t color_from_hex(std::uint32_t hex) {
    return lv_color_make(static_cast<std::uint8_t>((hex >> 16) & 0xFF),
                         static_cast<std::uint8_t>((hex >> 8) & 0xFF),
                         static_cast<std::uint8_t>(hex & 0xFF));
}

void set_background(lv_obj_t* object, lv_color_t color) {
    lv_obj_set_style_bg_color(object, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
}

void set_text(lv_obj_t* object, lv_color_t color) {
    lv_obj_set_style_text_color(object, color, LV_PART_MAIN);
}

lv_obj_t* make_label(lv_obj_t* parent, const char* text, lv_color_t color = TEXT) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    set_text(label, color);
    return label;
}

lv_obj_t* make_button(lv_obj_t* parent, const char* text, lv_coord_t width, lv_coord_t height,
                     lv_event_cb_t callback, void* user_data = nullptr) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    set_background(button, BROWN);
    lv_obj_set_style_border_color(button, BROWN_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    lv_obj_t* label = make_label(button, text);
    lv_obj_center(label);
    return button;
}

void refresh_auton_buttons() {
    const int selected = selected_auton_index.load();
    const bool locked = auton_locked.load();
    for (std::size_t index = 0; index < AUTON_COUNT; ++index) {
        const bool active = static_cast<int>(index) == selected;
        set_background(auton_buttons[index], locked ? PANEL : (active ? CYAN_DARK : BROWN));
        lv_obj_set_style_border_color(auton_buttons[index], active ? CYAN : BROWN_LIGHT, LV_PART_MAIN);
        lv_obj_t* label = lv_obj_get_child(auton_buttons[index], 0);
        if (label != nullptr) {
            set_text(label, locked ? MUTED : (active ? TEXT : MUTED));
        }
    }
    if (lock_button != nullptr) {
        set_background(lock_button, locked ? CYAN_DARK : CYAN);
        lv_obj_set_style_border_color(lock_button, locked ? CYAN : TEXT, LV_PART_MAIN);
        lv_obj_t* label = lv_obj_get_child(lock_button, 0);
        if (label != nullptr) {
            lv_label_set_text(label, locked ? "AUTON LOCKED" : "LOCK IN AUTON");
            set_text(label, locked ? TEXT : CHARCOAL);
        }
    }
    if (picker_status != nullptr) {
        lv_label_set_text(picker_status, locked ? "LOCKED  /  HOLD X + B TO UNLOCK" :
                                             "UNLOCKED  /  SELECT AUTON + ALLIANCE");
        set_text(picker_status, locked ? CYAN : MUTED);
    }
    for (PidControl& control : pid_controls) {
        if (control.slider == nullptr) {
            continue;
        }
        if (locked) {
            lv_obj_add_flag(control.slider, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(control.slider, LV_OBJ_FLAG_CLICKABLE);
            set_text(control.value_label, MUTED);
        } else {
            lv_obj_add_flag(control.slider, LV_OBJ_FLAG_CLICKABLE);
            set_text(control.value_label, TEXT);
        }
    }
}

void refresh_alliance_buttons() {
    const bool red_selected = alliance_index.load() == 0;
    set_background(red_button, red_selected ? RED : BROWN);
    set_background(blue_button, red_selected ? BROWN : BLUE);
    lv_obj_set_style_border_color(red_button, red_selected ? TEXT : BROWN_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(blue_button, red_selected ? BROWN_LIGHT : TEXT, LV_PART_MAIN);
}

void update_simulation_path(int routine) {
    if (path_segment_count < 3) {
        return;
    }

    const int paths[AUTON_COUNT][3][4] = {
        {{16, 112, 88, 4}, {100, 72, 4, 44}, {100, 72, 44, 4}},
        {{48, 112, 88, 4}, {48, 72, 4, 44}, {48, 72, 44, 4}},
        {{20, 42, 4, 72}, {20, 42, 60, 4}, {80, 42, 4, 40}},
        {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{24, 112, 4, 64}, {24, 48, 56, 4}, {80, 48, 4, 36}},
    };
    for (std::size_t index = 0; index < path_segment_count; ++index) {
        const int* segment = paths[routine][index];
        lv_obj_set_pos(path_segments[index], segment[0] + FIELD_INSET, segment[1] + FIELD_INSET);
        lv_obj_set_size(path_segments[index], segment[2], segment[3]);
        lv_obj_set_style_bg_opa(path_segments[index], routine == 3 ? LV_OPA_TRANSP : LV_OPA_COVER,
                                 LV_PART_MAIN);
    }
}

void auton_button_event(lv_event_t* event) {
    if (auton_locked.load()) {
        return;
    }
    const auto index = reinterpret_cast<std::intptr_t>(lv_event_get_user_data(event));
    selected_auton_index.store(static_cast<int>(index));
    refresh_auton_buttons();
    update_simulation_path(static_cast<int>(index));

    if (simulation_title != nullptr) {
        lv_label_set_text_fmt(simulation_title, "PATH  /  %s", AUTON_NAMES[index]);
    }
}

void red_button_event(lv_event_t*) {
    if (auton_locked.load()) {
        return;
    }
    alliance_index.store(0);
    refresh_alliance_buttons();
}

void blue_button_event(lv_event_t*) {
    if (auton_locked.load()) {
        return;
    }
    alliance_index.store(1);
    refresh_alliance_buttons();
}

void pid_slider_event(lv_event_t* event) {
    if (auton_locked.load()) {
        return;
    }
    auto* control = static_cast<PidControl*>(lv_event_get_user_data(event));
    const float value = static_cast<float>(lv_slider_get_value(control->slider)) / 10.0f;
    lemlib::PID& pid = control->angular ? chassis.angularPID : chassis.lateralPID;
    if (control->gain == 'P') pid.kP = value;
    if (control->gain == 'I') pid.kI = value;
    if (control->gain == 'D') pid.kD = value;
    lv_label_set_text_fmt(control->value_label, "%c  %0.1f", control->gain,
                          static_cast<double>(value));
}

void lock_button_event(lv_event_t*) {
    auton_locked.store(true);
    refresh_auton_buttons();
    refresh_alliance_buttons();
}

void draw_segment(lv_obj_t* parent, int x, int y, int width, int height) {
    lv_obj_t* segment = lv_obj_create(parent);
    lv_obj_remove_flag(segment, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(segment, x + FIELD_INSET, y + FIELD_INSET);
    lv_obj_set_size(segment, width, height);
    set_background(segment, CYAN);
    lv_obj_set_style_radius(segment, 2, LV_PART_MAIN);
    if (path_segment_count < 3) {
        path_segments[path_segment_count++] = segment;
    }
}

void create_simulation(lv_obj_t* tab) {
    simulation_title = make_label(tab, "PATH  /  Left Side Quals", CYAN);
    lv_obj_set_pos(simulation_title, 14, 8);

    lv_obj_t* field = lv_obj_create(tab);
    lv_obj_set_pos(field, 14, 32);
    lv_obj_set_size(field, 155, 155);
    set_background(field, CHARCOAL);
    lv_obj_set_style_border_color(field, BROWN_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(field, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(field, 3, LV_PART_MAIN);
    lv_obj_remove_flag(field, LV_OBJ_FLAG_SCROLLABLE);

    field_image = lv_image_create(field);
    lv_obj_set_size(field_image, 151, 151);
    lv_obj_align(field_image, LV_ALIGN_CENTER, 0, 0);
    if (load_field_bitmap()) {
        lv_image_set_src(field_image, &field_bitmap);
    } else {
        lv_obj_t* missing_label = make_label(field, "FIELD.BMP NOT FOUND", RED);
        lv_obj_center(missing_label);
    }
    lv_image_set_inner_align(field_image, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_remove_flag(field_image, LV_OBJ_FLAG_SCROLLABLE);

    // Route overlays are drawn above the SD-card field image.
    draw_segment(field, 16, 112, 88, 4);
    draw_segment(field, 104, 72, 4, 44);
    draw_segment(field, 104, 72, 44, 4);
    update_simulation_path(selected_auton_index.load());

    lv_obj_t* robot = lv_obj_create(field);
    lv_obj_set_pos(robot, 68 + FIELD_INSET, 68 + FIELD_INSET);
    lv_obj_set_size(robot, 16, 16);
    set_background(robot, CYAN);
    lv_obj_set_style_radius(robot, 8, LV_PART_MAIN);

    lv_obj_t* legend = make_label(tab, "CYAN = planned path   |   BROWN = field", MUTED);
    lv_obj_set_pos(legend, 14, 193);
}

void create_auton_selector(lv_obj_t* tab) {
    lv_obj_set_size(tab, 480, 208);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* selector_title = make_label(tab, "SELECT ROUTINE", CYAN);
    lv_obj_set_pos(selector_title, 15, 6);

    for (std::size_t index = 0; index < AUTON_COUNT; ++index) {
        auton_buttons[index] = make_button(tab, AUTON_NAMES[index], 250, 28,
                                            auton_button_event,
                                            reinterpret_cast<void*>(static_cast<std::intptr_t>(index)));
        lv_obj_set_pos(auton_buttons[index], 15, 28 + static_cast<int>(index) * 31);
    }
    red_button = make_button(tab, "RED ALLIANCE", 140, 34, red_button_event);
    blue_button = make_button(tab, "BLUE ALLIANCE", 140, 34, blue_button_event);
    lv_obj_set_pos(red_button, 285, 28);
    lv_obj_set_pos(blue_button, 285, 68);
    lock_button = make_button(tab, "LOCK IN AUTON", 165, 34, lock_button_event);
    lv_obj_set_pos(lock_button, 285, 108);
    picker_status = make_label(tab, "UNLOCKED  /  SELECT AUTON + ALLIANCE", MUTED);
    lv_obj_set_pos(picker_status, 15, 185);
    refresh_alliance_buttons();
    refresh_auton_buttons();
}

void create_match_tools(lv_obj_t* tab) {
    make_label(tab, "PID TUNER  /  LIVE GAINS", CYAN);
    const char gains[3] = {'P', 'I', 'D'};
    const char* const names[2] = {"LATERAL", "ANGULAR"};
    for (int group = 0; group < 2; ++group) {
        lv_obj_t* title = make_label(tab, names[group], group == 0 ? TEXT : CYAN);
        lv_obj_set_pos(title, group == 0 ? 15 : 250, 24);
        for (int index = 0; index < 3; ++index) {
            const int control_index = group * 3 + index;
            PidControl& control = pid_controls[control_index];
            control.angular = group == 1;
            control.gain = gains[index];
            control.slider = lv_slider_create(tab);
            lv_obj_set_pos(control.slider, group == 0 ? 15 : 250, 47 + index * 25);
            lv_obj_set_size(control.slider, 150, 12);
            lv_slider_set_range(control.slider, 0, 2000);
            const lemlib::PID& pid = control.angular ? chassis.angularPID : chassis.lateralPID;
            const float initial = control.gain == 'P' ? pid.kP : (control.gain == 'I' ? pid.kI : pid.kD);
            lv_slider_set_value(control.slider, static_cast<int32_t>(initial * 10.0f), LV_ANIM_OFF);
            control.value_label = make_label(tab, "", MUTED);
            lv_obj_set_pos(control.value_label, group == 0 ? 172 : 407, 44 + index * 25);
            lv_label_set_text_fmt(control.value_label, "%c  %0.1f", control.gain,
                                  static_cast<double>(initial));
            lv_obj_add_event_cb(control.slider, pid_slider_event, LV_EVENT_VALUE_CHANGED, &control);
        }
    }

    alignment_title = make_label(tab, "ALIGNMENT CHECK", CYAN);
    lv_obj_set_pos(alignment_title, 15, 128);
    lv_obj_t* align_panel = lv_obj_create(tab);
    lv_obj_set_pos(align_panel, 15, 148);
    lv_obj_set_size(align_panel, 440, 55);
    set_background(align_panel, PANEL);
    lv_obj_set_style_border_color(align_panel, BROWN_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(align_panel, 1, LV_PART_MAIN);

    alignment_led = lv_led_create(align_panel);
    lv_obj_set_pos(alignment_led, 14, 10);
    lv_obj_set_size(alignment_led, 35, 35);
    lv_led_set_color(alignment_led, CYAN);
    angle_label = make_label(align_panel, "GYRO  +0.0 deg", TEXT);
    lv_obj_set_pos(angle_label, 65, 5);
    make_label(align_panel, "SQUARE TO WALL", MUTED);
    lv_obj_set_pos(lv_obj_get_child(align_panel, 2), 65, 30);
}

void create_console(lv_obj_t* tab) {
    battery_label = make_label(tab, "BATTERY  -- mV", CYAN);
    lv_obj_set_pos(battery_label, 330, 8);

    console = lv_textarea_create(tab);
    lv_obj_set_pos(console, 12, 34);
    lv_obj_set_size(console, 440, 166);
    lv_textarea_set_text(console, "[BOOT] Team UI online\n[INFO] Waiting for match data...\n");
    lv_textarea_set_one_line(console, false);
    lv_obj_set_style_bg_color(console, CHARCOAL, LV_PART_MAIN);
    lv_obj_set_style_text_color(console, CYAN, LV_PART_MAIN);
    lv_obj_set_style_border_color(console, BROWN_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(console, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(console, 3, LV_PART_MAIN);
}

void update_timer(lv_timer_t*) {
    if (auton_locked.load() && controller_unlock_request.exchange(false)) {
        auton_locked.store(false);
        refresh_auton_buttons();
        refresh_alliance_buttons();
    }

    if (!auton_locked.load()) {
        const int delta = controller_auton_delta.exchange(0);
        if (delta != 0) {
            int next = selected_auton_index.load() + delta;
            if (next < 0) next = AUTON_COUNT - 1;
            if (next >= static_cast<int>(AUTON_COUNT)) next = 0;
            selected_auton_index.store(next);
            refresh_auton_buttons();
            update_simulation_path(next);
            lv_label_set_text_fmt(simulation_title, "PATH  /  %s", AUTON_NAMES[next]);
        }
        const int alliance_request = controller_alliance.exchange(-1);
        if (alliance_request >= 0) {
            alliance_index.store(alliance_request);
            refresh_alliance_buttons();
        }
        const int tab_delta = controller_tab_delta.exchange(0);
        if (tab_delta != 0 && tabview != nullptr) {
            int next_tab = static_cast<int>(lv_tabview_get_tab_active(tabview)) + tab_delta;
            if (next_tab < 0) next_tab = 3;
            if (next_tab > 3) next_tab = 0;
            lv_tabview_set_active(tabview, static_cast<uint32_t>(next_tab), LV_ANIM_OFF);
        }
        if (controller_lock_request.exchange(false)) {
            auton_locked.store(true);
            refresh_auton_buttons();
            refresh_alliance_buttons();
        }
    } else {
        controller_auton_delta.store(0);
        controller_alliance.store(-1);
        controller_tab_delta.store(0);
        controller_lock_request.store(false);
    }

    const float angle = gyro_angle.load();
    const int voltage = battery_mv.load();
    const bool aligned = angle > -2.0f && angle < 2.0f;

    if (angle_label != nullptr) {
        lv_label_set_text_fmt(angle_label, "GYRO  %+0.1f deg", static_cast<double>(angle));
    }
    if (battery_label != nullptr) {
        lv_label_set_text_fmt(battery_label, "BATTERY  %d mV", voltage);
    }
    if (alignment_led != nullptr) {
        lv_led_set_color(alignment_led, aligned ? CYAN : RED);
        if (aligned) {
            lv_led_on(alignment_led);
        } else {
            lv_led_off(alignment_led);
        }
    }
    if (console != nullptr) {
        lv_textarea_set_text(console, aligned ? "[OK] Alignment within +/-2 deg\n[INFO] Sensors nominal\n"
                                              : "[WARN] Square robot before start\n[INFO] Sensors nominal\n");
    }
}

void sensor_task(void*) {
    while (true) {
        gyro_angle.store(static_cast<float>(imu.get_rotation()));
        battery_mv.store(pros::battery::get_voltage());
        pros::delay(100);
    }
}

void controller_task(void*) {
    pros::Controller controller(pros::E_CONTROLLER_MASTER);
    bool unlock_chord_seen = false;
    bool start_chord_seen = false;
    while (true) {
        const bool unlock_chord = controller.get_digital(pros::E_CONTROLLER_DIGITAL_X) &&
                                  controller.get_digital(pros::E_CONTROLLER_DIGITAL_B);
        if (auton_locked.load() && unlock_chord && !unlock_chord_seen) {
            controller_unlock_request.store(true);
        }
        unlock_chord_seen = unlock_chord;

        const bool start_chord = controller.get_digital(pros::E_CONTROLLER_DIGITAL_DOWN) &&
                                 controller.get_digital(pros::E_CONTROLLER_DIGITAL_B);
        if (auton_locked.load() && start_chord && !start_chord_seen) {
            controller_manual_start_request.store(true);
        }
        start_chord_seen = start_chord;

        if (!auton_locked.load()) {
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_LEFT)) {
                controller_auton_delta.fetch_sub(1);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_RIGHT)) {
                controller_auton_delta.fetch_add(1);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_L1)) {
                controller_tab_delta.fetch_sub(1);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_R1)) {
                controller_tab_delta.fetch_add(1);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_UP)) {
                controller_alliance.store(0);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_DOWN)) {
                controller_alliance.store(1);
            }
            if (controller.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_A)) {
                controller_lock_request.store(true);
            }
        }
        pros::delay(20);
    }
}

}  // namespace

namespace team_ui {

void initialize() {
    lv_fs_stdio_init();

    lv_obj_t* screen = lv_screen_active();
    set_background(screen, CHARCOAL);

    tabview = lv_tabview_create(screen);
    lv_obj_set_size(tabview, 480, 240);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 32);

    lv_obj_t* selector_tab = lv_tabview_add_tab(tabview, "AUTON");
    lv_obj_t* simulation_tab = lv_tabview_add_tab(tabview, "SIM");
    lv_obj_t* tools_tab = lv_tabview_add_tab(tabview, "TOOLS");
    lv_obj_t* console_tab = lv_tabview_add_tab(tabview, "CONSOLE");

    create_auton_selector(selector_tab);
    create_simulation(simulation_tab);
    create_match_tools(tools_tab);
    create_console(console_tab);

    lv_timer_create(update_timer, 250, nullptr);

    static pros::Task sensors(sensor_task, nullptr, "ui sensors");
    static pros::Task controller_input(controller_task, nullptr, "ui controller");
    (void)sensors;
    (void)controller_input;
}

Alliance alliance() {
    return alliance_index.load() == 0 ? Alliance::RED : Alliance::BLUE;
}

int selected_auton() {
    return selected_auton_index.load();
}

bool is_locked() {
    return auton_locked.load();
}

void wait_for_lock() {
    while (!auton_locked.load()) {
        pros::delay(20);
    }
}

bool consume_manual_start_request() {
    return controller_manual_start_request.exchange(false);
}

}  // namespace team_ui

