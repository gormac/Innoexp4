#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHTesp.h"
#include <Secrets.h>

const int ledPin = 5;

const int lightPin = 36;
const int pirPin = 34;
const int dhtPin = 33;

const char mqttClientId[] =     "SimpleSensor1";

const char commandTopic[] =     "evision/restaurant/ssr1/command";

const char statusTopic[] =      "evision/restaurant/ssr1/status";
const char systemTopic[] =      "evision/restaurant/ssr1/telemetry/system";
const char climateTopic[] =     "evision/restaurant/ssr1/telemetry/climate";
const char activityTopic[] =    "evision/restaurant/ssr1/telemetry/activity";

WiFiClient wiFiClient;
PubSubClient client(wiFiClient);
DHTesp dht;

unsigned long lastReconnectAttemptAt = 0;
unsigned long lastSystemMessageAt = 0;
unsigned long lastTelemetryMessageAt = 0;

void connectToNetwork() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, wiFiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("connected at address ");
  Serial.println(WiFi.localIP());
}

void publish(const char* topic, String message){
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(message);

    char messageCharArray[message.length() + 1];
    message.toCharArray(messageCharArray, message.length() + 1);
    
    client.publish(topic, messageCharArray, true);    
}

void onReceive(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  DynamicJsonBuffer jsonBuffer(1024);
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (root.success()) {
    String commandName = root["name"];
    Serial.println(commandName);
    if (commandName == "turnLedOn") {
      turnLedOn();
    }
    else if (commandName == "turnLedOff") {
      turnLedOff();
    }
    else {
      Serial.println("Unknown command.");
    }
  }
  else {
    Serial.println("Unable to parse payload.");  
  }
}

void turnLedOn(){
  digitalWrite(ledPin, HIGH);
}

void turnLedOff(){
  digitalWrite(ledPin, LOW);
}

String getStatusMessage(bool isOnline) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& jsonObject = jsonBuffer.createObject();
  jsonObject["online"] = isOnline;
  jsonObject["ip"] = WiFi.localIP().toString();
  // jsonObject["clientId"] = mqttClientId;
  String jsonString;
  jsonObject.printTo(jsonString);
  return jsonString;  
}

String getOnlineStatusMessage() {
  return getStatusMessage(true); 
}

String getOfflineStatusMessage() {
  return getStatusMessage(false);
}

boolean connectToMqqtBroker() {  
  Serial.print("Connecting to MQTT broker...");

  String lastWill = getOfflineStatusMessage();
  char lastWillCharArray[lastWill.length() + 1];
  lastWill.toCharArray(lastWillCharArray, lastWill.length() + 1);

  if (client.connect(mqttClientId, mqttUsername, mqttPassword, statusTopic, 1, true, lastWillCharArray)) {
    turnLedOff();
    Serial.println("connected");
    publish(statusTopic, getOnlineStatusMessage());
    client.subscribe(commandTopic, 1);
  } else {
    Serial.println(mqttPassword);
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }

  return client.connected();
}

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(lightPin, INPUT);
  pinMode(pirPin, INPUT);
  dht.setup(dhtPin, DHTesp::DHT22);
  
  turnLedOn();
  Serial.begin(115200);
  connectToNetwork();
  client.setServer(mqttServer, 1883);
  client.setCallback(onReceive);
}

String getClimateMessage() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& jsonObject = jsonBuffer.createObject();
  jsonObject["light"] = (float)analogRead(lightPin) / 4096;
  
  TempAndHumidity dhtValues = dht.getTempAndHumidity();
  
  jsonObject["humidity"] = dhtValues.humidity;
  jsonObject["temperature"] = dhtValues.temperature;
  
  String jsonString;
  jsonObject.printTo(jsonString);
  return jsonString;
}

String getActivityMessage() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& jsonObject = jsonBuffer.createObject();
  jsonObject["presence"] = digitalRead(pirPin) == HIGH;
  
  String jsonString;
  jsonObject.printTo(jsonString);
  return jsonString;
}

void loop() {
  digitalWrite(ledPin, digitalRead(pirPin));
  unsigned long currentMillis = millis();

  if (!client.connected()) {
    if (currentMillis - lastReconnectAttemptAt >= 5000) {
      lastReconnectAttemptAt = currentMillis;

      if (connectToMqqtBroker()) {
        lastReconnectAttemptAt = 0;
      }     
    }
  } else {
    client.loop();

    if (currentMillis - lastSystemMessageAt >= 60000) {
      lastSystemMessageAt = currentMillis;
      
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& jsonObject = jsonBuffer.createObject();
      jsonObject["uptime"] = currentMillis / 1000;
      jsonObject["rssi"] = WiFi.RSSI();
      String jsonString;
      jsonObject.printTo(jsonString);

      publish(systemTopic, jsonString);  
    }

    if (currentMillis - lastTelemetryMessageAt >= 10000) {
      lastTelemetryMessageAt = currentMillis;
      publish(climateTopic, getClimateMessage());  
      publish(activityTopic, getActivityMessage());  
    }
  }  
}