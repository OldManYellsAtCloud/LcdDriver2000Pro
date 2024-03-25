This is a Linux driver for the display that comes with the Arduino starter kit. Haven't hecked exactly, but I believe it needs at least kernel 6.4.

It was created with RPi in mind, the device tree overlay was meant for that too.

This was done for myself only in mind. If you have found this want to use it, feel free, though I can't offer a lot of support (most likely won't say no, though).

(Otherwise there is a dedicated mainline driver also, which would be more recommended instead of this random out-of-tree driver. Also, I still had to use an analog source for the contrast pin, so maybe the display is more fun than useful, at least when using with the RPi)

Upon loading the driver, it can be interacted by screaming into `/dev/$DEVICE_NAME_FROM_DEVICETREE` file.

To switch to 2nd line, use `\r` or `\n` character in the string.
