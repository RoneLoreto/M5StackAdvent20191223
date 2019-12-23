#include <M5Stack.h>
#define TINY_GSM_MODEM_UBLOX
#include <TinyGsmClient.h>
//#include <TinyGPS++.h>
#include "Adafruit_SI1145.h"
#include <PubSubClient.h>
#include "Adafruit_SI1145.h" // センサのライブラリ


// 3G 設定
TinyGsm modem(Serial2);   // 3G モデム
TinyGsmClient ctx(modem);


// MQTT 設定 (subscriber:ThingSpeak)
const char mqttUserName[] = "name";   // name→設定した名前に書き換える
const char mqttPass[] = "pass";       // pass→設定されたmqttパスに書き換える
const char writeApiKey[] = "apikey";  // apikey→設定されたwriteAPIキーに書き換える
const long channelId = 000000;        // 000000→設定されたチャネルIDに書き換える

static const char alphanum[] ="0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";  // クライアントIDをランダム生成するための文字列

PubSubClient mqttClient(ctx);
const char* server = "mqtt.thingspeak.com";


// Publish 間隔の設定
unsigned long lastConnectionTime = 0; 
const unsigned long postingInterval = 30L * 1000L; // Publish の間隔を30秒にする


// センサ 設定
Adafruit_SI1145 uv = Adafruit_SI1145();
String sensorValStr;


// 関数宣言
void init3G();
void sensing();
void reConnect();
void mqttPublish();



void setup() {
  M5.begin();
  Serial.begin(115200);

  init3G();
  mqttClient.setServer(server, 1883); // MQTT ブローカの詳細設定
  
  M5.Lcd.clear(BLACK);
}


void loop() {
  reConnect();  // 接続が切れた際に再接続

  mqttClient.loop();   // MQTT 接続を確立する

  // 最後に Publish した時間から postingInterval が経過すると動作する
  if (millis() - lastConnectionTime > postingInterval) {
    sensing();
    mqttPublish();
  }
}



/* 
 * ----- 以下loop内で用いる関数 -----
 */
void init3G() { // 3G 接続の初期設定
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println(F("M5Stack + 3G Module"));

  M5.Lcd.print(F("modem.restart()"));
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  modem.restart();
  M5.Lcd.println(F("done"));

  M5.Lcd.print(F("getModemInfo:"));
  String modemInfo = modem.getModemInfo();
  M5.Lcd.println(modemInfo);

  M5.Lcd.print(F("waitForNetwork()"));
  while (!modem.waitForNetwork()) M5.Lcd.print(".");
  M5.Lcd.println(F("Ok"));

  M5.Lcd.print(F("gprsConnect(soracom.io)"));
  modem.gprsConnect("soracom.io", "sora", "sora");
  M5.Lcd.println(F("done"));

  M5.Lcd.print(F("isNetworkConnected()"));
  while (!modem.isNetworkConnected()) M5.Lcd.print(".");
  M5.Lcd.println(F("Ok"));

  M5.Lcd.print(F("My IP addr: "));
  IPAddress ipaddr = modem.localIP();
  M5.Lcd.print(ipaddr);
  delay(2000);
}


void sensing() { // センサの値を計測する関数
  if (! uv.begin()) {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println("SI1145 not found. Please restart.");
    while(1);
  }
  
  float visVal = uv.readVisible();
  sensorValStr = (String)visVal;

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.print("visVal:");
  M5.Lcd.setCursor(160, 60);
  M5.Lcd.print(visVal);
}


void reConnect() { // 再接続用の関数
  char clientId[10];

  // 接続まで繰り返す
  while (!mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
        clientId[i] = alphanum[random(51)];
    }

    // MQTT broker に接続する
    if (mqttClient.connect(clientId, mqttUserName, mqttPass)) 
    {
      Serial.print("Connected with Client ID:  ");
      Serial.print(String(clientId));
      Serial.print(", Username: ");
      Serial.print(mqttUserName);
      Serial.print(" , Passwword: ");
      Serial.println(mqttPass);
    } else 
    {
      Serial.print("failed, rc=");
      // http://pubsubclient.knolleary.net/api.html#state に state 一覧が書いてある
      Serial.print(mqttClient.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttPublish() {
  // ThingSpeak に Publish するための文字列データを作成する
  String data = ("field1=" + sensorValStr);

  int length = data.length();
  char msgBuffer[length];
  data.toCharArray(msgBuffer, length+1);
  
  // topic 文字列を作成し ThingSpeak の channel feed にデータを Publish する
  String topicString ="channels/" + String(channelId) + "/publish/" + String(writeApiKey);
  length = topicString.length();
  char topicBuffer[length];
  topicString.toCharArray(topicBuffer,length+1);
 
  mqttClient.publish(topicBuffer, msgBuffer);

  lastConnectionTime = millis();
}
