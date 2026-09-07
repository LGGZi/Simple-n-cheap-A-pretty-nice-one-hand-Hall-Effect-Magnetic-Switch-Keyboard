#include <EEPROM.h>

#define MAX_VALUE 0
#define MIN_VALUE 2
#define RT_ACTIVE 4
#define RT_DEACTIVE 6
#define UPPER_DEADZONE 8
#define LOWER_DEADZONE 10
#define BUTTONCOUNT 14
static uint16_t bt_maximum[BUTTONCOUNT] = {0};
static uint16_t bt_minimum[BUTTONCOUNT] = {0};
static uint16_t rt_active[BUTTONCOUNT] = {0};
static uint16_t rt_deactive[BUTTONCOUNT] = {0};
static uint16_t high_deadzone[BUTTONCOUNT] = {0};
static uint16_t low_deadzone[BUTTONCOUNT] = {0};
int temp=0;
int i=0;
void keyParameterSet(int8_t key, int8_t valName, uint16_t value){
  Serial.print("ParameterSet:  ");
  Serial.print("key=");
  Serial.print(key);
  Serial.print("   ");
  Serial.print("valName=");
  Serial.print(valName);
  Serial.print("   ");
  Serial.print("value=");
  Serial.print(value);
  Serial.print("   ");
  EEPROM.put((12*key+valName), value);
  Serial.print("\n");
}
uint16_t keyParameterGet(int8_t key, int8_t valName){
  EEPROM.get((12*key+valName), temp);
  Serial.print("Func--Get:");
  Serial.print("key=");
  Serial.print(key);
  Serial.print("   ");
  Serial.print("valName=");
  Serial.print(valName);
  Serial.print("   ");
  Serial.print("value=");
  Serial.print(temp);
  Serial.print("   ");
  Serial.print("\n");
}

void setup(){
  delay(1000);
  Serial.begin(9600);

}

void loop(){
  delay(3000);
  if(i<=1){

    Serial.print("###begin: \n");
     for(int round = 0; round < BUTTONCOUNT ;round++){
    //    keyParameterSet(round,MAX_VALUE,440);
    //    keyParameterSet(round,MIN_VALUE,0);
       keyParameterSet(round,RT_ACTIVE,40);
       keyParameterSet(round,RT_DEACTIVE,40);
       keyParameterSet(round,UPPER_DEADZONE,50);
       keyParameterSet(round,LOWER_DEADZONE,50);
     }
    for(int round = 0; round < BUTTONCOUNT ;round++){
      keyParameterGet(round,MAX_VALUE);
      keyParameterGet(round,MIN_VALUE);
      keyParameterGet(round,RT_ACTIVE);
      keyParameterGet(round,RT_DEACTIVE);
      keyParameterGet(round,UPPER_DEADZONE);
      keyParameterGet(round,LOWER_DEADZONE);
    }
    i = 10;
    Serial.print("___end \n");

  }
  // Serial.print("v:");
  // Serial.print(temp);
}