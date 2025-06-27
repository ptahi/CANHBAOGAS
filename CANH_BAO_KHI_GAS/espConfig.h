extern "C" {
  void app_loop();
}
#ifdef DEBUG
  #define dprint(...)   Serial.print(__VA_ARGS__)
  #define dprintln(...) Serial.println(__VA_ARGS__)
#else
  #define dprint(...)
  #define dprintln(...)
#endif
#ifdef ESP32
  #include <BlynkSimpleEsp32.h>
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer webServer(80);
#else
  #include <BlynkSimpleEsp8266.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <DNSServer.h>
  ESP8266WebServer webServer(80);
  DNSServer dnsServer;
  const byte DNS_PORT = 53;
#endif

#include "configForm.h"
#include <EEPROM.h>
#define EEPROM_CONFIG_START 10     //ô nhớ bắt đầu lưu config
#include <Ticker.h>
Ticker blinker;
#define btSetup   0
#define ledSignal 2
volatile bool     btSetupPressed = false;
volatile uint32_t btSetupPressTime = -1;
volatile uint32_t blinkTime = millis();
#define btSetupHoldTime             10000
#define WIFI_NET_CONNECT_TIMEOUT    50000
#define WIFI_CLOUD_CONNECT_TIMEOUT  50000
#define WIFI_CLOUD_MAX_RETRIES        500

static int connectNetRetries    = WIFI_CLOUD_MAX_RETRIES;
static int connectBlynkRetries  = WIFI_CLOUD_MAX_RETRIES;

struct ConfigStore{
  uint8_t flags;
  char    ssid_sta[34];
  char    pass_sta[64];
  char    blynk_auth[34];
}__attribute__((packed));
ConfigStore configStore; //Khai báo biến cấu trúc configStore
const ConfigStore configDefault = { //Cấu hình default
  0x00,
  "",
  "",
  "invalid token"
};

template<typename T, int size>
void copyString(const String& s, T(&arr)[size]) {
  s.toCharArray(arr, size);
}
void restartMCU() {
  ESP.restart();
  delay(10000);
  // ESP.reset();
  while(1) {};
}
//Trạng thái hoạt động của esp
enum State {
  MODE_WAIT_CONFIG,
  MODE_CONFIGURING,
  MODE_CONNECTING_NET,
  MODE_CONNECTING_CLOUD,
  MODE_RUNNING,
  MODE_SWITCH_TO_STA,
  MODE_RESET_CONFIG,
  MODE_ERROR,

  MODE_MAX_VALUE
};
const char* StateStr[MODE_MAX_VALUE+1] = {
  "WAIT_CONFIG",
  "CONFIGURING",
  "CONNECTING_NET",
  "CONNECTING_CLOUD",
  "RUNNING",
  "SWITCH_TO_STA",
  "RESET_CONFIG",
  "ERROR",

  "INIT"
};
namespace espState
{
  volatile State state = MODE_MAX_VALUE;

