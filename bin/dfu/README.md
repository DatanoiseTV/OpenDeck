Firmware update on OpenDeck boards is based on [LUFA HID bootloader](https://github.com/abcminiuser/lufa/tree/master/Bootloaders/HID). This directory contains two sub-directories:

* `src` - slighty modified version of the Python script found in LUFA HID repository (only VID/PID numbers are changed)
* `loader` - contains compiled single-file executables of that same script for Linux, Mac OS X and Windows