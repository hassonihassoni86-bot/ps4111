/*
 * Xbox 360 to DualShock 4 Input Translator
 * Converts Xbox 360 USB reports to PS4 OrbisPadData format
 */

#include "translator.h"
#include "ini_parser.h"
#include <string.h>
#include <orbis/libkernel.h>

/*
 * Initialize translator with default configuration
 */
void translator_init(TranslatorConfig* config) {
    if (!config) return;

    config->stick_deadzone = (g_config.stick_deadzone > 0) ? g_config.stick_deadzone : DEFAULT_STICK_DEADZONE;
    config->trigger_threshold = (g_config.trigger_threshold > 0) ? g_config.trigger_threshold : DEFAULT_TRIGGER_THRESHOLD;
    config->invert_left_y = 1;      // PS4 Y-axis is inverted from Xbox
    config->invert_right_y = 1;
    if (!g_config.switch_positional) {
        config->swap_ab = 1;        // Map Nintendo letter A->Circle, B->Cross
        config->swap_xy = 1;
    } else {
        config->swap_ab = 0;
        config->swap_xy = 0;
    }
}

/*
 * Convert Xbox 360 16-bit signed stick value to DS4 8-bit unsigned
 *
 * Xbox:  -32768 to 32767, center = 0
 * DS4:   0 to 255, center = 128
 */
static inline uint8_t convert_stick_value(int16_t xbox_val) {
    int32_t temp = (int32_t)xbox_val + 32768;
    return (uint8_t)(temp >> 8);
}

/*
 * Apply deadzone to stick value
 */
uint8_t translator_apply_deadzone(uint8_t value, uint8_t deadzone) {
    if (deadzone == 0) return value;

    int16_t centered = (int16_t)value - 128;
    int16_t abs_centered = (centered < 0) ? -centered : centered;

    if (abs_centered <= deadzone) {
        return 128;
    }

    int16_t range = 127 - deadzone;
    int16_t scaled = ((abs_centered - deadzone) * 127) / range;

    if (centered < 0) {
        return (uint8_t)(128 - scaled);
    } else {
        return (uint8_t)(128 + scaled);
    }
}

/*
 * Main translation function
 */