  State get()        { return state; }
  bool  is (State m) { return (state == m); }
  void  set(State m);
};
inline
void espState::set(State m) {
  if (state != m && m < MODE_MAX_VALUE) {
    dprintln(String(StateStr[state]) + " => " + StateStr[m]);
    state = m;
  }
}
//Lưu cấu hình vào vùng nhớ EEPROM
bool configSave(){
  EEPROM.put(EEPROM_CONFIG_START, configStore);
  EEPROM.commit();
  dprintln("Configuration stored to flash");
  return true;
}
//Load cấu hình từ bộ nhớ EEPROM
void configLoad(){
  dprintln("Load Configuration stored");
  memset(&configStore, 0, sizeof(configStore));//clear vùng nhớ đệm về 0
  EEPROM.get(EEPROM_CONFIG_START, configStore);
  dprintln("Glags: "+String(configStore.flags));
  dprintln("Wifi sta: "+String(configStore.ssid_sta));
  dprintln("Password: "+String(configStore.pass_sta));
  dprintln("Blynk auth: "+String(configStore.blynk_auth));
}
//Khởi tạo vùng nhớ EEPROM
bool configInit(){
  EEPROM.begin(sizeof(ConfigStore)+EEPROM_CONFIG_START); //Vùng nhớ bằng với size biến cấu trúc + config start
  dprintln("EEPROM config size: "+String(sizeof(ConfigStore)));
  configLoad();
  return true;
}
void blinkLed(uint32_t t){
  if(millis()-blinkTime>t){
    digitalWrite(ledSignal,!digitalRead(ledSignal));
    blinkTime=millis();
  }
}
//Ngắt xử lý ledSignal
void ledSignalControl(){
  State currState = espState::get();
  if(btSetupPressed && (millis() - btSetupPressTime)>btSetupHoldTime){
    digitalWrite(ledSignal,!digitalRead(ledSignal));
  }else if(btSetupPressed){
    blinkLed(1000);
  }else if(currState==MODE_WAIT_CONFIG){
    blinkLed(200);
  }else if(currState==MODE_CONNECTING_NET){
    blinkLed(500);
  }else if(currState==MODE_CONNECTING_CLOUD){
    blinkLed(1000);
  }else if(currState==MODE_RUNNING){
    blinkLed(5000);
  }
}
//Thiết lập lại cấu hình mặc định
void enterResetConfig(){
  dprintln("Esp is reset default!");
  configStore = configDefault;
  configSave();
  espState::set(MODE_WAIT_CONFIG);
}
//Ngắt xử lý sự kiện ấn nút setup
ICACHE_RAM_ATTR void btSetupChange(){
  bool btState = !digitalRead(btSetup);
  if (btState && !btSetupPressed) {
    btSetupPressTime = millis();
    btSetupPressed = true;
    dprintln("Hold the button for 10 seconds to reset default...");
    digitalWrite(ledSignal,HIGH);
  } else if (!btState && btSetupPressed) {
    digitalWrite(ledSignal,LOW);
    btSetupPressed = false;
    uint32_t btHoldTime = millis() - btSetupPressTime;
    if (btHoldTime >= btSetupHoldTime) {
      espState::set(MODE_RESET_CONFIG);
    }
    btSetupPressTime = -1;
  }
}

void enterConfigMode(){
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  String ssid_ap="esp-"+String(macAddr[4],HEX)+String(macAddr[5],HEX);
  ssid_ap.toUpperCase();
  WiFi.softAP(ssid_ap.c_str());
  delay(500);
  dprintln("AP SSID: "+ssid_ap);
  dprintln("AP IP: "+WiFi.softAPIP().toString());
#ifdef ESP8266
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Point all to our IP
#endif
  webServer.on("/", []() {
    webServer.send(200, "text/html", configForm);
  });
  webServer.on("/wifiscan.json", []() {
    dprintln("Scanning networks...");
    int wifi_nets = WiFi.scanNetworks(true, true);
    const uint32_t t = millis();
    while (wifi_nets < 0 && millis() - t < 20000){
      delay(20);
      wifi_nets = WiFi.scanComplete();
    }
    dprintln(String("Found networks: ") + wifi_nets);
    if (wifi_nets > 0) {
      String ssidList = "[\"";                //"[wifi1,wifi2,wifi3]"
      for(int i=0;i<wifi_nets;++i){
        ssidList+= WiFi.SSID(i)+ "\"";        
        if(i<(wifi_nets-1)){
          ssidList += ",\"";
        }
      }
      ssidList += "]";
      webServer.send(200, "application/json", ssidList);
    } else {
      webServer.send(200, "application/json", "[]");
    }
  });
  webServer.on("/configsave", []() {
    dprintln("Applying configuration...");
    String ssid= webServer.arg("ssid_sta");
    String pass = webServer.arg("pass_sta");
    String token = webServer.arg("blynk_auth");
    String content;
    if(ssid.length()>0 && token.length()==32){
      configStore.flags=0x01;
      copyString(ssid, configStore.ssid_sta);
      copyString(pass, configStore.pass_sta);
      copyString(token, configStore.blynk_auth);
      configSave();
      content = "Configuration saved";
    }else{
      dprintln("Configuration invalid");
      content = "Configuration invalid";
    }
    webServer.send(200, "application/json", content);
    connectNetRetries = connectBlynkRetries = 1;
    espState::set(MODE_SWITCH_TO_STA);
  });
  webServer.on("/reboot", []() {
    restartMCU();
  });
  webServer.onNotFound([]() {
    webServer.send(200, "text/html", configForm);
  });
  webServer.begin();

  while(espState::is(MODE_WAIT_CONFIG)||espState::is(MODE_CONFIGURING)){
    app_loop();
    delay(10);
    webServer.handleClient();
#ifdef ESP8266
    dnsServer.processNextRequest();
#endif
    if (espState::is(MODE_CONFIGURING) && WiFi.softAPgetStationNum() == 0) {
      espState::set(MODE_WAIT_CONFIG);
    }
  }
  webServer.stop();
}

