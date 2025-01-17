#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DHT.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1
#define DHTTYPE DHT11   // 使用DHT 11温度湿度模块 
#define MQ2_ANALOG_PIN A0  // ESP8266 的 ADC
#define FIRE_DIGITAL_PIN 16 // ESP8266 的 GPIO16 (D0)
#define BEEP D3
#define DHTPIN D4      //定义DHT11模块连接管脚io5

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ D2, /* data=*/ D1, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display
DHT dht(DHTPIN, DHTTYPE);    //定义dht

// Wifi: SSID and password
const char* WIFI_SSID = "Xiaomi_705";
const char* WIFI_PASSWORD = "20210101";

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = "Kitchen_alarm1";
const PROGMEM char* MQTT_SERVER_IP = "192.168.31.120";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char* MQTT_USER = "murphy";
const PROGMEM char* MQTT_PASSWORD = "murphy";

// MQTT: topics
const char* MQTT_LIGHT_STATE_TOPIC = "kitchen/alarm/state";
const char* MQTT_LIGHT_COMMAND_TOPIC = "kitchen/alarm/state";

int wifi_sign = 0;
float humi_read = 0, temp_read = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 60*60*8, 30*60*1000);

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  
  // handle message topic
}

int reconnect() {
  static int reconnectAttempts = 0;
  const int maxReconnectAttempts = 3;

  while (!client.connected() && reconnectAttempts < maxReconnectAttempts) {
    Serial.println("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("INFO: connected");
      // Once connected, publish an announcement...

      // ... and resubscribe
      client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
      reconnectAttempts = 0; // reset counter after successful connection
      u8g2.clearBuffer();
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 500ms");
      reconnectAttempts++;
      // Wait 500ms before retrying
      delay(500);
    }
  }

  if (reconnectAttempts >= maxReconnectAttempts) {
    Serial.println("ERROR: Maximum reconnect attempts reached. Giving up.\n");
    return 1;
  }
  return 0;
}

void publishData(float p_MQ2,float p_temp,float p_humi) {
  // 使用最新版本的ArduinoJson库
  StaticJsonDocument<200> doc;
  // 将数据添加到JSON文档中
  doc["GAS"] = p_MQ2;
  doc["TEMP"] = p_temp;
  doc["HUMI"] = p_humi;

  // 将JSON文档序列化为字符串
  char data[200];
  size_t n = serializeJson(doc, data);

  // 打印JSON到串口监视器
  serializeJsonPretty(doc, Serial);
  Serial.println("");

  // 发布到MQTT主题
  if (client.publish(MQTT_LIGHT_STATE_TOPIC, data, true)) {
    Serial.println("Data published successfully");
  } else {
    Serial.println("Failed to publish data");
  }

  yield();
}

float MQ2() {
  int analogValue = analogRead(MQ2_ANALOG_PIN);
  int digitalValue = digitalRead(FIRE_DIGITAL_PIN);
  Serial.print("MQ-2 Analog Value: ");
  Serial.println(analogValue);
  Serial.print("FIRE Digital Value: ");
  Serial.println(digitalValue ? "HIGH" : "LOW");
  return analogValue;
}

void SSD1306(float analogValue, int sign1, int wifi_sign,float h,float t) {
  char buffer1[30];
  char temp[30];
  char humi[30];
  String formattedTime = timeClient.getFormattedTime();
  sprintf(buffer1, "天然气浓度：%.2f", analogValue);
  sprintf(temp, "温度：%.1f", t);
  sprintf(humi, "湿度：%.1f", h);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);  // use a font that supports Chinese and numeric characters
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  u8g2.drawUTF8(0, 16, buffer1);
  if (wifi_sign == 3) {u8g2.drawUTF8(0, 64, "NX");}
  else if(sign1 == 1) {u8g2.drawUTF8(0, 64, "MX");}
  u8g2.drawUTF8(24, 32, temp);
  u8g2.drawUTF8(24, 48, humi);
  u8g2.drawStr(32, 64, formattedTime.c_str()); // drawStr can be used for standard ASCII characters
  u8g2.sendBuffer();
}

