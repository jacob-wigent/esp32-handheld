#pragma once
#include "driver/gpio.h"

// ----- INPUTS -----
// Tech
#define SELECT      GPIO_NUM_0
#define BOOT        GPIO_NUM_0
#define START       GPIO_NUM_2
// Controls
#define RIGHT       GPIO_NUM_17
#define UP          GPIO_NUM_18
#define DOWN        GPIO_NUM_36
#define LEFT        GPIO_NUM_35
#define A           GPIO_NUM_39
#define B           GPIO_NUM_40
#define X           GPIO_NUM_41
#define Y           GPIO_NUM_42
#define BUMPER_L    GPIO_NUM_37
#define BUMPER_R    GPIO_NUM_38
#define STICK_BTN   GPIO_NUM_15
#define AY          GPIO_NUM_6
#define AX          GPIO_NUM_7

// ----- OUTPUTS -----
// Status LED
#define STATUS_LED  GPIO_NUM_3
// Speaker
#define SPEAKER     GPIO_NUM_45
// Motor
#define MOTOR       GPIO_NUM_46

// ----- PERIPHERALS -----
// I2C Pins
#define SDA         GPIO_NUM_8
#define SCL         GPIO_NUM_9
// Display Pins
#define DISP8       GPIO_NUM_10
#define DISP7       GPIO_NUM_11
#define DISP6       GPIO_NUM_12
#define DISP5       GPIO_NUM_13
#define DISP4       GPIO_NUM_14
#define DISP3       GPIO_NUM_21
#define DISP2       GPIO_NUM_47
#define DISP1       GPIO_NUM_48

// ----- POWER MONITORING -----
// Battery
#define ABAT        GPIO_NUM_1
#define CHG_STAT    GPIO_NUM_16
// USB-C
#define ACC1        GPIO_NUM_4
#define ACC2        GPIO_NUM_5

