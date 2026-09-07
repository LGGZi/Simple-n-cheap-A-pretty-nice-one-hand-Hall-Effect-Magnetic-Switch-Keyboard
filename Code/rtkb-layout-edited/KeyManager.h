#ifndef KeyManager_h
#define KeyManager_h

#include "Universal.h"
#include "SubHID.h"
#define HIDStateUpdate(...) _HIDStateUpdate((1, ##__VA_ARGS__))

volatile  HIDConReport_t report;
const int keys[]{29,27,12,18,19,4,13,15,51,52,105,105,44,44};
static int key_map[BUTTONCOUNT][2] = {0};
static uint8_t step = 1;

void keyCodeConvert(){
  //键值转化函数，键盘报文以8位为一组，此函数将键值计算以寻找到其在report.keyboardKeys[]对应位置角标和bit位置
  for(int i=0;i<BUTTONCOUNT;i++){
    key_map[i][0] = keys[i] / 8;
    key_map[i][1] = keys[i] % 8;
    // delay(1000);
    // Serial.println("keymap");
    // Serial.println(key_map[i][0]);
    // Serial.println(key_map[i][1]);
  }
}

void keyOperation(uint8_t key){
  switch(key){
    case 0:

      break;
    case 1:
    
      break;
    case 2:
    
      break;
    case 3:
    
      break;
    case 4:
    
      break;
    case 5:
    
      break;
    case 6:
    
      break;
    case 7:
    
      break;
    case 8:
    
      break;
    case 9:
    
      break;
    case 10:
    
      break;
    case 11:
    
      break;
    default:
    
      break;
    // Serial.print("ERROR:keyOperation had a wrong parameter: key");
  }
}
void keyOperationGlobal(uint16_t keyTriggerState){
  // 清空键盘按键报告,方便处理同时按下
  memset(report.keyboardKeys, 0, sizeof(report.keyboardKeys));
  for(int i = 0; i < BUTTONCOUNT; i++){
    if((keyTriggerState >> i) & 1){
      report.keyboardKeys[key_map[i][0]] |= ((uint8_t)1 << key_map[i][1]);
    }
  }
}
void _HIDStateUpdate(uint8_t LoopCount){
  if(step < LoopCount){
    step++;
  }else{
    HIDCon.setState(&report);
    step = 1;
  }
}
#endif