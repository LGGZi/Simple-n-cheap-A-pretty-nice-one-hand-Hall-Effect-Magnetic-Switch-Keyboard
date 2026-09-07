#include <EEPROM.h>
#define MAX_VALUE 0
#define MIN_VALUE 2
#define RT_ACTIVE 4
#define RT_DEACTIVE 6
#define UPPER_DEADZONE 8
#define LOWER_DEADZONE 10

#define SEL_0 8
#define SEL_1 9
#define SEL_2 10
#define RD_0 A0
#define RD_1 A1
#define BUTTONCOUNT 14
uint16_t data0 = 0; 
uint16_t data1 = 0; 
uint16_t cali_threshold_position = 260;
volatile static  uint16_t buttonbuf = 0;
static uint16_t bt_maximum[BUTTONCOUNT];
static uint16_t bt_minimum[BUTTONCOUNT];
static uint16_t rt_active[BUTTONCOUNT];
static uint16_t rt_deactive[BUTTONCOUNT];
static uint16_t high_deadzone[BUTTONCOUNT];
static uint16_t low_deadzone[BUTTONCOUNT];
static uint16_t buttonstate[BUTTONCOUNT] = {0};

int temp=0;
int i=0;

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
  Serial.print(' ');
  Serial.print(bt_maximum[key]);
  Serial.print(' ');
  Serial.print(bt_minimum[key]);
  Serial.print(' ');
  Serial.print('\n');
}

void calibrateAll(){
  for(int i = 0; i < BUTTONCOUNT; i++){
    Serial.print("keyNO.");
    Serial.print(i);
    calibrateKey(i);
    Serial.print("\n");
  }
}

void setup() {
  delay(33);
  pinMode(SEL_0,OUTPUT);
  pinMode(SEL_1,OUTPUT);
  pinMode(SEL_2,OUTPUT);
  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,LOW);
  pinMode(RD_0,INPUT);
  pinMode(RD_1,INPUT);
  Serial.begin(9600);
}

void loop() {
  delay(3000);
  if(i<=1){
    Serial.print("###begin: \n");
    calibrateAll();
    i = 10;
    Serial.print("___end \n");
  }
}