void translator_convert(const Xbox360Report* xbox, OrbisPadData* ds4, const TranslatorConfig* config) {
    // Use default config if none provided
    TranslatorConfig default_config;
    if (!config) {
        translator_init(&default_config);
        config = &default_config;
    }

    // Clear output structure
    memset(ds4, 0, sizeof(OrbisPadData));

    // Get combined Xbox button state
    uint16_t xbox_buttons = xbox360_get_buttons(xbox);

    // ========================================
    // ANALOG STICKS (using OpenOrbis 'stick' struct)
    // ========================================

    uint8_t lx = convert_stick_value(xbox->left_stick_x);
    uint8_t ly = convert_stick_value(xbox->left_stick_y);
    uint8_t rx = convert_stick_value(xbox->right_stick_x);
    uint8_t ry = convert_stick_value(xbox->right_stick_y);

    // Apply Y-axis inversion
    if (config->invert_left_y) {
        ly = 255 - ly;
    }
    if (config->invert_right_y) {
        ry = 255 - ry;
    }

    // Apply deadzone
    if (config->stick_deadzone > 0) {
        lx = translator_apply_deadzone(lx, config->stick_deadzone);
        ly = translator_apply_deadzone(ly, config->stick_deadzone);
        rx = translator_apply_deadzone(rx, config->stick_deadzone);
        ry = translator_apply_deadzone(ry, config->stick_deadzone);
    }

    ds4->leftStick.x = lx;
    ds4->leftStick.y = ly;
    ds4->rightStick.x = rx;
    ds4->rightStick.y = ry;

    // ========================================
    // ANALOG TRIGGERS (using OpenOrbis 'analog' struct)
    // ========================================

    if (g_config.swap_triggers_bumpers) {
        ds4->analogButtons.l2 = (xbox_buttons & XBOX360_BTN_LB) ? 255 : 0;
        ds4->analogButtons.r2 = (xbox_buttons & XBOX360_BTN_RB) ? 255 : 0;
    } else {
        ds4->analogButtons.l2 = xbox->left_trigger;
        ds4->analogButtons.r2 = xbox->right_trigger;
    }

    // ========================================
    // DIGITAL BUTTONS
    // ========================================

    uint32_t ds4_buttons = 0;

    // Face buttons with optional swap
    if (config->swap_ab) {
        if (xbox_buttons & XBOX360_BTN_A) ds4_buttons |= DS4_BUTTON_CIRCLE;
        if (xbox_buttons & XBOX360_BTN_B) ds4_buttons |= DS4_BUTTON_CROSS;
    } else {
        if (xbox_buttons & XBOX360_BTN_A) ds4_buttons |= DS4_BUTTON_CROSS;
        if (xbox_buttons & XBOX360_BTN_B) ds4_buttons |= DS4_BUTTON_CIRCLE;
    }

    if (config->swap_xy) {
        if (xbox_buttons & XBOX360_BTN_X) ds4_buttons |= DS4_BUTTON_TRIANGLE;
        if (xbox_buttons & XBOX360_BTN_Y) ds4_buttons |= DS4_BUTTON_SQUARE;
    } else {
        if (xbox_buttons & XBOX360_BTN_X) ds4_buttons |= DS4_BUTTON_SQUARE;
        if (xbox_buttons & XBOX360_BTN_Y) ds4_buttons |= DS4_BUTTON_TRIANGLE;
    }

    // Shoulder buttons & digital triggers
    if (g_config.swap_triggers_bumpers) {
        if (xbox_buttons & XBOX360_BTN_LB) ds4_buttons |= DS4_BUTTON_L2;
        if (xbox_buttons & XBOX360_BTN_RB) ds4_buttons |= DS4_BUTTON_R2;
        if (xbox->left_trigger >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_L1;
        }
        if (xbox->right_trigger >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_R1;
        }
    } else {
        if (xbox_buttons & XBOX360_BTN_LB) ds4_buttons |= DS4_BUTTON_L1;
        if (xbox_buttons & XBOX360_BTN_RB) ds4_buttons |= DS4_BUTTON_R1;
        if (xbox->left_trigger >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_L2;
        }
        if (xbox->right_trigger >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_R2;
        }
    }

    // Stick click buttons
    if (xbox_buttons & XBOX360_BTN_L3) ds4_buttons |= DS4_BUTTON_L3;
    if (xbox_buttons & XBOX360_BTN_R3) ds4_buttons |= DS4_BUTTON_R3;

    // Menu buttons
    if (xbox_buttons & XBOX360_BTN_START) ds4_buttons |= DS4_BUTTON_OPTIONS;
    if (xbox_buttons & XBOX360_BTN_BACK)  ds4_buttons |= DS4_BUTTON_SHARE;

    // Guide button action
    if (xbox_buttons & XBOX360_BTN_GUIDE) {
        if (g_config.guide_action == CONFIG_GUIDE_TOUCHPAD) {
            ds4_buttons |= DS4_BUTTON_TOUCHPAD;
        } else if (g_config.guide_action == CONFIG_GUIDE_SHARE) {
            ds4_buttons |= DS4_BUTTON_SHARE;
        }
    }

    // ========================================
    // D-PAD
    // ========================================

    uint8_t xbox_dpad = xbox360_get_dpad(xbox);
    ds4_buttons |= dpad_bits_to_buttons(xbox_dpad);

    // Store final button state
    ds4->buttons = ds4_buttons;

    // ========================================
    // STATUS & METADATA
    // ========================================

    ds4->connected = 1;
    ds4->count = 1;
    if (ds4->timestamp == 0) {
        ds4->timestamp = sceKernelGetProcessTime();
    }

    // Motion data - set to neutral
    ds4->quat.x = 0.0f;
    ds4->quat.y = 0.0f;
    ds4->quat.z = 0.0f;
    ds4->quat.w = 1.0f;

    ds4->vel.x = 0.0f;
    ds4->vel.y = 0.0f;
    ds4->vel.z = 0.0f;

    ds4->acell.x = 0.0f;
    ds4->acell.y = 0.0f;
    ds4->acell.z = 1.0f;  // 1g downward

    // Touchpad - no touches
    ds4->touch.fingers = 0;
}

