// FastLED 必须最先 include：它的模板参数里有个名字叫 CONTROLLER，
// 而 Universal.h 里 #define CONTROLLER 2，先包含固件头再包含 FastLED 会撞名。
#include <FastLED.h>
#include "KeyManager.h"
#include "EncoderManager.h"
#include "SubHID.h"
#include "HallUnitManager.h"
#include "EEPROMManager.h"
#include "KeyBehaviorEEPROM.h"   // 连发参数的 EEPROM 存读
#include "WS2812BManager.h"      // 灯效（14 颗 WS2812B，数据脚 LEDPIN/D13）
#include "HIDcmdHandler.h"
// extern volatile HIDConReport_t report;

void setup(){
  delay(1000);
  pinMode(ENC_BT,INPUT_PULLUP);
  pinMode(ENC_L_A,INPUT_PULLUP);
  pinMode(ENC_L_B,INPUT_PULLUP);
  pinMode(SEL_0,OUTPUT);
  pinMode(SEL_1,OUTPUT);
  pinMode(SEL_2,OUTPUT);
  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,LOW);
  pinMode(RD_0,INPUT);
  pinMode(RD_1,INPUT);
  #if DEBUG
    Serial.begin(9600);
    while (!Serial) {
      ; // 等待串口连接，仅适用于 USB CDC 板子
    }
    Serial.println("Serial connected");
  #endif
  wsInit();                      // 灯带初始化（FastLED.addLeds + 清屏）
  ledParamsGetFromEEPROM();      // 读灯效参数；无效就写默认值
  flagInit();
  keyParameterInit();
  keyCodeInit();
  //连发参数：紧跟在灯效参数区之后，上电读一次
  rapidParamsGetFromEEPROM();
  // calibrateKey(0);
  // delay(600);
  // readFromEEPROM();
}

void loop() {
  //每帧只在这里读一次 millis()，其它地方都用 getFrameTime()
  frameTimeUpdate();
  //扫描 + 状态机：结果写在 keys[] 里（每键一个结构体）
  updateKeyState();
  
  reportBufferClear();

  keyOperation();
  keyOperation_stickyKey();
  // 灯效默认关闭（有意为之，不是忘了开）：
  // 14 颗 WS2812B 刷新时电流会突变（全白可达数百 mA），顺着 5V 走线和地线产生压降/地弹；
  // 霍尔是比例式输出、ADC 又以 AVCC（同一路 5V）为基准，电源波动会直接叠加到读数上，
  // 让行程百分比和 RT 阈值跟着抖。需要灯效时再取消注释，并接受读数抖动/做硬件滤波。
  // wsTick();                      // 灯效：内部按 fps 限速，每帧只 FastLED.show() 一次
  encoderRotateStateRefresh();
  ENCButtonOnPress(calibrateAll);

  HIDcmdResponseIfChanged();
  HIDStateUpdate(2);
  delayMicroseconds(500);

}
