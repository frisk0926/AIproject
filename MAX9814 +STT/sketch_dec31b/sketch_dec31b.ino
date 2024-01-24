#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"
#include <ArduinoJson.h>

// 常量和宏定义
#define key 0
#define ADC 2
const int DEV_PID = 1537;
const char* CUID = "44950592";
const char* CLIENT_ID = "PjFa4O5ioXojwyfdSTHtNMnh";
const char* CLIENT_SECRET = "7BZtKSpsIhAWHGOYB2ygNG500lO2fx6e";

// 全局变量声明
HTTPClient http_client;
hw_timer_t *timer = NULL;
const int recordTimeSeconds = 5;
const int adc_data_len = 16000 * recordTimeSeconds;
const int data_json_len = adc_data_len * 2 * 1.4;
uint16_t *adc_data;
char *data_json;
uint8_t adc_start_flag = 0;
uint8_t adc_complete_flag = 0;
uint32_t num = 0;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// 函数声明
void IRAM_ATTR onTimer();
String gainToken();
void assembleJson(String token);
void sendToSTT();

void setup() {
  Serial.begin(115200);
  pinMode(ADC, ANALOG);
  pinMode(key, INPUT_PULLUP);

  WiFi.disconnect(true);
  WiFi.begin("ASUS GJW", "12345678"); // 填写您的wifi账号密码
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    vTaskDelay(200);
  }
  Serial.println("\n-- wifi connect success! --");

  timer = timerBegin(0, 40, true);
  timerAlarmWrite(timer, 125, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmEnable(timer);
  timerStop(timer); // 先暂停

  // 动态分配PSRAM
  adc_data = (uint16_t *)ps_malloc(adc_data_len * sizeof(uint16_t));
  if (!adc_data) {
    Serial.println("Failed to allocate memory for adc_data");
  }

  data_json = (char *)ps_malloc(data_json_len * sizeof(char)); // 根据需要调整大小
  if (!data_json) {
    Serial.println("Failed to allocate memory for data_json");
  }
}

uint32_t time1, time2;
void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    if (command == "start") {
      Serial.println("开始录音");
      String token = gainToken();
      adc_start_flag = 1;
      timerStart(timer);

      while (!adc_complete_flag) {
        ets_delay_us(10);
      }

      Serial.println("录音结束");
      timerStop(timer);
      adc_complete_flag = 0;

      assembleJson(token);
      sendToSTT();

      while (!digitalRead(key));
      Serial.println("Recognition complete");
    }
  }
}

void assembleJson(String token) {
  memset(data_json, '\0', data_json_len * sizeof(char));
  strcat(data_json, "{");
  strcat(data_json, "\"format\":\"pcm\",");
  strcat(data_json, "\"rate\":16000,");
  strcat(data_json, "\"dev_pid\":1537,");
  strcat(data_json, "\"channel\":1,");
  strcat(data_json, "\"cuid\":\"44950592\",");
  strcat(data_json, "\"token\":\"");
  strcat(data_json, token.c_str());
  strcat(data_json, "\",");
  sprintf(data_json + strlen(data_json), "\"len\":%d,", adc_data_len * 2);
  strcat(data_json, "\"speech\":\"");
  strcat(data_json, base64::encode((uint8_t *)adc_data, adc_data_len * sizeof(uint16_t)).c_str());
  strcat(data_json, "\"");
  strcat(data_json, "}");
}

void sendToSTT() {
  http_client.begin("http://vop.baidu.com/server_api");
  http_client.addHeader("Content-Type", "application/json");
  int httpCode = http_client.POST(data_json);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http_client.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] POST failed, error: %s\n", http_client.errorToString(httpCode).c_str());
  }
  http_client.end();
}


void IRAM_ATTR onTimer()
{
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  if (adc_start_flag == 1)
  {
    // Serial.println("");
    adc_data[num] = analogRead(ADC);
    num++;
    if (num >= adc_data_len)
    {
      adc_complete_flag = 1;
      adc_start_flag = 0;
      num = 0;
      // Serial.println(Complete_flag);
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

String gainToken() {
    HTTPClient http;
    String token;
    String url = String("https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=") + CLIENT_ID + "&client_secret=" + CLIENT_SECRET;

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        token = doc["access_token"].as<String>();
        Serial.println(token);
    } else {
        Serial.println("Error on HTTP request for token");
    }
    http.end();
    return token;
}
