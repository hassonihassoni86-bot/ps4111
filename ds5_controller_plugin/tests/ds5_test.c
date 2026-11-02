#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "translator.h"
#include "ini_parser.h"

PluginConfig g_config;

/* same defaults as config_init_defaults() in src/config.c (that file needs
   the PS4 kernel/log APIs, so it is not linked into the host test) */
void config_init_defaults(PluginConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->mode = CONFIG_MODE_AUTO;
    cfg->guide_action = CONFIG_GUIDE_TOUCHPAD;
    cfg->enable_notifications = 1; cfg->enable_mode_combo = 1;
    cfg->enable_rumble = 1; cfg->enable_homebrew = 1;
    cfg->stick_deadzone = DEFAULT_STICK_DEADZONE;
    cfg->trigger_threshold = DEFAULT_TRIGGER_THRESHOLD;
    cfg->switch_positional = 1;
}

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("ok:   %s\n", msg); } } while (0)

static void neutral(DualSenseReport* r) {
    memset(r, 0, sizeof(*r));
    r->raw[0] = 0x01;
    r->raw[DS5_OFF_LX] = r->raw[DS5_OFF_LY] = r->raw[DS5_OFF_RX] = r->raw[DS5_OFF_RY] = 128;
    r->raw[DS5_OFF_BTN0] = 0x08;           // hat released
    r->raw[DS5_OFF_TOUCH0] = r->raw[DS5_OFF_TOUCH1] = 0x80;
    r->len = 64;
}