float temp(){
  float t = dht.readTemperature();
  if (isnan(t))
  {
    Serial.print("Failed to read Temperature!\n");
  }
  else
  {
    temp_read = t;
  }
  return t;
}

float humi(){
  float h = dht.readHumidity();
  if (isnan(h))
  {
    Serial.print("Failed to read Humidity!\n");
  }
  else
  {
    humi_read = h;
  }
  return h;
}

void setup() {
  // init the serial
  Serial.begin(115200);
  dht.begin();
  // init the u8g2
  u8g2.begin();
  u8g2.enableUTF8Print();

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED && wifi_sign < 60) {
    delay(500);
    Serial.print(".");
    wifi_sign++;
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);

  // 初始化 BEEP 引脚
  pinMode(BEEP, OUTPUT);
  digitalWrite(BEEP, HIGH);

  // Initialize NTPClient
  timeClient.begin();
}

void loop() {
  int sign1 = 0;
  if (!client.connected()) {
    sign1 = reconnect();
  }
  client.loop();
  timeClient.update();

  // 调用MQ2函数并获取返回值
  float mq2Value = MQ2() / 10.24;
  float tempValue = temp();
  float humiValue = humi();

  // 使用返回值调用publishData和SSD1306函数
  // SSD1306(mq2Value);
  // publishData(mq2Value);

  // 报警系统
  static unsigned long lastAlarmTime = 0;
  static unsigned long lastUpdateTime = 0;
  unsigned long currentTime = millis();
  static bool beepState = false;

  if (mq2Value > 6.8 || digitalRead(FIRE_DIGITAL_PIN) == LOW) {
    if (currentTime - lastAlarmTime >= 100) { // 切换蜂鸣器状态的间隔
      lastAlarmTime = currentTime;
      beepState = !beepState;
      digitalWrite(BEEP, beepState ? LOW : HIGH);
    }
    Serial.println("!!!WARNING!!!");
    publishData(100,tempValue,humiValue);
  } else {
    digitalWrite(BEEP, HIGH);
  }

  if (currentTime - lastUpdateTime >= 1000) { // 每秒更新一次屏幕和发送数据
    lastUpdateTime = currentTime;
    SSD1306(mq2Value, sign1, wifi_sign,humiValue,tempValue);
    publishData(mq2Value,tempValue,humiValue);
  }
}

/*
主题发布：
Topic: homeassistant/sensor/kitchen_gas/config
{
  "name": "天然气浓度",
  "device_class": "gas",
  "state_topic": "kitchen/alarm/state",
  "unit_of_measurement": "ppm",
  "value_template": "{{ value_json.GAS }}",
  "unique_id": "Kitchen_GAS",
  "device": {
    "identifiers": [
      "ESP-12F"
    ],
    "name": "厨房传感器",
    "model": "ESP-12F",
    "manufacturer": "Powered By ZZH"
  }
}

homeassistant/sensor/kitchen_humi/config
{
  "name": "湿度",
  "device_class": "humidity",
  "state_topic": "kitchen/alarm/state",
  "unit_of_measurement": "%",
  "value_template": "{{ value_json.HUMI }}",
  "unique_id": "Kitchen_HUMI",
  "device": {
    "identifiers": [
      "ESP-12F"
    ],
    "name": "厨房传感器",
    "model": "ESP-12F",
    "manufacturer": "Powered By ZZH"
  }
}

homeassistant/sensor/kitchen_temp/config
{
  "name": "温度",
  "device_class": "temperature",
  "state_topic": "kitchen/alarm/state",
  "unit_of_measurement": "℃",
  "value_template": "{{ value_json.TEMP }}",
  "unique_id": "Kitchen_TEMP",
  "device": {
    "identifiers": [
      "ESP-12F"
    ],
    "name": "厨房传感器",
    "model": "ESP-12F",
    "manufacturer": "Powered By ZZH"
  }
}
*/