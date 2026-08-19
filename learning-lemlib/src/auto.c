#include "pal/auto.h"
#include "liblvgl/core/lv_obj.h"
#include "liblvgl/core/lv_obj_event.h"
#include "liblvgl/core/lv_obj_pos.h"
#include "liblvgl/misc/lv_color.h"
#include "liblvgl/widgets/button/lv_button.h"
#include "liblvgl/widgets/image/lv_image.h"
#include "liblvgl/widgets/label/lv_label.h"

extern const lv_image_dsc_t field;

static auto_pos_t active_pos = AUTO_POS_SKILLS;
static auto_color_t active_color = AUTO_COLOR_SKILLS;
static const auto_routine_t *local_auto_list;
static size_t local_auto_length;
static int active_auto = -1;
static auto_func_t local_pre_auto;

static const auto_pos_t active_pos_mode[5] = {
    AUTO_POS_1, AUTO_POS_2, AUTO_POS_1, AUTO_POS_2, AUTO_POS_SKILLS
};
static const auto_color_t active_color_mode[5] = {
    AUTO_COLOR_RED, AUTO_COLOR_RED, AUTO_COLOR_BLUE, AUTO_COLOR_BLUE, AUTO_COLOR_SKILLS
};
static const char *const position_labels[5] = {
    "Red 1", "Red 2", "Blue 1", "Blue 2", "Skills"
};
static const lv_align_t position_alignments[5] = {
    LV_ALIGN_LEFT_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_RIGHT_MID,
    LV_ALIGN_TOP_MID, LV_ALIGN_CENTER
};

#define LIST_SIZE 8

static lv_obj_t *field_image;
static lv_obj_t *position_buttons[5];
static lv_obj_t *routine_buttons[LIST_SIZE];

static lv_color_t color_for_mode(auto_color_t color)
{
    if (color == AUTO_COLOR_RED) return lv_color_hex(0xe53935);
    if (color == AUTO_COLOR_BLUE) return lv_color_hex(0x1e88e5);
    return lv_color_hex(0xf6c344);
}