/*
 * Simple wrapper for translation with default config
 */
void xbox360_to_ds4(const Xbox360Report* xbox, OrbisPadData* ds4) {
    translator_convert(xbox, ds4, NULL);
}

/*
 * Xbox One translation function
 */
void translator_convert_xboxone(const XboxOneReport* xbox, OrbisPadData* ds4, const TranslatorConfig* config) {
    // Use default config if none provided
    TranslatorConfig default_config;
    if (!config) {
        translator_init(&default_config);
        config = &default_config;
    }

    // Clear output structure
    memset(ds4, 0, sizeof(OrbisPadData));

    // ========================================
    // ANALOG STICKS (same format as Xbox 360)
    // ========================================

    uint8_t lx = convert_stick_value(xbox->left_stick_x);
    uint8_t ly = convert_stick_value(xbox->left_stick_y);
    uint8_t rx = convert_stick_value(xbox->right_stick_x);
    uint8_t ry = convert_stick_value(xbox->right_stick_y);

    // Apply Y-axis inversion
    if (config->invert_left_y) {
        ly = 255 - ly;
    }
    if (config->invert_right_y) {
        ry = 255 - ry;
    }

    // Apply deadzone
    if (config->stick_deadzone > 0) {
        lx = translator_apply_deadzone(lx, config->stick_deadzone);
        ly = translator_apply_deadzone(ly, config->stick_deadzone);
        rx = translator_apply_deadzone(rx, config->stick_deadzone);
        ry = translator_apply_deadzone(ry, config->stick_deadzone);
    }

    ds4->leftStick.x = lx;
    ds4->leftStick.y = ly;
    ds4->rightStick.x = rx;
    ds4->rightStick.y = ry;

    // ========================================
    // ANALOG TRIGGERS (Xbox One uses 10-bit, convert to 8-bit)
    // ========================================

    uint8_t lt = xboxone_trigger_to_8bit(xbox->left_trigger);
    uint8_t rt = xboxone_trigger_to_8bit(xbox->right_trigger);

    if (g_config.swap_triggers_bumpers) {
        ds4->analogButtons.l2 = (xbox->buttons_high & XBOXONE_LB) ? 255 : 0;
        ds4->analogButtons.r2 = (xbox->buttons_high & XBOXONE_RB) ? 255 : 0;
    } else {
        ds4->analogButtons.l2 = lt;
        ds4->analogButtons.r2 = rt;
    }

    // ========================================
    // DIGITAL BUTTONS
    // ========================================

    uint32_t ds4_buttons = 0;

    // Face buttons (Xbox One layout: A/B/X/Y in buttons_low)
    if (config->swap_ab) {
        if (xbox->buttons_low & XBOXONE_A) ds4_buttons |= DS4_BUTTON_CIRCLE;
        if (xbox->buttons_low & XBOXONE_B) ds4_buttons |= DS4_BUTTON_CROSS;
    } else {
        if (xbox->buttons_low & XBOXONE_A) ds4_buttons |= DS4_BUTTON_CROSS;
        if (xbox->buttons_low & XBOXONE_B) ds4_buttons |= DS4_BUTTON_CIRCLE;
    }

    if (config->swap_xy) {
        if (xbox->buttons_low & XBOXONE_X) ds4_buttons |= DS4_BUTTON_TRIANGLE;
        if (xbox->buttons_low & XBOXONE_Y) ds4_buttons |= DS4_BUTTON_SQUARE;
    } else {
        if (xbox->buttons_low & XBOXONE_X) ds4_buttons |= DS4_BUTTON_SQUARE;
        if (xbox->buttons_low & XBOXONE_Y) ds4_buttons |= DS4_BUTTON_TRIANGLE;
    }

    // Shoulder buttons & digital triggers
    if (g_config.swap_triggers_bumpers) {
        if (xbox->buttons_high & XBOXONE_LB) ds4_buttons |= DS4_BUTTON_L2;
        if (xbox->buttons_high & XBOXONE_RB) ds4_buttons |= DS4_BUTTON_R2;
        if (lt >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_L1;
        }
        if (rt >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_R1;
        }
    } else {
        if (xbox->buttons_high & XBOXONE_LB) ds4_buttons |= DS4_BUTTON_L1;
        if (xbox->buttons_high & XBOXONE_RB) ds4_buttons |= DS4_BUTTON_R1;
        if (lt >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_L2;
        }
        if (rt >= config->trigger_threshold) {
            ds4_buttons |= DS4_BUTTON_R2;
        }
    }

    // Stick click buttons (in buttons_high)
    if (xbox->buttons_high & XBOXONE_LEFT_STICK)  ds4_buttons |= DS4_BUTTON_L3;
    if (xbox->buttons_high & XBOXONE_RIGHT_STICK) ds4_buttons |= DS4_BUTTON_R3;

    // Menu buttons (in buttons_low)
    if (xbox->buttons_low & XBOXONE_MENU) ds4_buttons |= DS4_BUTTON_OPTIONS;
    if (xbox->buttons_low & XBOXONE_VIEW) ds4_buttons |= DS4_BUTTON_SHARE;

    // Note: Xbox/Guide button comes in separate report (0x07), not handled here

    // ========================================
    // D-PAD (in buttons_high, same bit layout as Xbox 360)
    // ========================================

    uint8_t xbox_dpad = xboxone_get_dpad(xbox);
    ds4_buttons |= dpad_bits_to_buttons(xbox_dpad);

    // Store final button state
    ds4->buttons = ds4_buttons;

    // ========================================
    // STATUS & METADATA
    // ========================================

    ds4->connected = 1;
    ds4->count = 1;
    if (ds4->timestamp == 0) {
        ds4->timestamp = sceKernelGetProcessTime();
    }

    // Motion data - set to neutral
    ds4->quat.x = 0.0f;
    ds4->quat.y = 0.0f;
    ds4->quat.z = 0.0f;
    ds4->quat.w = 1.0f;

    ds4->vel.x = 0.0f;
    ds4->vel.y = 0.0f;
    ds4->vel.z = 0.0f;

    ds4->acell.x = 0.0f;
    ds4->acell.y = 0.0f;
    ds4->acell.z = 1.0f;  // 1g downward

    // Touchpad - no touches
    ds4->touch.fingers = 0;
}

