#define SEL_0 8
#define SEL_1 9
#define SEL_2 10
#define RD_0 A0
#define RD_1 A1
#define BUTTONCOUNT 14
//编码器AB相对应引脚
#define ENC_L_A 2
#define ENC_L_B 1
#define ENC_BT 3

static uint16_t bt_maximum[BUTTONCOUNT] = {0};
static uint16_t bt_minimum[BUTTONCOUNT] = {0};
static uint16_t rt_active[BUTTONCOUNT] = {0};
static uint16_t rt_deactive[BUTTONCOUNT] = {0};
static uint16_t high_deadzone[BUTTONCOUNT] = {0};
static uint16_t low_deadzone[BUTTONCOUNT] = {0};
static uint16_t buttonstate[BUTTONCOUNT] = {0};
static uint16_t high_buttonflag[BUTTONCOUNT] = {0};
static uint16_t low_buttonflag[BUTTONCOUNT] = {0};
void kbScan(){
  static uint16_t data0 = 0; 
  static uint16_t data1 = 0; 
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
void setup(){
  delay(1000);
  pinMode(SEL_0,OUTPUT);
  pinMode(SEL_1,OUTPUT);
  pinMode(SEL_2,OUTPUT);
  digitalWrite(SEL_0,LOW);
  digitalWrite(SEL_1,LOW);
  digitalWrite(SEL_2,LOW);
  pinMode(RD_0,INPUT);
  pinMode(RD_1,INPUT);
  Serial.begin(9600);
  // calibrateKey(0);
  // delay(600);
  // readFromEEPROM();
}
void loop() {
  kbScan();
  for(int i = 0; i < BUTTONCOUNT; i++){
    Serial.print(" ");
    Serial.print(buttonstate[i]);
  }
  Serial.print("\n");
  delayMicroseconds(500);
}