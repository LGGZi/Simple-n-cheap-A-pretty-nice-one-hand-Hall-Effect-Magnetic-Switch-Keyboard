#ifndef KeyManager_h
#define KeyManager_h

#include "Universal.h"
#include "SubHID.h"
#define HIDStateUpdate(...) _HIDStateUpdate((1, ##__VA_ARGS__))
#define DEVICE 0
#define KEYOFFSET 1
volatile  HIDConReport_t report;
const uint8_t keys[]{29,27,12,18,19,4,13,15,51,52,105,105,44,44};
static uint8_t step = 1;


void reportBufferClear(){
  memset(&report, 0, sizeof(report));
}
void keyOperation(uint16_t keyTriggerState){
  for(int i = 0; i < BUTTONCOUNT; i++){
    if((keyTriggerState >> i) & 1){
      //用 for 而不是 while：下标一定会自增到 MAXIMUM_BINDABLE_KEY 然后退出，
      //不存在"忘了自增就永远转下去"的可能（之前这里就是死循环的来源）
      for(uint8_t key_map_addr = 0; key_map_addr < MAXIMUM_BINDABLE_KEY; key_map_addr++){
        uint8_t device = key_map[i][key_map_addr][DEVICE];
        if(device == NO_DEVICE){
          break;   //空槽：本按键的映射到此为止
        }
        //每个 case 都用 {} 包起来：C++ 里 case 里的变量声明如果不用大括号，
        //作用域会撑到整个 switch，编译告警 "jump to case label"，
        //而且 AVR 上实测会让后面几个 case 根本不执行（表现就是只有键盘映射生效）。
        switch(device){
          case KEYBOARD: {
            uint8_t high_bits = (key_map[i][key_map_addr][KEYOFFSET] >> 4) & 0x0F;
            uint8_t low_bits = (key_map[i][key_map_addr][KEYOFFSET] & 0x0F);
            //keyboardKeys[] 一共 14 字节（13 普通键 + 1 修饰键），
            //越界会踩到后面的 mouseButtons，所以这里挡一下（EEPROM 数据损坏时才会发生）
            if(high_bits < 14){
              report.keyboardKeys[high_bits] |= ((uint8_t)1 << low_bits);
            }
            break;
          }
          case CONTROLLER: {
            report.xboxButtons |= (uint8_t)1 << key_map[i][key_map_addr][KEYOFFSET];
            break;
          }
          case MOUSE: {
            report.mouseButtons |= (uint8_t)1 << key_map[i][key_map_addr][KEYOFFSET];
            break;
          }
          case MULTIMEDIA: {
            report.mediaButtons |= (uint8_t)1 << key_map[i][key_map_addr][KEYOFFSET];
            break;
          }
          default:
            break;   //损坏 / 未知的 device：跳过这一个槽继续处理后面的，
                     //不要 return —— 那样这一个坏槽会让整个按键甚至其它按键都不响应
        }
      }
    }
  }
}
void keyOperationGlobal(uint16_t keyTriggerState){
  // 清空键盘按键报告,方便处理同时按下
  // memset(report.keyboardKeys, 0, sizeof(report.keyboardKeys));
  for(int i = 0; i < BUTTONCOUNT; i++){
    if((keyTriggerState >> i) & 1){
 
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