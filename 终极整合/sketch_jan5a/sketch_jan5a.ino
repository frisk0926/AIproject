#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "Audio.h"
#include "WiFiUser.h"

#define ADC 2
#define key 0             //没有什么用处  一开始想着是用按钮按下然后结束录音的  后来没有用  也没删lol
#define I2S_DOUT      4  //25
#define I2S_BCLK      5  //27
#define I2S_LRC       6  //26
#define VOICE_PIN_1 1    //唤醒音引脚
#define VOICE_PIN_42 42  //唤醒音引脚
#define VOICE_PIN_40 40  //唤醒音引脚


// WiFi设置
const char* ssid = "your wifi name";     // 替换为您的WiFi名称
const char* password = "your wifi password"; // 替换为您的WiFi密码
const int resetPin = 0;    // 设置重置按键引脚
int connectTimeOut_s = 15; // WiFi连接超时时间，单位秒
// MQTT设置
const char* mqtt_server = "your mqttServer address"; // 替换为您的MQTT服务器地址
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
Audio audio;
// 百度STT API设置
const char* CLIENT_ID = "your baidu API ID";
const char* CLIENT_SECRET = "your baidu API secret";

// 其他全局变量
HTTPClient http_client;
hw_timer_t *timer = NULL;
const int recordTimeSeconds = 4;
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
void startRecordingProcess();

void setup_wifi() {
    // delay(10);
    Serial.println("\nConnecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
}

void mqtt_reconnect() {
    // setup_wifi();
    if (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // setup_wifi();
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
            client.subscribe("esp32/output");
            client.setCallback(mqtt_callback);
        } else {
            
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 2 seconds");
             
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(ADC, ANALOG);
    pinMode(key, INPUT_PULLUP);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(18); // default 0...21
    pinMode(VOICE_PIN_1, INPUT_PULLDOWN);
    pinMode(VOICE_PIN_42, INPUT_PULLDOWN);
    pinMode(VOICE_PIN_40, INPUT_PULLDOWN);
    
    // setup_wifi();
    connectToWiFi(connectTimeOut_s);
    client.setServer(mqtt_server, mqtt_port);

    timer = timerBegin(0, 40, true);
    timerAlarmWrite(timer, 125, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmEnable(timer);
    timerStop(timer); // 先暂停

    
    adc_data = (uint16_t *)ps_malloc(adc_data_len * sizeof(uint16_t));        //ps_malloc 指使用片外PSRAM内存
    if (!adc_data) {
    Serial.println("Failed to allocate memory for adc_data");
    }

    data_json = (char *)ps_malloc(data_json_len * sizeof(char)); // 根据需要调整大小
    if (!data_json) {
    Serial.println("Failed to allocate memory for data_json");
    }
}

void loop() {
    if (!client.connected()) {
        mqtt_reconnect();
    }
    client.loop();
    
    if (digitalRead(VOICE_PIN_1) == HIGH) {
        // 执行录音到播放音频的整个流程
        Serial.println("开始聊天");
        startRecordingProcess();
        delay(10);
    }

    if (digitalRead(VOICE_PIN_40) == HIGH) {
        // 执行录音到播放音频的整个流程
        Serial.println("删除对话内容");
        client.publish("esp32/clear_history", "clear");
        
        delay(10);
    }

    if (digitalRead(VOICE_PIN_42) == HIGH) {
        // 执行录音到播放音频的整个流程
        Serial.println("重复");
        audio.connecttohost("http://localhost:5000/latest_audio");   //这里需要修改成自己PC的IP和自行设置的端口
        delay(10);
    }
    audio.loop();
    checkDNS_HTTP();                  //检测客户端DNS&HTTP请求，也就是检查配网页面那部分
    checkConnect(true);               //检测网络连接状态，参数true表示如果断开重新连接
    
    
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
    } else {
        Serial.println("Error on HTTP request for token");
    }
    http.end();
    return token;
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
            // 发送到MQTT
            client.publish("esp32/input", payload.c_str());
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

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == "esp32/output") {
        if (message == "OK") {
            // 收到"OK"消息，连接到音频流
            audio.connecttohost("http://10.178.122.30:5000/latest_audio");
        }
    }
}

void startRecordingProcess() {
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
}

void audio_info(const char *info){
  
    Serial.print("info        "); Serial.println(info);
    
}

void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
    
}
