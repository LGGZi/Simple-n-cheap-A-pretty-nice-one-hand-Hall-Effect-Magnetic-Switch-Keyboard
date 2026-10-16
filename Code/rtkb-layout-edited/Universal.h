#ifndef Universal_h
#define Universal_h
//在这里开启或关闭串口DEBUG
#include "SubHID.h"
#define DEBUG 0
#define FIRMWARE_VERSION 0x43

#define MAX_VALUE 0
#define MIN_VALUE 2
#define RT_ACTIVE 4
#define RT_DEACTIVE 6
#define UPPER_DEADZONE 8
#define LOWER_DEADZONE 10
#define MAXIMUM_BINDABLE_KEY 12
//每个按键在 EEPROM 里占用的映射空间：每个映射 2 字节(device, offset) * MAXIMUM_BINDABLE_KEY
//整个映射区 = BUTTONCOUNT * KEYMAP_SIZE_PER_KEY 字节，紧接在按键参数区
//(BUTTONCOUNT * PSOOSK) 之后，各按键的区域依次紧挨着排列。
//注意：这里不做 EEPROM 越界检查 —— 改按键数 / 最大映射数 / 换芯片时请自行核算总占用
//（ATmega32u4 = 1024 字节，例：参数区 14*12 = 168，映射区 14*12*2 = 336，共 504）
#define KEYMAP_SIZE_PER_KEY (MAXIMUM_BINDABLE_KEY * 2)
#define NO_DEVICE 0
#define KEYBOARD 1
#define CONTROLLER 2
#define MOUSE 3
#define MULTIMEDIA 4
#define CONTROLLER_XAXIS_LEFT 5
#define CONTROLLER_YAXIS_LEFT 6
#define CONTROLLER_XAXIS_RIGHT 7
#define CONTROLLER_YAXIS_RIGHT 8
#define MOUSE_X 9
#define MOUSE_Y 10
#define CONTROLLER_LEFT_TRIGGER 11
#define CONTROLLER_RIGHT_TRIGGER 12
//回报速率
#define REPORT_DELAY 1000
// Number of microseconds between HID reports
// 2000 = 500hz
#define SEL_0 8
#define SEL_1 9
#define SEL_2 10
#define RD_0 A0
#define RD_1 A1
#define BUTTONCOUNT 14

#define LEDPIN 10
#define ENC_BT 3
//编码器AB相对应引脚
#define ENC_L_A 2
#define ENC_L_B 1

#if DEBUG
char buff[80];
#endif

static uint16_t bt_maximum[BUTTONCOUNT] = {0};
static uint16_t bt_minimum[BUTTONCOUNT] = {0};
static uint16_t rt_active[BUTTONCOUNT] = {40,40,40,40,40,40,40,40,40,40,40,40};
static uint16_t rt_deactive[BUTTONCOUNT] = {40,40,40,40,40,40,40,40,40,40,40,40};
static uint16_t high_deadzone[BUTTONCOUNT] = {20,20,20,20,20,20,20,20,20,20,20,20};
static uint16_t low_deadzone[BUTTONCOUNT] = {20,20,20,20,20,20,20,20,20,20,20,20};
static uint16_t buttonstate[BUTTONCOUNT] = {0};
static uint16_t high_buttonflag[BUTTONCOUNT] = {0};
static uint16_t low_buttonflag[BUTTONCOUNT] = {0};
static uint8_t key_map[BUTTONCOUNT][MAXIMUM_BINDABLE_KEY][2] = {0};

volatile HIDConReport_t report;
static uint8_t step = 1;
uint16_t cali_threshold_position = 260;

#define HIDStateUpdate(...) _HIDStateUpdate((1, ##__VA_ARGS__))
void _HIDStateUpdate(uint8_t LoopCount){
  if(step < LoopCount){
    step++;
  }else{
    HIDCon.setState(&report);
    HIDCon.setConsumerState(report.mediaButtons);
    step = 1;
  }
}
#endif