int main(void) {
    config_init_defaults(&g_config);
    DualSenseReport r; OrbisPadData o;

    neutral(&r); dualsense_to_ds4(&r, &o);
    CHECK(o.buttons == 0, "neutral: no buttons");
    CHECK(o.leftStick.x == 128 && o.leftStick.y == 128 && o.rightStick.x == 128 && o.rightStick.y == 128, "neutral: sticks centred");
    CHECK(o.connected == 1 && o.count == 1, "neutral: connected");
    CHECK(o.touch.fingers == 0, "neutral: no touch");

    struct { int off; uint8_t mask; uint32_t expect; const char* n; } t[] = {
        {DS5_OFF_BTN0, DS5_BTN0_CROSS,    DS4_BUTTON_CROSS,    "Cross"},
        {DS5_OFF_BTN0, DS5_BTN0_CIRCLE,   DS4_BUTTON_CIRCLE,   "Circle"},
        {DS5_OFF_BTN0, DS5_BTN0_SQUARE,   DS4_BUTTON_SQUARE,   "Square"},
        {DS5_OFF_BTN0, DS5_BTN0_TRIANGLE, DS4_BUTTON_TRIANGLE, "Triangle"},
        {DS5_OFF_BTN1, DS5_BTN1_L1,       DS4_BUTTON_L1,       "L1"},
        {DS5_OFF_BTN1, DS5_BTN1_R1,       DS4_BUTTON_R1,       "R1"},
        {DS5_OFF_BTN1, DS5_BTN1_L2,       DS4_BUTTON_L2,       "L2 digital"},
        {DS5_OFF_BTN1, DS5_BTN1_R2,       DS4_BUTTON_R2,       "R2 digital"},
        {DS5_OFF_BTN1, DS5_BTN1_CREATE,   DS4_BUTTON_SHARE,    "Create->Share"},
        {DS5_OFF_BTN1, DS5_BTN1_OPTIONS,  DS4_BUTTON_OPTIONS,  "Options"},
        {DS5_OFF_BTN1, DS5_BTN1_L3,       DS4_BUTTON_L3,       "L3"},
        {DS5_OFF_BTN1, DS5_BTN1_R3,       DS4_BUTTON_R3,       "R3"},
        {DS5_OFF_BTN2, DS5_BTN2_TOUCHPAD, DS4_BUTTON_TOUCHPAD, "Touchpad click"},
        {DS5_OFF_BTN2, DS5_BTN2_PS,       DS4_BUTTON_TOUCHPAD, "PS->guide(touchpad)"},
    };
    for (unsigned i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        neutral(&r); r.raw[t[i].off] |= t[i].mask; dualsense_to_ds4(&r, &o);
        char m[64]; snprintf(m, sizeof m, "button %s", t[i].n);
        CHECK(o.buttons == t[i].expect, m);
    }

    neutral(&r); r.raw[DS5_OFF_BTN2] = DS5_BTN2_MUTE; dualsense_to_ds4(&r, &o);
    CHECK(o.buttons == 0, "Mute is not mapped");

    g_config.guide_action = CONFIG_GUIDE_SHARE;
    neutral(&r); r.raw[DS5_OFF_BTN2] = DS5_BTN2_PS; dualsense_to_ds4(&r, &o);
    CHECK(o.buttons == DS4_BUTTON_SHARE, "PS->guide(share) when configured");
    g_config.guide_action = CONFIG_GUIDE_NONE;
    dualsense_to_ds4(&r, &o);
    CHECK(o.buttons == 0, "PS->nothing when guide_action=none");
    g_config.guide_action = CONFIG_GUIDE_TOUCHPAD;

    uint32_t hat[8] = {
        DS4_BUTTON_DPAD_UP, DS4_BUTTON_DPAD_UP|DS4_BUTTON_DPAD_RIGHT, DS4_BUTTON_DPAD_RIGHT,
        DS4_BUTTON_DPAD_DOWN|DS4_BUTTON_DPAD_RIGHT, DS4_BUTTON_DPAD_DOWN,
        DS4_BUTTON_DPAD_DOWN|DS4_BUTTON_DPAD_LEFT, DS4_BUTTON_DPAD_LEFT,
        DS4_BUTTON_DPAD_UP|DS4_BUTTON_DPAD_LEFT };
    for (int h = 0; h < 8; h++) {
        neutral(&r); r.raw[DS5_OFF_BTN0] = (uint8_t)h; dualsense_to_ds4(&r, &o);
        char m[32]; snprintf(m, sizeof m, "hat %d", h);
        CHECK(o.buttons == hat[h], m);
    }
    neutral(&r); r.raw[DS5_OFF_BTN0] = DS5_BTN0_CROSS | 2; dualsense_to_ds4(&r, &o);
    CHECK(o.buttons == (DS4_BUTTON_CROSS|DS4_BUTTON_DPAD_RIGHT), "cross + right together");

    neutral(&r); r.raw[DS5_OFF_L2] = 200; r.raw[DS5_OFF_R2] = 10; dualsense_to_ds4(&r, &o);
    CHECK(o.analogButtons.l2 == 200 && o.analogButtons.r2 == 10, "analog triggers passed through");
    CHECK((o.buttons & DS4_BUTTON_L2) && !(o.buttons & DS4_BUTTON_R2), "trigger threshold -> digital L2 only");

    g_config.swap_triggers_bumpers = 1;
    neutral(&r); r.raw[DS5_OFF_BTN1] = DS5_BTN1_L1; dualsense_to_ds4(&r, &o);
    CHECK((o.buttons & DS4_BUTTON_L2) && o.analogButtons.l2 == 255, "swap: L1 acts as L2");
    g_config.swap_triggers_bumpers = 0;

    neutral(&r); r.raw[DS5_OFF_LX] = 0; r.raw[DS5_OFF_LY] = 0; r.raw[DS5_OFF_RX] = 255; r.raw[DS5_OFF_RY] = 255; dualsense_to_ds4(&r, &o);
    CHECK(o.leftStick.x == 1 || o.leftStick.x == 0, "left stick full left");
    CHECK(o.leftStick.y <= 1, "left stick up stays 0 (no inversion)");
    CHECK(o.rightStick.x >= 254 && o.rightStick.y >= 254, "right stick full right/down");

    neutral(&r); r.raw[DS5_OFF_LX] = 128 + 5; dualsense_to_ds4(&r, &o);
    CHECK(o.leftStick.x == 128, "small deflection inside deadzone is zeroed");

    // touch: finger 0 id 5 at x=1000,y=540 ; finger 1 id 6 at x=1919,y=1079
    neutral(&r);
    r.raw[DS5_OFF_TOUCH0+0] = 5;
    r.raw[DS5_OFF_TOUCH0+1] = 1000 & 0xFF;
    r.raw[DS5_OFF_TOUCH0+2] = ((1000 >> 8) & 0x0F) | ((540 & 0x0F) << 4);
    r.raw[DS5_OFF_TOUCH0+3] = 540 >> 4;
    r.raw[DS5_OFF_TOUCH1+0] = 6;
    r.raw[DS5_OFF_TOUCH1+1] = 1919 & 0xFF;
    r.raw[DS5_OFF_TOUCH1+2] = ((1919 >> 8) & 0x0F) | ((1079 & 0x0F) << 4);
    r.raw[DS5_OFF_TOUCH1+3] = 1079 >> 4;
    dualsense_to_ds4(&r, &o);
    CHECK(o.touch.fingers == 2, "two fingers detected");
    CHECK(o.touch.touch[0].x == 1000 && o.touch.touch[0].finger == 5, "finger0 x/id");
    CHECK(o.touch.touch[0].y == (540*941)/1079, "finger0 y scaled to DS4 range");
    CHECK(o.touch.touch[1].x == 1919 && o.touch.touch[1].y == 941 && o.touch.touch[1].finger == 6, "finger1 corner");

    neutral(&r); r.raw[DS5_OFF_TOUCH1] = 7; // only second finger down
    r.raw[DS5_OFF_TOUCH1+1] = 100;
    dualsense_to_ds4(&r, &o);
    CHECK(o.touch.fingers == 1 && o.touch.touch[0].finger == 7, "second finger alone is compacted to slot 0");

    // validity / short reports
    uint8_t bad[64] = {0x31};
    CHECK(!dualsense_report_valid(bad, 64), "bluetooth report id rejected");
    uint8_t good[64] = {0x01};
    CHECK(dualsense_report_valid(good, 64) && !dualsense_report_valid(good, 5), "short report rejected");
    neutral(&r); r.len = 11; dualsense_to_ds4(&r, &o);
    CHECK(o.touch.fingers == 0, "short report: touch skipped safely");

    printf("\n%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