/*
 * Simple wrapper for Xbox One translation with default config
 */
void xboxone_to_ds4(const XboxOneReport* xbox, OrbisPadData* ds4) {
    translator_convert_xboxone(xbox, ds4, NULL);
}

/*
 * Convert Switch hat value to DS4 d-pad buttons
 */
static uint32_t switch_hat_to_dpad(uint8_t hat) {
    switch (hat) {
        case SWITCH_HAT_UP:
            return DS4_BUTTON_DPAD_UP;
        case SWITCH_HAT_UP_RIGHT:
            return DS4_BUTTON_DPAD_UP | DS4_BUTTON_DPAD_RIGHT;
        case SWITCH_HAT_RIGHT:
            return DS4_BUTTON_DPAD_RIGHT;
        case SWITCH_HAT_DOWN_RIGHT:
            return DS4_BUTTON_DPAD_DOWN | DS4_BUTTON_DPAD_RIGHT;
        case SWITCH_HAT_DOWN:
            return DS4_BUTTON_DPAD_DOWN;
        case SWITCH_HAT_DOWN_LEFT:
            return DS4_BUTTON_DPAD_DOWN | DS4_BUTTON_DPAD_LEFT;
        case SWITCH_HAT_LEFT:
            return DS4_BUTTON_DPAD_LEFT;
        case SWITCH_HAT_UP_LEFT:
            return DS4_BUTTON_DPAD_UP | DS4_BUTTON_DPAD_LEFT;
        default:
            return 0;  // Centered or invalid
    }
}

