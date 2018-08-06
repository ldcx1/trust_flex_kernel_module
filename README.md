This is an experimental driver for Trust Flex Design graphic tablet.
When hovering the stylus over surface the cursor is not moving smooth.

  - BTN_TOOL_PEN is signaled as soon as the stylus is in hover range;
  - BTN_TOUCH is set when stylus touch the screen;
  - BTN_STYLUS2 is set while button pressed;
  - BTN_LEFT is set while pen touch the pad;
  - BTN_STYLUS is set while the third button is pressed;
  - GIMP and KRITA work;

Installation
============

To install:
- verify that your computer sees your tablet.  `lsusb` should show 'ID 2179:0077';
- `make` to build the driver;
- `sudo make install` to install the driver.

At this point you should be able to use the stylus as the mouse and with GIMP or Krita, etc.

To uninstall, of course
```
sudo make uninstall
```
References
==========
based on the driver by <fpgasm@apple2.x10.mx>
- https://github.com/odysseywestra/Monoprice_22_linux_kernel_module

and on original drivers by  <weixing@hanwang.com.cn>
- http://linux.fjfi.cvut.cz/~taxman/hw/hanvon/
- https://github.com/exaroth/pentagram_virtuoso_drivers/blob/master/hanvon.c
- further modified and improved by aidyw https://github.com/aidyw/bosto-2g-linux-kernel-module

The driver was built to these guidelines:
- https://www.kernel.org/doc/Documentation/input/event-codes.txt
- http://linuxwacom.sourceforge.net/wiki/index.php/Kernel_Input_Event_Overview


