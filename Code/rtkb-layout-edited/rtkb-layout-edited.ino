// #include <FastLED.h>
#include "KeyManager.h"
#include "EncoderManager.h"
#include "SubHID.h"
#include "HallUnitManager.h"
#include "EEPROMManager.h"
// extern volatile HIDConReport_t report;
int debug = 0;



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
  // Serial.begin(9600);
  flagInit();
  keyParameterInit();
  keyCodeConvert();
  // calibrateKey(0);
  // delay(600);
  // readFromEEPROM();
}

void loop() {
  keyOperationGlobal(getKeyState());
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
  // encoderOnRotate();
  HIDStateUpdate(2);
  delayMicroseconds(500);

}