/*
 * Switch Input-Only controller translation function
 */
void translator_convert_switch(const SwitchInputOnlyReport* sw, OrbisPadData* ds4, const TranslatorConfig* config) {
    // Use default config if none provided
    TranslatorConfig default_config;
    if (!config) {
        translator_init(&default_config);
        // Switch sticks don't need Y-axis inversion
        default_config.invert_left_y = 0;
        default_config.invert_right_y = 0;
        config = &default_config;
    }

    // Clear output structure
    memset(ds4, 0, sizeof(OrbisPadData));

    // ========================================
    // ANALOG STICKS (already 8-bit 0-255 format)
    // ========================================

    uint8_t lx = sw->left_stick_x;
    uint8_t ly = sw->left_stick_y;
    uint8_t rx = sw->right_stick_x;
    uint8_t ry = sw->right_stick_y;

    // Apply Y-axis inversion if configured
    if (config->invert_left_y) {
        ly = 255 - ly;
    }
    if (config->invert_right_y) {
        ry = 255 - ry;
    }

    // Apply deadzone
    if (config->stick_deadzone > 0) {
        lx = translator_apply_deadzone(lx, config->stick_deadzone);
        ly = translator_apply_deadzone(ly, config->stick_deadzone);
        rx = translator_apply_deadzone(rx, config->stick_deadzone);
        ry = translator_apply_deadzone(ry, config->stick_deadzone);
    }

    ds4->leftStick.x = lx;
    ds4->leftStick.y = ly;
    ds4->rightStick.x = rx;
    ds4->rightStick.y = ry;

    // ========================================
    // ANALOG TRIGGERS (Switch has digital only, fake analog)
    // ========================================

    if (g_config.swap_triggers_bumpers) {
        ds4->analogButtons.l2 = (sw->buttons0 & SWITCH_BTN_L) ? 255 : 0;
        ds4->analogButtons.r2 = (sw->buttons0 & SWITCH_BTN_R) ? 255 : 0;
    } else {
        ds4->analogButtons.l2 = (sw->buttons0 & SWITCH_BTN_ZL) ? 255 : 0;
        ds4->analogButtons.r2 = (sw->buttons0 & SWITCH_BTN_ZR) ? 255 : 0;
    }

    // ========================================
    // DIGITAL BUTTONS
    // ========================================

    uint32_t ds4_buttons = 0;

    // Face buttons - Switch uses Nintendo layout (A=East, B=South)
    // Map to PS4 positions: A->Circle, B->Cross, X->Triangle, Y->Square
    if (config->swap_ab) {
        if (sw->buttons0 & SWITCH_BTN_A) ds4_buttons |= DS4_BUTTON_CROSS;
        if (sw->buttons0 & SWITCH_BTN_B) ds4_buttons |= DS4_BUTTON_CIRCLE;
    } else {
        if (sw->buttons0 & SWITCH_BTN_A) ds4_buttons |= DS4_BUTTON_CIRCLE;
        if (sw->buttons0 & SWITCH_BTN_B) ds4_buttons |= DS4_BUTTON_CROSS;
    }

    if (config->swap_xy) {
        if (sw->buttons0 & SWITCH_BTN_X) ds4_buttons |= DS4_BUTTON_SQUARE;
        if (sw->buttons0 & SWITCH_BTN_Y) ds4_buttons |= DS4_BUTTON_TRIANGLE;
    } else {
        if (sw->buttons0 & SWITCH_BTN_X) ds4_buttons |= DS4_BUTTON_TRIANGLE;
        if (sw->buttons0 & SWITCH_BTN_Y) ds4_buttons |= DS4_BUTTON_SQUARE;
    }

    // Shoulder buttons & digital triggers
    if (g_config.swap_triggers_bumpers) {
        if (sw->buttons0 & SWITCH_BTN_L)  ds4_buttons |= DS4_BUTTON_L2;
        if (sw->buttons0 & SWITCH_BTN_R)  ds4_buttons |= DS4_BUTTON_R2;
        if (sw->buttons0 & SWITCH_BTN_ZL) ds4_buttons |= DS4_BUTTON_L1;
        if (sw->buttons0 & SWITCH_BTN_ZR) ds4_buttons |= DS4_BUTTON_R1;
    } else {
        if (sw->buttons0 & SWITCH_BTN_L)  ds4_buttons |= DS4_BUTTON_L1;
        if (sw->buttons0 & SWITCH_BTN_R)  ds4_buttons |= DS4_BUTTON_R1;
        if (sw->buttons0 & SWITCH_BTN_ZL) ds4_buttons |= DS4_BUTTON_L2;
        if (sw->buttons0 & SWITCH_BTN_ZR) ds4_buttons |= DS4_BUTTON_R2;
    }

    // Stick click buttons (byte 1)
    if (sw->buttons1 & SWITCH_BTN_L3) ds4_buttons |= DS4_BUTTON_L3;
    if (sw->buttons1 & SWITCH_BTN_R3) ds4_buttons |= DS4_BUTTON_R3;

    // Menu buttons (byte 1)
    if (sw->buttons1 & SWITCH_BTN_PLUS)  ds4_buttons |= DS4_BUTTON_OPTIONS;
    if (sw->buttons1 & SWITCH_BTN_MINUS) ds4_buttons |= DS4_BUTTON_SHARE;

    // Home button action
    if (sw->buttons1 & SWITCH_BTN_HOME) {
        if (g_config.guide_action == CONFIG_GUIDE_TOUCHPAD) {
            ds4_buttons |= DS4_BUTTON_TOUCHPAD;
        } else if (g_config.guide_action == CONFIG_GUIDE_SHARE) {
            ds4_buttons |= DS4_BUTTON_SHARE;
        }
    }

    // ========================================
    // D-PAD
    // ========================================

    ds4_buttons |= switch_hat_to_dpad(sw->hat);

    // Store final button state
    ds4->buttons = ds4_buttons;

    // ========================================
    // STATUS & METADATA
    // ========================================

    ds4->connected = 1;
    ds4->count = 1;
    if (ds4->timestamp == 0) {
        ds4->timestamp = sceKernelGetProcessTime();
    }

    // Motion data - set to neutral
    ds4->quat.x = 0.0f;
    ds4->quat.y = 0.0f;
    ds4->quat.z = 0.0f;
    ds4->quat.w = 1.0f;

    ds4->vel.x = 0.0f;
    ds4->vel.y = 0.0f;
    ds4->vel.z = 0.0f;

    ds4->acell.x = 0.0f;
    ds4->acell.y = 0.0f;
    ds4->acell.z = 1.0f;  // 1g downward

    // Touchpad - no touches
    ds4->touch.fingers = 0;
}

