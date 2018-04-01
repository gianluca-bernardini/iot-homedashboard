# iot-homedashboard
This repository contains a couple of examples to implement a domotic application based on ESP8266 and Watson services
The code is split into two folders:
- the Arduino folder contains the ino scatch that implements a pub/sub iot client for the Watson IoT platform. It is supposed to be loaded on an ESP8266 with the ability to connect over the internet and reach the Watson IoT platform, to register itself as a device. The device will publish its status to a specific topic (iot-2/evt/status/fmt/json), and will receive commands on a separate topic (iot-2/cmd/update/fmt/json)
- the conversation-pi folder contains a nodejs standalone application that uses the following Watson services:
  - the Speach-to-Text service to convert voice commands to text
  - the conversation service to implement a conversation flow, to get status from the home dashboard and publish commands to it
  - the Text-to-Speach service to convert the feedback sent by the device to an audio format
The nodejs code is supposed to be run on any device that can receive voice input through a microphone and produce audio output through a speaker. In the demo implementation of this flow, a Raspberry PI has been used.
