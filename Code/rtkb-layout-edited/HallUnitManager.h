#ifndef HallUnitManager_h
#define HallUnitManager_h
#include "Universal.h"
#include "EEPROMManager.h"

static uint16_t buttonstate[BUTTONCOUNT] = {0};
static uint16_t high_buttonflag[BUTTONCOUNT] = {0};
static uint16_t low_buttonflag[BUTTONCOUNT] = {0};

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

//扫描 + 状态机：结果写进 keys[]（每键一个结构体，和 6kb 固件同一套）
//以前是把结果拼成一个 uint16_t 位图返回（buttonbuf），最多 16 键；
//现在直接维护 keys[i].down / down_edge / pressedAt，位宽不再受限。
void updateKeyState(){
  //上一帧的按下状态：和 6kb 的 updateKeyState() 一样必须是 static（跨扫描保存），
  //不能写成循环里的局部变量，否则每次都从 0 开始、判不出边沿。
  static uint8_t pre_button_state[BUTTONCOUNT] = {0};
  _kbScan();
  for (int i = 0; i < BUTTONCOUNT; i++) {
    // --- 第一步：死区检测 ---
    if(buttonstate[i] > (bt_maximum[i] - high_deadzone[i])){
      // 上死区检测
      keys[i].down = 0;
      //确保低阈值在上死区-触发行程的位置，避免低阈值离死区过近导致的误触发或断触问题
      low_buttonflag[i] = bt_maximum[i] - high_deadzone[i] - rt_active[i];
    }else if(buttonstate[i] < (bt_minimum[i] + low_deadzone[i])){
      // 下死区检测
      keys[i].down = 1;
      //确保高阈值在下死区+释放行程的位置，避免高阈值离死区过近导致的误触发或断触问题
      high_buttonflag[i] = bt_minimum[i] + low_deadzone[i] + rt_deactive[i];
    }else{
      // --- 按下检测和低阈值跟随行为 ---
      // 如果当前值低于低阈值，判定为按下
      if ( (buttonstate[i] <= low_buttonflag[i])){
        keys[i].down = 1;
        // 更新阈值。
        // 注意：原来写的是 if((s-1) > 0) low = s-1; else if((s-1) <= 0) low = s;
        // s 是 uint16，s == 0 时 s-1 会下溢成 65535，让这个键之后一直判按下。
        low_buttonflag[i] = (buttonstate[i] > 0) ? (buttonstate[i] - 1) : 0;
      }
      // 如果当前值高于低阈值 + rt_active[i]，则重新定位低阈值
      if (buttonstate[i] > low_buttonflag[i] + rt_active[i]) {
        low_buttonflag[i] = buttonstate[i] - rt_active[i];
      }

      // --- 释放检测和高阈值跟随行为 ---
      // 如果当前值高于高阈值，判定为释放
      if (buttonstate[i] >= high_buttonflag[i]){
        keys[i].down = 0;
        // 更新阈值。
        // 原来写的是 if((s+1) < max) high = s+1; else if((s+1) >= 0) high = s;
        // 后半句是无符号恒真（编译器会告警），等价于 else，这里直接写成三元。
        high_buttonflag[i] = (buttonstate[i] + 1 < bt_maximum[i])
                             ? (buttonstate[i] + 1)
                             : buttonstate[i];
      }
      // 如果当前值低于高阈值 - rt_deactive[i]，则重新定位高阈值
      if (buttonstate[i] < (high_buttonflag[i] - rt_deactive[i]) ) {
        high_buttonflag[i] = buttonstate[i] + rt_deactive[i];
      }
    }

    if(keys[i].down){
      keys[i].pressedAt = getFrameTime();
      if(pre_button_state[i] == 0){
        keys[i].down_edge = 1;
      }else{
        keys[i].down_edge = 0;
      }
    }else{
      keys[i].down_edge = 0;
    }
    pre_button_state[i] = keys[i].down;

    // if(i == 0){
    //   Serial.print(buttonstate[i]);
    //   Serial.print(' ');
    //   Serial.print(high_buttonflag[i]);
    //   Serial.print(' ');
    //   Serial.print(low_buttonflag[i]);
    //   Serial.print(' ');
    //   Serial.print(keys[i].down);
    //   Serial.print('\n');
    // }
  }
}

void calibrateKey(uint8_t key){
  bt_maximum[key]=0;
  bt_minimum[key]=20000;
  bool cali_not_finish = true;
  while(cali_not_finish){
    #if DEBUG
    // Serial.print(bt_maximum[0]);    
    //   Serial.print(' ');
    //   Serial.print(bt_minimum[0]);    
    //   Serial.print(' ');
    //   Serial.print('\n');
    #endif
    _kbScan();
    if(buttonstate[key] > bt_maximum[key]){ bt_maximum[key] = buttonstate[key]; }
    if(buttonstate[key] < bt_minimum[key]){ bt_minimum[key] = buttonstate[key]; }
    if(buttonstate[key] < cali_threshold_position ){
      while(cali_not_finish){
        _kbScan();
        if(buttonstate[key] > bt_maximum[key]){ bt_maximum[key] = buttonstate[key]; }
        if(buttonstate[key] < bt_minimum[key]){ bt_minimum[key] = buttonstate[key]; }
        if(buttonstate[key] > cali_threshold_position){ cali_not_finish = false; }
      }
    }
  }
  keyParameterSet(key,MAX_VALUE,bt_maximum[key]);
  keyParameterSet(key,MIN_VALUE,bt_minimum[key]);
  #if DEBUG
  // Serial.print("Calibrate Down \n");
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
  #endif
  delay(100);
  parameterRefresh(key);
}

void calibrateAll(){
  for(int i = 0; i < BUTTONCOUNT; i++){
    calibrateKey(i);
  }
}
#endif
