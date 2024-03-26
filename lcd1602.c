#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>

#include <linux/device/driver.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>

#include <linux/delay.h>

#include "lcd1602.h"

static int lcd1602_font_size = 7;
static int lcd1602_line_no = 2;

module_param(lcd1602_font_size, int, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(lcd1602_font_size, "Font size to use for the initial configuration. Valid values: 7, 10");
module_param(lcd1602_line_no, int, S_IRUSR | S_IRGRP);
MODULE_PARM_DESC(lcd1602_line_no, "Number of lines to use for the initial configuration. Valid values: 1, 2");

static int lcd1602_device_major;
static struct class* lcd1602_cls;

static struct lcd1602_gpiod_device lcd1602_gpio_dev;

static void lcd1602_set_rw_and_rs(int rw_val, int rs_val){
    gpiod_set_value(lcd1602_gpio_dev.rwGpio, rw_val);
    gpiod_set_value(lcd1602_gpio_dev.registerSelectGpio, rs_val);
}

static int lcd1602_wait_for_non_busy(void){
    int ret, orig_register_select_value, orig_rw_value;
    uint8_t timeout_counter = 165;

    orig_register_select_value = gpiod_get_value(lcd1602_gpio_dev.registerSelectGpio);
    orig_rw_value = gpiod_get_value(lcd1602_gpio_dev.rwGpio);

    ret = gpiod_direction_input(lcd1602_gpio_dev.dataBits->desc[7]);

    if (ret != 0){
        pr_err("Could not switch db7 to input!\n");
        return ret;
    }

    lcd1602_set_rw_and_rs(LCD1602_ENABLE_GPIO, LCD1602_DISABLE_GPIO);

    // based on datasheet none of the commands take more than 1640 us to execute (after bootup)
    // Here wait up to 1650us, or until the busy bit is low.
    while (timeout_counter-- && gpiod_get_value(lcd1602_gpio_dev.dataBits->desc[7]))
        udelay(10);

    lcd1602_set_rw_and_rs(orig_rw_value, orig_register_select_value);

    ret = gpiod_direction_output(lcd1602_gpio_dev.dataBits->desc[7], 0);
    if (ret != 0){
        pr_emerg("Could not switch db7 back to input! Possibly everything is broken from here\n");
        return ret;
    }

    if (timeout_counter == 0)
        return -EBUSY;

    return ret;
}

static int lcd1602_set_gpio_array_value(unsigned long* value){
    return gpiod_set_array_value(LCD1602_NUMBER_OF_DATABITS, lcd1602_gpio_dev.dataBits->desc, lcd1602_gpio_dev.dataBits->info, value);
}

static void lcd1602_enable_signal(void){
    gpiod_set_value(lcd1602_gpio_dev.enableGpio, 1);
    mdelay(1);
    gpiod_set_value(lcd1602_gpio_dev.enableGpio, 0);
}

static void lcd1602_get_ctrl_gpio(struct gpio_desc** dest, unsigned int index, struct device* dev){
    *dest = gpiod_get_index(dev, "ctrl", index, GPIOD_OUT_LOW);
    if (IS_ERR(dest)){
        pr_err("Could not get GPIO index %d. Error: %li\n", index, PTR_ERR(dest));
    }
}

static void lcd1602_get_display_gpios(struct gpio_descs** dest, struct device* dev){
    *dest = gpiod_get_array(dev, "display", GPIOD_OUT_LOW);
    if (IS_ERR(dest)){
        pr_err("Could not get GPIO array. Error: %li\n", PTR_ERR(dest));
    }
}

static int lcd1602_verify_dt(struct device* dev){
    struct device_node *dn = of_find_compatible_node(NULL, NULL, LCD1602_DISPLAY_COMPATIBLE_STRING);
    int n;

    if (!dn){
        pr_err("Could not find root node!\n");
        return -ENODEV;
    }

    n = gpiod_count(dev, "display");
    if (n != LCD1602_NUMBER_OF_DATABITS){
        pr_err("Incorrect device tree. Expected %d display-gpios, but found only %d.\n", LCD1602_NUMBER_OF_DATABITS, n);
        return -ENODEV;
    }

    n = gpiod_count(dev, "ctrl");
    if (n != LCD1602_NUMBER_OF_CTRLBITS){
        pr_err("Incorrect device tree. Expected %d ctrl-gpios, but found only %d.\n", LCD1602_NUMBER_OF_CTRLBITS, n);
        return -ENODEV;
    }
    return 0;
}

static unsigned long lcd1602_get_char_value(char c){
    unsigned long ret = 0x20; // space
    size_t x, y;
    // the first 2 lines are only nulls. Start the search at the 3rd line.
    for (x = 2; x < LCD1602_CHAR_MAP_X; ++x){
        for (y = 0; y < LCD1602_CHAR_MAP_Y; ++y){
            if (LCD1602_CHAR_MAP[x][y] == c){
                ret = x << 4;
                ret |= y;
                goto done;
            }
        }
    }

done:
    return ret;
}

static void lcd1602_send_command(unsigned long cmd){
    lcd1602_wait_for_non_busy();
    lcd1602_set_gpio_array_value(&cmd);
    lcd1602_enable_signal();
}

static void lcd1602_reset_screen(void){
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(LCD1602_CLEAR_SCREEN);
}

static void lcd1602_return_home(void){
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(LCD1602_RETURN_HOME);
}

static void lcd1602_set_entry_mode(uint8_t cursor_move, uint8_t display_shift){
    unsigned long value = LCD1602_ENTRY_MODE_BASE;
    value |= (cursor_move << 1);
    value |= display_shift;
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_display_control(uint8_t display_on_off, uint8_t cursor_on_off, uint8_t cursor_blink){
    unsigned long value = LCD1602_DISPLAY_CTRL_BASE;
    value |= (display_on_off << 2);
    value |= (cursor_on_off << 1);
    value |= cursor_on_off;
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_cursor_display_shift(uint8_t shift_mode, uint8_t shift_direction){
    unsigned long value = LCD1602_DISPLAY_SHIFT_BASE;
    value |= (shift_mode << 3);
    value |= (shift_direction << 2);
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_function_set(uint8_t data_length, uint8_t line_no, uint8_t font_size){
    unsigned long value = LCD1602_DISPLAY_FUNCTION_BASE;
    value |= (data_length << 4);
    value |= (line_no << 3);
    value |= (font_size << 2);
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_display_char(char c){
    unsigned long value = lcd1602_get_char_value(c);
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_ENABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_switch_line(uint8_t line){
    unsigned long value = LCD1602_SET_DD_RAM_BASE;
    if (line == 1){
        value |= LCD1602_LINE_1_START_ADDRESS;
    } else {
        value |= LCD1602_LINE_2_START_ADDRESS;
    }
    lcd1602_set_rw_and_rs(LCD1602_DISABLE_GPIO, LCD1602_DISABLE_GPIO);
    lcd1602_send_command(value);
}

static void lcd1602_display_text(char* text){
    size_t i;
    lcd1602_reset_screen();
    for (i = 0; i < strlen(text); ++i){
        if (text[i] == '\n' || text[i] == '\r'){
            lcd1602_switch_line(2);
        } else {
            lcd1602_display_char(text[i]);
        }
    }
}

static ssize_t lcd1602_device_write (struct file* fp, const char __user * ch, size_t sz, loff_t * off){
    int ret;
    char buff[64];

    if (sz > 63)
        return -EMSGSIZE;

    ret = copy_from_user(buff, ch, sz);

    if (ret != 0){
        pr_alert("copy from user failed: %d", ret);
        return 0;
    }
    buff[sz] = '\0';
    lcd1602_display_text(buff);
    return sz;
}

static int lcd1602_device_open (struct inode* in, struct file* f){
    try_module_get(THIS_MODULE);
    return 0;
}

static int lcd1602_device_release (struct inode* in, struct file* f){
    module_put(THIS_MODULE);
    return 0;
}

static struct file_operations lcd1602_fops = {
    .write = lcd1602_device_write,
    .open = lcd1602_device_open,
    .release = lcd1602_device_release,
    .owner = THIS_MODULE
};

static void lcd1602_setup_userspace(struct platform_device* pdev){
    lcd1602_device_major = register_chrdev(0, pdev->name, &lcd1602_fops);
    if (lcd1602_device_major < 0){
        pr_alert("Could not register device: %d\n", lcd1602_device_major);
        return;
    }

    lcd1602_cls = class_create(pdev->name);
    device_create(lcd1602_cls, NULL, MKDEV(lcd1602_device_major, 0), NULL, pdev->name);
}

static void lcd1602_setup_display(struct platform_device* pdev){
    struct device *dev = &pdev->dev;
    int font_size, line_no;
    font_size = lcd1602_font_size == 7 ? LCD1602_FONT_5X7 : LCD1602_FONT_5X10;
    line_no = lcd1602_line_no == 1 ? LCD1602_1_LINE : LCD1602_2_LINES;

    lcd1602_verify_dt(dev);

    lcd1602_get_ctrl_gpio(&lcd1602_gpio_dev.registerSelectGpio, 0, dev);
    lcd1602_get_ctrl_gpio(&lcd1602_gpio_dev.rwGpio, 1, dev);
    lcd1602_get_ctrl_gpio(&lcd1602_gpio_dev.enableGpio, 2, dev);

    lcd1602_get_display_gpios(&lcd1602_gpio_dev.dataBits, dev);

    lcd1602_reset_screen();
    lcd1602_function_set(LCD1602_8BIT_INTERFACE, line_no, font_size);
    lcd1602_display_control(LCD1602_DISPLAY_OFF, LCD1602_CURSOR_ON, LCD1602_CURSOR_BLINK_ON);
    lcd1602_display_control(LCD1602_DISPLAY_ON, LCD1602_CURSOR_ON, LCD1602_CURSOR_BLINK_ON);
    lcd1602_set_entry_mode(LCD1602_DISPLAY_SHIFT, LCD1602_SHIFT_TO_LEFT);
}


static int lcd1602_probe(struct platform_device* pdev){
    //void* pdata = platform_get_drvdata(pdev);
    lcd1602_setup_display(pdev);
    lcd1602_setup_userspace(pdev);

    return 0;
}

static void lcd1602_remove(struct platform_device* pdev){
    lcd1602_reset_screen();
    gpiod_put(lcd1602_gpio_dev.registerSelectGpio);
    gpiod_put(lcd1602_gpio_dev.rwGpio);
    gpiod_put(lcd1602_gpio_dev.enableGpio);

    gpiod_put_array(lcd1602_gpio_dev.dataBits);
}

static const struct of_device_id lcd1602_match[] = {
    { .compatible = LCD1602_DISPLAY_COMPATIBLE_STRING },
    { }
};

MODULE_DEVICE_TABLE(of, lcd1602_match);

static struct platform_driver lcd1602_driver = {
    .probe = lcd1602_probe,
    .remove_new = lcd1602_remove,
    .driver = {
        .name = "lcd1602",
        .of_match_table = lcd1602_match
    }
};

static int __init lcd1602_init(void){

    return platform_driver_register(&lcd1602_driver);
}

static void __exit lcd1602_cleanup(void){
    lcd1602_reset_screen();
    device_destroy(lcd1602_cls, MKDEV(lcd1602_device_major, 0));
    class_destroy(lcd1602_cls);
    platform_driver_unregister(&lcd1602_driver);
}

module_init(lcd1602_init);
module_exit(lcd1602_cleanup);

MODULE_AUTHOR("Gyorgy Sarvari");
MODULE_LICENSE("GPL");
