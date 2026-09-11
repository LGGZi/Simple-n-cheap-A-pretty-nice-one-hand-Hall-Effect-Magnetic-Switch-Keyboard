#ifndef EEPROMManager_h
#define EEPROMManager_h
#include "Universal.h"
#include <EEPROM.h>

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
#endif