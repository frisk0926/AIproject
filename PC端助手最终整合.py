# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import json
import requests
import io
from flask import Flask, Response
import edge_tts
import asyncio
from threading import Thread

conversation_history = []
app = Flask(__name__)
latest_audio_data = io.BytesIO()

# MQTT设置
mqtt_server = "your mqtt_server IP"  # 替换为您的MQTT服务器地址
mqtt_port = 1883

# OpenAI GPT设置
openai_api_key = "your api-key"  # 替换为您的OpenAI API密钥


def on_connect(client, userdata, flags, rc):
    print("Connected with MQTT Server")
    client.subscribe("esp32/input")
    client.subscribe("esp32/clear_history")
    client.message_callback_add("esp32/input", on_message_esp32_input)
    client.message_callback_add("esp32/clear_history", on_message_clear_history)

def on_message_clear_history(client, userdata, msg):
    global conversation_history
    if msg.payload.decode('utf-8') == 'clear':
        conversation_history = []
        print("Conversation history cleared.")

def on_message_esp32_input(client, userdata, msg):
    print("Received a message on topic: " + msg.topic)
    try:
        # 解析接收到的消息
        message_data = json.loads(msg.payload.decode('utf-8'))
        input_text = message_data["result"][0]  # 提取result字段的内容
        # 调用OpenAI GPT
        response_content = call_chatgpt(
            input_text, openai_api_key, conversation_history)
        # 生成语音
        asyncio.run(generate_speech(response_content))
        # 发布通知到ESP32
        client.publish("esp32/output", "OK")
    except json.JSONDecodeError:
        print("Error decoding MQTT message as JSON.")
    except KeyError:
        print("Error: 'result' key not found in the received JSON.")


def call_chatgpt(prompt, api_key, conversation_history):
    url = 'url'    ## 这里替换为你的中转apikey的接口地址或官方的接口地址
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }

    # 将新提示添加到对话历史
    conversation_history.append({"role": "user", "content": prompt})

    data = {
        "model": "gpt-3.5-turbo",
        "messages": conversation_history,
        
    }

    response = requests.post(url, headers=headers, json=data)
    # 将响应添加到对话历史
    response_content = response.json()['choices'][0]['message']['content']
    conversation_history.append(
        {"role": "assistant", "content": response_content})

    return response_content


async def generate_speech(input_text):
    global latest_audio_data
    audio_content = io.BytesIO()
    communicator = edge_tts.Communicate(input_text, "zh-CN-XiaoxiaoNeural")
    async for chunk in communicator.stream():
        if chunk["type"] == "audio":
            audio_content.write(chunk["data"])
    latest_audio_data = audio_content
    print("Audio updated successfully.")
    print(
        "You can listen to the latest audio at: http://localhost:5000/latest_audio")   ##这里为你PC的IP和你设置的端口


@app.route('/latest_audio')
def latest_audio():
    latest_audio_data.seek(0)
    return Response(latest_audio_data.read(), mimetype='audio/mpeg')


def flask_thread():
    app.run(debug=True, host='0.0.0.0', port=5000, use_reloader=False)


def main():
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.connect(mqtt_server, mqtt_port, 60)
    Thread(target=flask_thread, daemon=True).start()
    mqtt_client.loop_forever()


if __name__ == '__main__':
    main()
