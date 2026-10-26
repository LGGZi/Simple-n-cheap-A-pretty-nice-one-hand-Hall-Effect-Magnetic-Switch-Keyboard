// #include <FastLED.h>
#include "KeyManager.h"
#include "EncoderManager.h"
#include "SubHID.h"
#include "HallUnitManager.h"
#include "EEPROMManager.h"
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
  flagInit();
  keyParameterInit();
  keyCodeInit();
  // calibrateKey(0);
  // delay(600);
  // readFromEEPROM();
}

void loop() {
  //每帧只在这里读一次 millis()，其它地方都用 getFrameTime()
  frameTimeUpdate();
  reportBufferClear();
  // keyOperationGlobal(getKeyState());
  keyOperation(getKeyState());
  encoderRotateStateRefresh();
  // for(int i = 0; i < BUTTONCOUNT; i++){
  //   Serial.print(" ");
  //   Serial.print(buttonstate[i]);
  // }
  // Serial.print("\n");
  // if(debug < 500){
  //   Serial.print(getKeyState());
  //   Serial.print("\n");
  //   debug = 0;
  // }
  // debug += 1;
  // for(int i = 0; i<BUTTONCOUNT; i++){
  //   if((buttonbuf >> i) & 1){
  //     // keyOperation(i);
  //   }
  // }
  ENCButtonOnPress(calibrateAll);
  HIDcmdResponseIfChanged();
  HIDStateUpdate(2);
  delayMicroseconds(500);

}
