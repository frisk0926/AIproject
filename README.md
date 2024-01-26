# 基于 ChatGPT 的桌面语音助手 (云服务器版本未完成，目前依附于PC)


## 一、主要实现流程

### (1) 流程图

![语音助手流程图](https://github.com/frisk0926/AIproject/assets/129162725/9acf90bc-441d-4810-9d3e-b1c5f0e51163)



## 二、使用到的模块和API接口

### (1) 硬件模块

#### a. 唤醒检测模块：SU-03T
- **固件DIY平台：** 机芯智能元平台
- **烧录方式：** 串口烧录

#### b. 麦克风模块：max9814

#### c. 放大器模块：max98357

#### d. 扬声器
- 4欧3W扬声器或市面上其他带有正负极扬声器

#### e. 杜邦线若干

### (2) 接口API和协议

#### a. 百度STT接口

#### b. ChatGPT聊天接口
- **中转apikey**

#### c. edgeTTS接口

#### d. MQTT协议


## 三、原作者的源代码参考链接

1. ESP32-S3 AP 智能配网源码参考链接：[https://blog.csdn.net/qq_41650023/article/details/124674493](https://blog.csdn.net/qq_41650023/article/details/124674493)

2. MQTT broker和EMQX 官方下载链接：[https://www.emqx.io](https://www.emqx.io)

3. ESP32+MAX9814+百度STT调用源码参考：[https://blog.csdn.net/wojueburenshu/article/details/119244390](https://blog.csdn.net/wojueburenshu/article/details/119244390ops_request_misc=&request_id=&biz_id=102&utm_term=esp32%E8%AF%AD%E9%9F%B3%E8%BD%AC%E6%96%87%E5%AD%97&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-4-119244390.142)
4. EPS32-audioI2S库：https://github.com/schreibfaul1/ESP32-audioI2S

*PS: 链接最好复制到浏览器自行打开，直接点击打开可能出错。*