/*
 * Simple wrapper for Switch translation with default config
 */
void switch_to_ds4(const SwitchInputOnlyReport* sw, OrbisPadData* ds4) {
    translator_convert_switch(sw, ds4, NULL);
}

/*
 * ============================================================================
 * Sony DualSense (PS5) controller - USB report translation
 * ============================================================================
 *
 * The DualSense face buttons, sticks and touchpad already follow the DS4
 * layout, so there is no letter swap and no Y inversion here: 0 is "up" on
 * both pads.
 */
static inline __attribute__((unused)) int16_t ds5_s16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void translator_convert_dualsense(const DualSenseReport* ds, OrbisPadData* ds4, const TranslatorConfig* config) {
    TranslatorConfig default_config;
    if (!config) {
        translator_init(&default_config);
        config = &default_config;
    }

    memset(ds4, 0, sizeof(OrbisPadData));

    const uint8_t* r = ds->raw;
    if (ds->len < DS5_MIN_BUTTONS_LEN || r[0] != DUALSENSE_REPORT_ID_INPUT) {
        return;
    }

    // ========================================
    // ANALOG STICKS (8-bit, same orientation as DS4)
    // ========================================

    uint8_t lx = r[DS5_OFF_LX];
    uint8_t ly = r[DS5_OFF_LY];
    uint8_t rx = r[DS5_OFF_RX];
    uint8_t ry = r[DS5_OFF_RY];

    if (config->stick_deadzone > 0) {
        lx = translator_apply_deadzone(lx, config->stick_deadzone);
        ly = translator_apply_deadzone(ly, config->stick_deadzone);
        rx = translator_apply_deadzone(rx, config->stick_deadzone);
        ry = translator_apply_deadzone(ry, config->stick_deadzone);
    }

    ds4->leftStick.x = lx;
    ds4->leftStick.y = ly;
    ds4->rightStick.x = rx;
    ds4->rightStick.y = ry;

    // ========================================
    // TRIGGERS (analog value + digital button)
    // ========================================

    uint8_t l2 = r[DS5_OFF_L2];
    uint8_t r2 = r[DS5_OFF_R2];
    uint8_t b0 = r[DS5_OFF_BTN0];
    uint8_t b1 = r[DS5_OFF_BTN1];
    uint8_t b2 = r[DS5_OFF_BTN2];

    int l2_down = (l2 >= config->trigger_threshold) || (b1 & DS5_BTN1_L2);
    int r2_down = (r2 >= config->trigger_threshold) || (b1 & DS5_BTN1_R2);
    int l1_down = (b1 & DS5_BTN1_L1) != 0;
    int r1_down = (b1 & DS5_BTN1_R1) != 0;

    uint32_t buttons = 0;

    if (g_config.swap_triggers_bumpers) {
        // Bumpers act as triggers and the other way round
        ds4->analogButtons.l2 = l1_down ? 255 : 0;
        ds4->analogButtons.r2 = r1_down ? 255 : 0;
        if (l1_down) buttons |= DS4_BUTTON_L2;
        if (r1_down) buttons |= DS4_BUTTON_R2;
        if (l2_down) buttons |= DS4_BUTTON_L1;
        if (r2_down) buttons |= DS4_BUTTON_R1;
    } else {
        ds4->analogButtons.l2 = l2;
        ds4->analogButtons.r2 = r2;
        if (l1_down) buttons |= DS4_BUTTON_L1;
        if (r1_down) buttons |= DS4_BUTTON_R1;
        if (l2_down) buttons |= DS4_BUTTON_L2;
        if (r2_down) buttons |= DS4_BUTTON_R2;
    }

    // ========================================
    // FACE BUTTONS (positional, identical to DS4)
    // ========================================

    if (b0 & DS5_BTN0_CROSS)    buttons |= DS4_BUTTON_CROSS;
    if (b0 & DS5_BTN0_CIRCLE)   buttons |= DS4_BUTTON_CIRCLE;
    if (b0 & DS5_BTN0_SQUARE)   buttons |= DS4_BUTTON_SQUARE;
    if (b0 & DS5_BTN0_TRIANGLE) buttons |= DS4_BUTTON_TRIANGLE;

    // ========================================
    // MENU / STICK BUTTONS
    // ========================================

    if (b1 & DS5_BTN1_L3)      buttons |= DS4_BUTTON_L3;
    if (b1 & DS5_BTN1_R3)      buttons |= DS4_BUTTON_R3;
    if (b1 & DS5_BTN1_OPTIONS) buttons |= DS4_BUTTON_OPTIONS;
    if (b1 & DS5_BTN1_CREATE)  buttons |= DS4_BUTTON_SHARE;     // Create == Share

    // Real touchpad click
    if (b2 & DS5_BTN2_TOUCHPAD) buttons |= DS4_BUTTON_TOUCHPAD;

    // PS button: follows the same "guide_action" setting as the other pads
    // (the real PS button is handled by the system and can not be injected)
    if (b2 & DS5_BTN2_PS) {
        if (g_config.guide_action == CONFIG_GUIDE_TOUCHPAD) {
            buttons |= DS4_BUTTON_TOUCHPAD;
        } else if (g_config.guide_action == CONFIG_GUIDE_SHARE) {
            buttons |= DS4_BUTTON_SHARE;
        }
    }
    // Mute button: the PS4 has no equivalent, intentionally not mapped.

    // ========================================
    // D-PAD (hat switch 0-7, 8+ = released)
    // ========================================

    buttons |= switch_hat_to_dpad(b0 & DS5_BTN0_HAT_MASK);

    ds4->buttons = buttons;

    // ========================================
    // STATUS & METADATA
    // ========================================

    ds4->connected = 1;
    ds4->count = 1;
    ds4->timestamp = sceKernelGetProcessTime();

    // ========================================
    // MOTION (neutral unless explicitly enabled at build time)
    // ========================================

    ds4->quat.x = 0.0f;
    ds4->quat.y = 0.0f;
    ds4->quat.z = 0.0f;
    ds4->quat.w = 1.0f;

#if DUALSENSE_PASS_MOTION
    if (ds->len >= DS5_OFF_ACCEL + 6) {
        const float deg2rad = 0.01745329252f;
        ds4->vel.x = (float)ds5_s16(&r[DS5_OFF_GYRO + 0]) / DS5_GYRO_PER_DEG_S * deg2rad;
        ds4->vel.y = (float)ds5_s16(&r[DS5_OFF_GYRO + 2]) / DS5_GYRO_PER_DEG_S * deg2rad;
        ds4->vel.z = (float)ds5_s16(&r[DS5_OFF_GYRO + 4]) / DS5_GYRO_PER_DEG_S * deg2rad;
        ds4->acell.x = (float)ds5_s16(&r[DS5_OFF_ACCEL + 0]) / DS5_ACCEL_PER_G;
        ds4->acell.y = (float)ds5_s16(&r[DS5_OFF_ACCEL + 2]) / DS5_ACCEL_PER_G;
        ds4->acell.z = (float)ds5_s16(&r[DS5_OFF_ACCEL + 4]) / DS5_ACCEL_PER_G;
    } else
#endif
    {
        ds4->vel.x = 0.0f;
        ds4->vel.y = 0.0f;
        ds4->vel.z = 0.0f;
        ds4->acell.x = 0.0f;
        ds4->acell.y = 0.0f;
        ds4->acell.z = 1.0f;  // 1g downward
    }

    // ========================================
    // TOUCHPAD (up to two fingers)
    // ========================================

    ds4->touch.fingers = 0;
    if (ds->len >= DS5_MIN_TOUCH_LEN) {
        static const uint8_t offs[2] = { DS5_OFF_TOUCH0, DS5_OFF_TOUCH1 };
        uint8_t n = 0;
        for (int i = 0; i < 2; i++) {
            const uint8_t* t = &r[offs[i]];
            if (t[0] & DS5_TOUCH_INACTIVE) {
                continue;
            }
            uint32_t x = (uint32_t)t[1] | ((uint32_t)(t[2] & 0x0F) << 8);
            uint32_t y = (uint32_t)(t[2] >> 4) | ((uint32_t)t[3] << 4);
            if (x > DS5_TOUCH_MAX_X) x = DS5_TOUCH_MAX_X;
            if (y > DS5_TOUCH_MAX_Y) y = DS5_TOUCH_MAX_Y;
            // DS4 touchpad is shorter (942 rows) than the DualSense one (1080)
            y = (y * DS4_TOUCH_MAX_Y) / DS5_TOUCH_MAX_Y;

            ds4->touch.touch[n].x = (uint16_t)x;
            ds4->touch.touch[n].y = (uint16_t)y;
            ds4->touch.touch[n].finger = (uint8_t)(t[0] & 0x7F);
            n++;
        }
        ds4->touch.fingers = n;
    }
}

void dualsense_to_ds4(const DualSenseReport* ds, OrbisPadData* ds4) {
    translator_convert_dualsense(ds, ds4, NULL);
}
