/*
 * Sony DualSense (PS5) controller - USB input report definitions
 *
 * Layout of the 64 byte USB input report (report id 0x01). This is the same
 * layout the Linux "hid-playstation" driver uses (offsets here include the
 * leading report id byte):
 *
 *   0       report id (0x01)
 *   1..4    left X, left Y, right X, right Y   (0-255, centre ~128, 0 = up)
 *   5..6    L2 / R2 analog trigger             (0-255)
 *   7       sequence counter
 *   8       D-pad hat (low nibble) + Square/Cross/Circle/Triangle (high nibble)
 *   9       L1 R1 L2 R2 Create Options L3 R3
 *   10      PS, Touchpad click, Mute
 *   11      reserved
 *   12..15  reserved
 *   16..21  gyro  X/Y/Z  (int16 LE)
 *   22..27  accel X/Y/Z  (int16 LE)
 *   28..31  sensor timestamp
 *   32      reserved
 *   33..36  touch point 0  (contact, x lo, x hi|y lo, y hi)
 *   37..40  touch point 1
 *   41..    reserved / battery status
 */

#ifndef DUALSENSE_H
#define DUALSENSE_H

#include <stdint.h>

// USB identifiers
#define DUALSENSE_VID               0x054C  // Sony Interactive Entertainment
#define DUALSENSE_PID               0x0CE6  // DualSense
#define DUALSENSE_PID_EDGE          0x0DF2  // DualSense Edge

// Standard DualSense USB layout (used if the descriptor can not be parsed)
#define DUALSENSE_USB_INTERFACE     3       // HID interface
#define DUALSENSE_USB_EP_IN         0x84
#define DUALSENSE_USB_EP_OUT        0x03

#define DUALSENSE_REPORT_SIZE       64
#define DUALSENSE_REPORT_ID_INPUT   0x01
#define DUALSENSE_REPORT_ID_OUTPUT  0x02
#define DUALSENSE_OUTPUT_SIZE       63      // USB output report 0x02 (incl. id)

// Offsets inside the input report
#define DS5_OFF_LX          1
#define DS5_OFF_LY          2
#define DS5_OFF_RX          3
#define DS5_OFF_RY          4
#define DS5_OFF_L2          5
#define DS5_OFF_R2          6
#define DS5_OFF_BTN0        8
#define DS5_OFF_BTN1        9
#define DS5_OFF_BTN2        10
#define DS5_OFF_GYRO        16
#define DS5_OFF_ACCEL       22
#define DS5_OFF_TOUCH0      33
#define DS5_OFF_TOUCH1      37

// Minimum number of bytes needed for buttons / sticks / triggers
#define DS5_MIN_BUTTONS_LEN 11
// Minimum number of bytes needed to also read both touch points
#define DS5_MIN_TOUCH_LEN   41

// buttons byte 0 (offset 8)
#define DS5_BTN0_HAT_MASK       0x0F    // 0-7 = N..NW clockwise, 8+ = released
#define DS5_BTN0_SQUARE         0x10
#define DS5_BTN0_CROSS          0x20
#define DS5_BTN0_CIRCLE         0x40
#define DS5_BTN0_TRIANGLE       0x80

// buttons byte 1 (offset 9)
#define DS5_BTN1_L1             0x01
#define DS5_BTN1_R1             0x02
#define DS5_BTN1_L2             0x04    // digital L2 (analog value is separate)
#define DS5_BTN1_R2             0x08
#define DS5_BTN1_CREATE         0x10    // PS4 "Share"
#define DS5_BTN1_OPTIONS        0x20
#define DS5_BTN1_L3             0x40
#define DS5_BTN1_R3             0x80

// buttons byte 2 (offset 10)
#define DS5_BTN2_PS             0x01
#define DS5_BTN2_TOUCHPAD       0x02
#define DS5_BTN2_MUTE           0x04

// Touch point contact byte: bit 7 set = finger NOT touching, bits 0-6 = id
#define DS5_TOUCH_INACTIVE      0x80

// Touchpad resolution of the DualSense (the DS4 pad is 1920 x 942)
#define DS5_TOUCH_MAX_X         1919
#define DS5_TOUCH_MAX_Y         1079
#define DS4_TOUCH_MAX_Y         941

// Sensor scaling (hid-playstation defaults, uncalibrated)
#define DS5_GYRO_PER_DEG_S      1024.0f
#define DS5_ACCEL_PER_G         8192.0f

// Passing raw (uncalibrated) motion data to games is OFF by default: wrong
// scaling is worse than neutral values for most titles. Build with
// -DDUALSENSE_PASS_MOTION=1 to experiment.
#ifndef DUALSENSE_PASS_MOTION
#define DUALSENSE_PASS_MOTION   0
#endif

typedef struct {
    uint8_t raw[DUALSENSE_REPORT_SIZE];
    uint8_t len;                // number of valid bytes received
} DualSenseReport;

static inline int is_dualsense_controller(uint16_t vid, uint16_t pid) {
    return vid == DUALSENSE_VID &&
           (pid == DUALSENSE_PID || pid == DUALSENSE_PID_EDGE);
}

static inline int dualsense_report_valid(const uint8_t* buf, int len) {
    return buf != 0 && len >= DS5_MIN_BUTTONS_LEN &&
           buf[0] == DUALSENSE_REPORT_ID_INPUT;
}

#endif // DUALSENSE_H
