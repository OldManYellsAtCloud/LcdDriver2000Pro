#ifndef LCD1602_H
#define LCD1602_H

#define LCD1602_DISPLAY_COMPATIBLE_STRING "gardenVariety,lcd1602"
#define LCD1602_NUMBER_OF_CTRLBITS 3
#define LCD1602_NUMBER_OF_DATABITS 8

#define LCD1602_DISABLE_GPIO 0
#define LCD1602_ENABLE_GPIO 1

#define LCD1602_CHAR_MAP_Y 16
#define LCD1602_CHAR_MAP_X 8

#define LCD1602_SHIFT_DECREMENT 0
#define LCD1602_SHIFT_INCREMENT 1

#define LCD1602_DISPLAY_SHIFT_DISABLE 0
#define LCD1602_DISPLAY_SHIFT_ENABLE 1

#define LCD1602_DISPLAY_OFF 0
#define LCD1602_DISPLAY_ON 1

#define LCD1602_CURSOR_OFF 0
#define LCD1602_CURSOR_ON 1

#define LCD1602_CURSOR_BLINK_OFF 0
#define LCD1602_CURSOR_BLINK_ON 0

#define LCD1602_CURSOR_MOVE 0
#define LCD1602_DISPLAY_SHIFT 1

#define LCD1602_SHIFT_TO_LEFT 0
#define LCD1602_SHIFT_TO_RIGHT 1

#define LCD1602_4BIT_INTERFACE 0
#define LCD1602_8BIT_INTERFACE 1

#define LCD1602_1_LINE 0
#define LCD1602_2_LINES 1

#define LCD1602_FONT_5X7 0
#define LCD1602_FONT_5X10 1

#define LCD1602_LINE_1_START_ADDRESS 0x0
#define LCD1602_LINE_2_START_ADDRESS 0x40

static unsigned long LCD1602_CLEAR_SCREEN = 0x1;
static unsigned long LCD1602_RETURN_HOME  = 0x2;
static unsigned long LCD1602_ENTRY_MODE_BASE = 0x4;
static unsigned long LCD1602_DISPLAY_CTRL_BASE = 0x8;
static unsigned long LCD1602_DISPLAY_SHIFT_BASE = 0x10;
static unsigned long LCD1602_DISPLAY_FUNCTION_BASE = 0x20;
static unsigned long LCD1602_SET_DD_RAM_BASE = 0x80;

// x index matches the 4 high bits' value of the matching character value in the display
// y index is the lower 4 bits
// 0-bytes can't be represented as a char. Will change once I manage to copy-paste them at least,
// but most of them are non-EU characters, so possibly I won't do that.
const char LCD1602_CHAR_MAP[LCD1602_CHAR_MAP_X][LCD1602_CHAR_MAP_Y] = {
    {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},
    {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},
    {' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/'},
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?'},
    {'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O'},
    {'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\0', ']', '^', '_'},
    {'\0', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o'},
    {'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '\0', '\0'}

};

struct lcd1602_gpiod_device {
    struct gpio_desc* registerSelectGpio;
    struct gpio_desc* rwGpio;
    struct gpio_desc* enableGpio;
    struct gpio_descs* dataBits;
};

#endif // LCD1602_H
