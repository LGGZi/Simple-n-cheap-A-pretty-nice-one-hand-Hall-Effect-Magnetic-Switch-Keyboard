#ifndef Universal_h
#define Universal_h

#include <EEPROM.h>
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
/*——————————通用函数——————————*/
void keyParameterSet(int8_t key, int8_t valName, uint16_t value){
  // Serial.print("ParameterSet:  ");
  // Serial.print("key=");
  // Serial.print(key);
  // Serial.print("   ");
  // Serial.print("valName=");
  // Serial.print(valName);
  // Serial.print("   ");
  // Serial.print("value=");
  // Serial.print(value);
  // Serial.print("   ");
  EEPROM.put((12*key+valName), value);
}
void ParameterRefresh(int8_t key){
  uint16_t temp = 0;
    EEPROM.get((12*key + RT_ACTIVE), temp);
    rt_active[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
    EEPROM.get((12*key + RT_DEACTIVE), temp);
    rt_deactive[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
    EEPROM.get((12*key + UPPER_DEADZONE), temp);
    high_deadzone[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
    EEPROM.get((12*key + LOWER_DEADZONE), temp);
    low_deadzone[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
}
void keyParameterGetFromEEPROM(int8_t key, int8_t valName){
  // Serial.print("Func--Get:");
  // Serial.print("key=");
  // Serial.print(key);
  // Serial.print("   ");
  // Serial.print("valName=");
  // Serial.print(valName);
  // Serial.print("   ");
  // Serial.print("value=");
  uint16_t temp = 0;
  switch (valName){
    case MAX_VALUE :
      EEPROM.get((12*key+valName), bt_maximum[key]);
      break;
    case MIN_VALUE :
      EEPROM.get((12*key+valName), bt_minimum[key]);
      break;
    case RT_ACTIVE :
      EEPROM.get((12*key+valName), temp);
      rt_active[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
      break;
    case RT_DEACTIVE :
      EEPROM.get((12*key+valName), temp);
      rt_deactive[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
      break;
    case UPPER_DEADZONE :
      EEPROM.get((12*key+valName), temp);
      high_deadzone[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
      break;
    case LOWER_DEADZONE :
      EEPROM.get((12*key+valName), temp);
      low_deadzone[key] = (bt_maximum[key] - bt_minimum[key]) * ((double)temp/1000);
      break;
  }
  // switch (valName){
  //   case MAX_VALUE :
  //     Serial.print(bt_maximum[key]);
  //     break;
  //   case MIN_VALUE :
  //     Serial.print(bt_minimum[key]);
  //     break;
  //   case RT_ACTIVE :
  //     Serial.print(rt_active[key]);
  //     break;
  //   case RT_DEACTIVE :
  //     Serial.print(rt_deactive[key]);
  //     break;
  //   case UPPER_DEADZONE :
  //     Serial.print(high_deadzone[key]);
  //     break;
  //   case LOWER_DEADZONE :
  //     Serial.print(low_deadzone[key]);
  //     break;
  //   default :
  //     Serial.print("ERROR:keyParameterGetFromEEPROM had wrong input of valName");
  // }
  // Serial.print("   ");
  // Serial.print("\n");
}
void keyParameterInit(){
  for(int i = 0; i < BUTTONCOUNT; i++){
    keyParameterGetFromEEPROM(i,MAX_VALUE);
    keyParameterGetFromEEPROM(i,MIN_VALUE);
    keyParameterGetFromEEPROM(i,RT_ACTIVE);
    keyParameterGetFromEEPROM(i,RT_DEACTIVE);
    keyParameterGetFromEEPROM(i,UPPER_DEADZONE);
    keyParameterGetFromEEPROM(i,LOWER_DEADZONE);
  }
}
void kbScan(){
  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,LOW);
  buttonstate[3] = analogRead(RD_0);
  buttonstate[10] = analogRead(RD_1);

  digitalWrite(SEL_0,HIGH);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,LOW);
  buttonstate[4] = analogRead(RD_0);
  buttonstate[5] = analogRead(RD_1);

  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,HIGH);
  digitalWrite(SEL_2,LOW);
  buttonstate[9] = analogRead(RD_0);
  buttonstate[0] = analogRead(RD_1);

  digitalWrite(SEL_0,HIGH);
  digitalWrite(SEL_1,HIGH);
  digitalWrite(SEL_2,LOW);
  buttonstate[8] = analogRead(RD_0);
  buttonstate[12] = analogRead(RD_1);

  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,HIGH);
  buttonstate[13] = analogRead(RD_0);
  buttonstate[1] = analogRead(RD_1);


  digitalWrite(SEL_0,HIGH);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,HIGH);
  buttonstate[2] = analogRead(RD_0);
  buttonstate[7] = analogRead(RD_1);

  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,HIGH);
  digitalWrite(SEL_2,HIGH);
  buttonstate[11] = analogRead(RD_0);
  buttonstate[6] = analogRead(RD_1);


}
// void readFromEEPROM(){
//   uint16_t tempActive = 0;
//   uint16_t tempDeActive = 0;
//   for(int i = 0; i < BUTTONCOUNT; i++){
//     EEPROM.get((12*i),bt_maximum[i]);
//     EEPROM.get((12*(i+2)),bt_minimum[i]);
//     // Serial.print("read");
//     // Serial.print(bt_maximum[0]);
//     // Serial.print(' ');
//     // Serial.print(bt_minimum[0]);
//     // Serial.print(' ');
//     // Serial.print('\n');
//     EEPROM.get(12*(i+4), tempActive);
//     EEPROM.get(12*(i+6), tempDeActive);
//     EEPROM.get(12*(i+8), high_deadzone[i]);
//     EEPROM.get(12*(i+10), low_deadzone[i]);
//     rt_active[i] = (bt_maximum[i] - bt_minimum[i]) * (40 / 1000);
//     rt_deactive[i] = (bt_maximum[i] - bt_minimum[i]) * (40 / 1000);
//     // rt_active[i] = (uint16_t)((bt_maximum[i] - bt_minimum[i]) * (double)(80 / (double)1000));
//     // rt_deactive[i] = (uint16_t)((bt_maximum[i] - bt_minimum[i]) * (double)(80 / (double)1000));

//   }
// }
void flagInit(){
  kbScan();
  memcpy(high_buttonflag, buttonstate, sizeof(high_buttonflag));
  memcpy(low_buttonflag, buttonstate, sizeof(low_buttonflag));
}

uint16_t getKeyState(){
  kbScan();
  for (int i = 0; i < BUTTONCOUNT; i++) {
    // --- 按下检测 ---
    // 如果当前值低于低阈值|下死区，判定为按下，同时更新高阈值，避免释放条件过紧
    if ((buttonstate[i] <= low_buttonflag[i])){
      buttonbuf |= (uint16_t)1 << i;  // 设置对应位
      // 更新阈值
      if((buttonstate[i] - 1) > 0 && (buttonstate[i] - 1) < bt_maximum[i]) {
        low_buttonflag[i] = buttonstate[i] - 1;
      }else{
        low_buttonflag[i] = buttonstate[i];
      }
      high_buttonflag[i] = buttonstate[i] + rt_deactive[i];
    // 如果当前值低于下死区，判定为按下
    }
    // 如果当前值高于低阈值 + rt_active[i]，则重新定位低阈值
      else if (buttonstate[i] > low_buttonflag[i] + rt_active[i]) {
      low_buttonflag[i] = buttonstate[i] - rt_active[i];
    }

    // --- 释放检测 ---
    // 如果当前值高于高阈值|上死区，判定为释放，同时更新低阈值，避免触发条件过紧
    if (buttonstate[i] >= high_buttonflag[i]){
      buttonbuf &= ~((uint16_t)1 << i);  // 清除对应位
      // 更新阈值
      high_buttonflag[i] = buttonstate[i] + 1;
      low_buttonflag[i] = buttonstate[i] - rt_active[i];
    // 如果当前值高于上死区，判定为释放
    }
    // 如果当前值低于高阈值 - rt_deactive[i]，则重新定位高阈值
      else if (buttonstate[i] < (high_buttonflag[i] - rt_deactive[i])) {
      high_buttonflag[i] = buttonstate[i] + rt_deactive[i];
    }
    // --- 死区检测 ---
    if(buttonstate[i] > (bt_maximum[i] - high_deadzone[i])){
      buttonbuf &= ~((uint16_t)1 << i);  // 清除对应位
    }
    if(buttonstate[i] < (bt_minimum[i] + low_deadzone[i])){
      buttonbuf |= (uint16_t)1 << i;  // 设置对应位
    }
    // Serial.print(buttonstate[i]);
    // Serial.print(' ');
    // Serial.print(high_buttonflag[i]);
    // Serial.print(' ');
    // Serial.print(low_buttonflag[i]);
    // Serial.print(' ');
    // Serial.print((buttonbuf >> 0) & i);
    // Serial.print('\n');
  }
  return buttonbuf;
}

void calibrateKey(uint8_t key){
  bt_maximum[key]=0;
  bt_minimum[key]=20000;
  bool cali_not_finish = true;
  while(cali_not_finish){
    // Serial.print(bt_maximum[0]);    
    //   Serial.print(' ');
    //   Serial.print(bt_minimum[0]);    
    //   Serial.print(' ');
    //   Serial.print('\n');
    kbScan();
    if(buttonstate[key] > bt_maximum[key]){ bt_maximum[key] = buttonstate[key]; }
    if(buttonstate[key] < bt_minimum[key]){ bt_minimum[key] = buttonstate[key]; }
    if(buttonstate[key] < cali_threshold_position ){
      while(cali_not_finish){
        // Serial.print(bt_maximum[0]);    
        //   Serial.print(' ');
        //   Serial.print(bt_minimum[0]);    
        //   Serial.print(' ');
        //   Serial.print(cali_not_finish);    
        //   Serial.print(' ');
        //   Serial.print('\n');
        kbScan();
        if(buttonstate[key] > bt_maximum[key]){ bt_maximum[key] = buttonstate[key]; }
        if(buttonstate[key] < bt_minimum[key]){ bt_minimum[key] = buttonstate[key]; }
        if(buttonstate[key] > cali_threshold_position){ cali_not_finish = false; }
      }
    }
  }
  keyParameterSet(key,MAX_VALUE,bt_maximum[key]);
  keyParameterSet(key,MIN_VALUE,bt_minimum[key]);
  // EEPROM.put((12*key),bt_maximum[key]);
  // EEPROM.put((12*key+2),bt_minimum[key]);
  // Serial.print(key);
  // Serial.print(' ');
  // Serial.print(bt_maximum[0]);
  // Serial.print(' ');
  // Serial.print(bt_minimum[0]);
  // Serial.print(' ');
  // Serial.print('\n');
  // uint16_t max1 = 0;
  // uint16_t min1 = 0;
  // Serial.print(max1);
  // Serial.print(' ');
  // Serial.print(min1);
  // Serial.print('\n');
  delay(100);
  ParameterRefresh(key);
}
void calibrateAll(){
  for(int i = 0; i < BUTTONCOUNT; i++){
    calibrateKey(i);
  }
}

#endif