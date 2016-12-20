# Zephyr Heartrate Monitor on Arduino 101

This is the sample app to measure heart rate using a pulse sensor
on an Arduino 101. The measured data will then be sent to a connected
smart phone over BLE.

## Bluetooth Firmware
A bluetooth firmware is neccessary to use the nRF51 Bluetooth LE controller.
Follow the instructions here to prepare the firmware:
https://wiki.zephyrproject.org/view/Arduino_101#Bluetooth_firmware_for_the_Arduino_101

## Building and flashing the applications to Arduino 101
In order to build the Zephyr app for the Arduino 101 board, first setup the
Zephyr dev environment following the guidance here.
* https://wiki.zephyrproject.org/view/Arduino_101

In the app source folder, use the following commands to build and flash the app
to Arduino 101 board:

### ARC core application
```
    $ make pristine
    $ make BOARD=arduino_101_sss ARCH=arc
    $ sudo -E dfu-util -a sensor_core -D outdir/arduino_101_sss/zephyr.bin
```

### x86 core application
```
    $ make BOARD=arduino_101 ARCH=x86
    $ sudo -E dfu-util -a x86_app -D outdir/arduino_101/zephyr.bin
```