static void set_button_style(lv_obj_t *button, lv_color_t color, bool active)
{
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, active ? 5 : 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(button, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void set_routine_style(lv_obj_t *button, bool active, bool pre_auto)
{
    lv_color_t color = pre_auto
        ? (active ? lv_color_hex(0xff9800) : lv_color_hex(0xe040fb))
        : lv_color_hex(0x7cb342);
    set_button_style(button, color, active);
}

static void delete_routine_buttons(void)
{
    for (int i = 0; i < LIST_SIZE; ++i) {
        if (routine_buttons[i]) lv_obj_delete(routine_buttons[i]);
        routine_buttons[i] = NULL;
    }
}

static void routine_button_event(lv_event_t *event)
{
    lv_obj_t *button = lv_event_get_target_obj(event);
    int index = -1;
    for (int i = 0; i < LIST_SIZE; ++i) {
        if (routine_buttons[i] == button) {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    if (local_pre_auto && index == LIST_SIZE - 1) {
        set_routine_style(button, true, true);
        local_pre_auto(active_color, active_pos);
        set_routine_style(button, false, true);
        return;
    }

    for (int i = 0; i < LIST_SIZE; ++i) {
        if (routine_buttons[i]) set_routine_style(routine_buttons[i], i == index, false);
    }

    int matching_index = 0;
    for (size_t i = 0; i < local_auto_length; ++i) {
        if (local_auto_list[i].position != active_pos) continue;
        if (matching_index++ == index) {
            active_auto = (int)i;
            return;
        }
    }
}

static void create_routine_buttons(void)
{
    delete_routine_buttons();

    int valid_routines = 0;
    for (size_t i = 0; i < local_auto_length && valid_routines < LIST_SIZE; ++i) {
        if (local_auto_list[i].position == active_pos) ++valid_routines;
    }
    if (local_pre_auto && valid_routines == LIST_SIZE) --valid_routines;

    lv_obj_t *screen = lv_screen_active();
    int button_width = lv_obj_get_width(screen) - 240;
    int button_height = lv_obj_get_height(screen) / LIST_SIZE;
    for (int i = 0; i < valid_routines; ++i) {
        int routine_index = 0;
        const char *name = "";
        for (size_t j = 0; j < local_auto_length; ++j) {
            if (local_auto_list[j].position == active_pos && routine_index++ == i) {
                name = local_auto_list[j].name;
                break;
            }
        }
        routine_buttons[i] = lv_button_create(screen);
        lv_obj_set_size(routine_buttons[i], button_width, button_height);
        lv_obj_align(routine_buttons[i], LV_ALIGN_TOP_RIGHT, 0, i * button_height);
        set_routine_style(routine_buttons[i], false, false);
        lv_obj_add_event_cb(routine_buttons[i], routine_button_event, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(routine_buttons[i]);
        lv_label_set_text(label, name);
        lv_obj_center(label);
    }

    if (local_pre_auto) {
        routine_buttons[LIST_SIZE - 1] = lv_button_create(screen);
        lv_obj_set_size(routine_buttons[LIST_SIZE - 1], button_width, button_height);
        lv_obj_align(routine_buttons[LIST_SIZE - 1], LV_ALIGN_TOP_RIGHT, 0, (LIST_SIZE - 1) * button_height);
        set_routine_style(routine_buttons[LIST_SIZE - 1], false, true);
        lv_obj_add_event_cb(routine_buttons[LIST_SIZE - 1], routine_button_event, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(routine_buttons[LIST_SIZE - 1]);
        lv_label_set_text(label, "Pre-Auto Routine");
        lv_obj_center(label);
    }
}

static void position_button_event(lv_event_t *event)
{
    lv_obj_t *button = lv_event_get_target_obj(event);
    int index = -1;
    for (int i = 0; i < 5; ++i) {
        if (position_buttons[i] == button) {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    active_pos = active_pos_mode[index];
    active_color = active_color_mode[index];
    for (int i = 0; i < 5; ++i) {
        set_button_style(position_buttons[i], color_for_mode(active_color_mode[i]), i == index);
    }
    create_routine_buttons();
}

void auto_picker(const auto_routine_t *list, size_t length)
{
    local_auto_list = list;
    local_auto_length = length;
    active_auto = -1;

    field_image = lv_image_create(lv_screen_active());
    lv_image_set_src(field_image, &field);
    lv_obj_set_size(field_image, 240, 240);
    lv_obj_align(field_image, LV_ALIGN_TOP_LEFT, 0, 0);

    for (int i = 0; i < 5; ++i) {
        position_buttons[i] = lv_button_create(lv_screen_active());
        lv_obj_set_size(position_buttons[i], 66, 66);
        lv_obj_align_to(position_buttons[i], field_image, position_alignments[i], 0, 0);
        set_button_style(position_buttons[i], color_for_mode(active_color_mode[i]), i == 0);
        lv_obj_add_event_cb(position_buttons[i], position_button_event, LV_EVENT_CLICKED, NULL);
        lv_obj_t *label = lv_label_create(position_buttons[i]);
        lv_label_set_text(label, position_labels[i]);
        lv_obj_center(label);
    }
}

void auto_pre_auto(auto_func_t pre_auto)
{
    local_pre_auto = pre_auto;
    create_routine_buttons();
}

void auto_clean(void)
{
    delete_routine_buttons();
    for (int i = 0; i < 5; ++i) {
        if (position_buttons[i]) lv_obj_delete(position_buttons[i]);
        position_buttons[i] = NULL;
    }
    if (field_image) lv_obj_delete(field_image);
    field_image = NULL;
}

void auto_run(void)
{
    if (active_auto < 0 || (size_t)active_auto >= local_auto_length) return;
    if (local_auto_list[active_auto].function) {
        local_auto_list[active_auto].function(active_color, active_pos);
    }
}

auto_color_t auto_get_color(void) { return active_color; }
auto_pos_t auto_get_pos(void) { return active_pos; }
int auto_get_active(void) { return active_auto; }
