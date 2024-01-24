#include "WiFiUser.h"
 
const int resetPin = 0;                    //设置重置按键引脚,用于删除WiFi信息
int connectTimeOut_s = 15;                 //WiFi连接超时时间，单位秒
 
void setup() 
{
  pinMode(resetPin, INPUT_PULLUP);     //按键上拉输入模式(默认高电平输入,按下时下拉接到低电平)
  Serial.begin(115200);                //波特率
  
  LEDinit();                           //LED用于显示WiFi状态
  connectToWiFi(connectTimeOut_s);     //连接wifi，传入的是wifi连接等待时间15s
}
 
void loop() 
{
  if (!digitalRead(resetPin)) //长按5秒(P0)清除网络配置信息
  {
    delay(5000);              //哈哈哈哈，这样不准确
    if (!digitalRead(resetPin)) 
    {
      Serial.println("\n按键已长按5秒,正在清空网络连保存接信息.");
      restoreWiFi();     //删除保存的wifi信息
      ESP.restart();              //重启复位esp32
      Serial.println("已重启设备.");//有机会读到这里吗？
    }
  }
  
  checkDNS_HTTP();                  //检测客户端DNS&HTTP请求，也就是检查配网页面那部分
  checkConnect(true);               //检测网络连接状态，参数true表示如果断开重新连接
 
  delay(30); 
}
 