void enterSwitchToSTA() {
  espState::set(MODE_SWITCH_TO_STA);
  dprintln("Switching to STA...");
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  espState::set(MODE_CONNECTING_NET);
}

void enterConnectNet() {
  espState::set(MODE_CONNECTING_NET);
  dprintln(String("Connecting to WiFi: ") + configStore.ssid_sta);
  WiFi.mode(WIFI_STA);

  if (!WiFi.begin(configStore.ssid_sta, configStore.pass_sta)) {
    espState::set(MODE_ERROR);
    return;
  }
  int n=0;
  unsigned long timeoutMs = millis() + WIFI_NET_CONNECT_TIMEOUT;
  while ((timeoutMs > millis()) && (WiFi.status() != WL_CONNECTED))
  {
    app_loop();
    delay(10);
    dprint(".");
    n++;
    if(n==40){
      dprintln();
      n=0;
    }
    if (!espState::is(MODE_CONNECTING_NET)) {
      WiFi.disconnect();
      return;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    dprintln("\nUsing Static IP: "+WiFi.localIP().toString());
    espState::set(MODE_CONNECTING_CLOUD);
    connectNetRetries = WIFI_CLOUD_MAX_RETRIES;
  } else if(--connectNetRetries <= 0){
    dprintln();
    espState::set(MODE_ERROR);
  }
}

void enterConnectCloud() {
  espState::set(MODE_CONNECTING_CLOUD);

  Blynk.config(configStore.blynk_auth, "blynk.cloud", 80);
  Blynk.connect(0);

  unsigned long timeoutMs = millis() + WIFI_CLOUD_CONNECT_TIMEOUT;
  while ((timeoutMs > millis()) &&
        (WiFi.status() == WL_CONNECTED) &&
        (!Blynk.isTokenInvalid()) &&
        (Blynk.connected() == false))
  {
    app_loop();
    delay(10);
    Blynk.run();
    if (!espState::is(MODE_CONNECTING_CLOUD)) {
      Blynk.disconnect();
      return;
    }
  }

  if (millis() > timeoutMs) {
    dprintln("Timeout");
  }

  if (Blynk.isTokenInvalid()) {
    espState::set(MODE_WAIT_CONFIG);
  } else if (WiFi.status() != WL_CONNECTED) {
    espState::set(MODE_CONNECTING_NET);
  } else if (Blynk.connected()) {
    espState::set(MODE_RUNNING);
    connectBlynkRetries = WIFI_CLOUD_MAX_RETRIES;
  } else if(--connectBlynkRetries <= 0){
    espState::set(MODE_ERROR);
  }
}

void runBlynkWithChecks() {
  Blynk.run();
  if (espState::get() == MODE_RUNNING) {
    if (!Blynk.connected()) {
      if (WiFi.status() == WL_CONNECTED) {
        espState::set(MODE_CONNECTING_CLOUD);
      } else {
        espState::set(MODE_CONNECTING_NET);
      }
    }
  }
}

void enterError() {
  espState::set(MODE_ERROR);

  unsigned long timeoutMs = millis() + 10000;
  while (timeoutMs > millis() || btSetupPressed)
  {
    delay(10);
    if (!espState::is(MODE_ERROR)) {
      return;
    }
  }
  dprintln("Restarting after error.");
  delay(10);
  restartMCU();
}

class Config{
public:
  void begin(){
    dprintln("\n----------------Esp Config--------------");

    pinMode(btSetup,INPUT_PULLUP);
    pinMode(ledSignal,OUTPUT);
    digitalWrite(ledSignal,LOW);

    blinker.attach_ms(100, ledSignalControl);
    attachInterrupt(btSetup,btSetupChange,CHANGE);

    configInit();

    if(configStore.flags==0x01){
      espState::set(MODE_CONNECTING_NET);
    }else {
      espState::set(MODE_WAIT_CONFIG);
    }
  }
  void run(){
    switch (espState::get()) {
      case MODE_WAIT_CONFIG:       
      case MODE_CONFIGURING:       enterConfigMode();    break;
      case MODE_CONNECTING_NET:    enterConnectNet();    break;
      case MODE_CONNECTING_CLOUD:  enterConnectCloud();  break;
      case MODE_RUNNING:           runBlynkWithChecks(); break;
      case MODE_SWITCH_TO_STA:     enterSwitchToSTA();   break;
      case MODE_RESET_CONFIG:      enterResetConfig();   break;
      default:                     enterError();         break;
    }
  }
} espConfig;