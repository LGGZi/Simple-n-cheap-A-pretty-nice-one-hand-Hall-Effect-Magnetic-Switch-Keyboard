#ifndef KeyManager_h
#define KeyManager_h

#include "Universal.h"
#include "SubHID.h"
#include "KeyBehaviorManager.h"

void reportBufferClear(){
  memset(&report, 0, sizeof(report));
}

//粘滞键：把“保持输出”的键每帧写进 report（和 6kb 一样，在 keyOperation 之后调用）
void keyOperation_stickyKey(){
  stickyKeyBehavior();
}

//按键映射的执行部分：把“哪几个键按下了”翻译成 HID 报文内容。
//device 字节高 4 位是 behavior（0 普通 / 1 粘滞 / 2 连发 / 3 粘滞+连发），
//低 4 位才是真正的设备类型 —— 和 6kb 固件同一套。
void keyOperation(){
  for(uint8_t i = 0; i < BUTTONCOUNT; i++){
    if(keyDown(i)){
      //遍历此按键的槽，直到遇到 DEVICE 为 0
      for (uint8_t key_map_addr = 0; key_map_addr < MAX_BINDINGS_PER_KEY; key_map_addr++) {
        uint8_t device = key_map[i][key_map_addr][DEVICE] & 0x0F;
        uint8_t behavior = (key_map[i][key_map_addr][DEVICE] & 0xF0) >> 4;
        uint8_t offset = key_map[i][key_map_addr][KEYOFFSET];
        if (key_map[i][key_map_addr][DEVICE] == NO_DEVICE) {
          break;  //空槽：本按键的映射到此为止
        }
        if(behavior == 0) {
          //behavior 为 0 则为常规按键
          keyCodeWrite(device, offset);
          continue;
        }else{
          //否则，有特殊功能
          switch (behavior){
            case STICKY:
            {
              if(device == TOGGLE){
                if(keyDownEdge(i)){
                  stickyKeyBehavior_toggle();
                }
              }
              if(device == 0x0E){
                if(keyDownEdge(i)){
                  stickyKeyBehavior_stateSwitch(i);
                }
              }
            break;
            }
            case RAPID_TAB:
            {
              if(device == TOGGLE){
                if(keyDownEdge(i)){
                  rapidTabBehavior_toggle();
                }
              }else if(device == RAPID_TAB_INC){
                if(keyDownEdge(i)){
                  rapidTabInterval_Increase();
                }
              }else if(device == RAPID_TAB_DEC){
                if(keyDownEdge(i)){
                  rapidTabInterval_Decrease();
                }
              }else{
                rapidTabBehavior(i, device, offset);
              }
              break;
            }
          }
        }
      }
    }
  }
}

#endif
