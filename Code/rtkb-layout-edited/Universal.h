#ifndef Universal_h
#define Universal_h
#define MAX_VALUE 0
#define MIN_VALUE 2
#define RT_ACTIVE 4
#define RT_DEACTIVE 6
#define UPPER_DEADZONE 8
#define LOWER_DEADZONE 10
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
uint16_t data0 = 0; 
uint16_t data1 = 0; 
uint16_t cali_threshold_position = 260;
volatile static  uint16_t buttonbuf = 0;
static uint16_t bt_maximum[BUTTONCOUNT] = {0};
static uint16_t bt_minimum[BUTTONCOUNT] = {0};
static uint16_t rt_active[BUTTONCOUNT] = {40,40,40,40,40,40,40,40,40,40,40,40};
static uint16_t rt_deactive[BUTTONCOUNT] = {40,40,40,40,40,40,40,40,40,40,40,40};
static uint16_t high_deadzone[BUTTONCOUNT] = {20,20,20,20,20,20,20,20,20,20,20,20};
static uint16_t low_deadzone[BUTTONCOUNT] = {20,20,20,20,20,20,20,20,20,20,20,20};
static uint16_t buttonstate[BUTTONCOUNT] = {0};
static uint16_t high_buttonflag[BUTTONCOUNT] = {0};
static uint16_t low_buttonflag[BUTTONCOUNT] = {0};
// volatile HIDConReport_t report;

#endif