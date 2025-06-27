#include <dummy.h>

#include <Servo.h>
Servo myservo;
#define servoPin D4  //D4
int pos=0;

#define BLYNK_FIRMWARE_VERSION "0.1.0"
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL68kpTQfli"
#define BLYNK_TEMPLATE_NAME "CANHBAOGAS"
#define BLYNK_AUTH_TOKEN "x76FirLATt3NzwYpKTu-0PyXBf7LaHIG"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
char auth[] = "x76FirLATt3NzwYpKTu-0PyXBf7LaHIG";
char ssid[] = "P203";
char pass[] = "88888888";
#define DEBUG
#include "espConfig.h"
#define ledBlue D6
#define ledRed D5
#define buzzer D7
#define gasPin A0
unsigned long timeReadSensor=millis();
int gasWarning=50;
bool runMode=0; //0:bình thường, 1: bật cảnh báo
WidgetLED LEDCONNECT(V0);
#define GASVALUE V1
#define GASWARN V2
#define RUNMODE V3
#define WARNING V4
void setup() {
  Serial.begin(115200);
  pinMode(ledBlue,OUTPUT);
  pinMode(ledRed,OUTPUT);
  pinMode(buzzer,OUTPUT);
  digitalWrite(ledBlue,HIGH);//Tắt led xanh
  digitalWrite(ledRed,HIGH);//Tắt led đỏ
  digitalWrite(buzzer,LOW);//Tắt buzzer

  espConfig.begin();
  gasWarning = readInt(0);
  runMode = EEPROM.read(2);
  Serial.println("\nGas warning: "+String(gasWarning));
  Serial.println("RunMode: "+String(runMode));
  if(runMode==0){
    digitalWrite(ledBlue,HIGH);
  }else{
    digitalWrite(ledBlue,LOW);
  }
}

void loop() {
  espConfig.run();
  app_loop();
}
void app_loop(){
  if(millis()-timeReadSensor>1000){
   
    int adcValue = analogRead(gasPin);
    float voltage = adcValue / 1024.0 * 3.3;
    float ratio = voltage / 1.4;
    float gasValue = 1000.0 * pow(10, ((log10(ratio) - 1.0278) / 0.6629));
    Serial.println("Gas value: "+String(gasValue));
    if(runMode==1){
      if(gasValue>gasWarning){
         myservo.write(90);
        digitalWrite(ledBlue,HIGH);
        digitalWrite(ledRed,LOW);//Bật đèn đỏ
        digitalWrite(buzzer,HIGH);//Bật còi báo
        Blynk.virtualWrite(WARNING,HIGH);
      }else{
        myservo.write(0);
        digitalWrite(ledBlue,LOW);
        digitalWrite(ledRed,HIGH);//Tắt đèn đỏ
        digitalWrite(buzzer,LOW);//Tắt còi báo
        Blynk.virtualWrite(WARNING,LOW);
      }
    }else{
      if(gasValue>gasWarning){
        digitalWrite(ledRed,LOW);
      }else{
        digitalWrite(ledRed,HIGH);
      }
    }
    if(LEDCONNECT.getValue()) LEDCONNECT.off();
    else LEDCONNECT.on();
    Blynk.virtualWrite(GASVALUE,gasValue);
    timeReadSensor=millis();
  }
}
void writeInt(int add, int num){
  byte b1 = num >> 8;
  byte b2 = num & 0xFF;
  EEPROM.write(add,b1);
  EEPROM.write(add+1,b2);
}
int readInt(int add){
  byte b1 = EEPROM.read(add);
  byte b2 = EEPROM.read(add+1);
  return (b1 << 8) + b2;
}
BLYNK_CONNECTED(){
  Blynk.syncAll(); //Đồng bộ dữ liệu từ server xuống esp khi kết nối
}
BLYNK_WRITE(GASWARN){
  int p = param.asInt();
  if(p!=gasWarning) gasWarning = p;
  writeInt(0,gasWarning);
  EEPROM.commit();
  Serial.println("Gas warning: "+String(gasWarning));
}
BLYNK_WRITE(RUNMODE){
  runMode = param.asInt();
  EEPROM.write(2,runMode);
  EEPROM.commit();
  Serial.println("Run mode:"+String(runMode));
  if(runMode==1){
    digitalWrite(ledBlue,LOW);
  }else{
    digitalWrite(ledBlue,HIGH);
    digitalWrite(ledRed,HIGH);//Tắt đèn đỏ
    digitalWrite(buzzer,LOW);//Tắt còi báo
    Blynk.virtualWrite(WARNING,LOW);
  }
}