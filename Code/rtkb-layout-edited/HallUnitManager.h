#ifndef HallUnitManager_h
#define HallUnitManager_h
#include "Universal.h"
#include "EEPROMManager.h"

void _kbScan(){
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

void flagInit(){
  _kbScan();
  memcpy(high_buttonflag, buttonstate, sizeof(high_buttonflag));
  memcpy(low_buttonflag, buttonstate, sizeof(low_buttonflag));
}

uint16_t getKeyState(){
  _kbScan();
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
    _kbScan();
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
        _kbScan();
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