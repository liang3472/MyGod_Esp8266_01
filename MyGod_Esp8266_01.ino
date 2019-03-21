#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

ESP8266WiFiMulti WiFiMulti;
int GPIO_0 = 0;
int GPIO_2 = 2;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
const long interval = 10000;
String currWeather = "";
const char* mqttServer = "69.171.79.169";
const int mqttPort = 1883;
const char* mqttUserName = "";
const char* mqttPassword = "";
const char* lightTopic = "weather/000001";
const char* clientId = "0000001";
const char* weatherApi = "http://api.map.baidu.com/telematics/v3/weather?location=%E5%8C%97%E4%BA%AC&output=json&ak=8UI6PHtek99nSfQdnoC0Pawf";

WiFiClient wifiClient;
PubSubClient pubClient(wifiClient);

void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFiManager wifiManager;

  wifiManager.setBreakAfterConfig(true);
  //reset settings - for testing
  //wifiManager.resetSettings();

  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("connected...yeey :)");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  pubClient.setServer(mqttServer, mqttPort);
  pubClient.setCallback(callback);

  pinMode(GPIO_2, OUTPUT);
  pinMode(GPIO_0, OUTPUT);

  //开灯
  toggleLight(true);
}

void lightning() {
  int count = random(6) + 1;
  int time = random(5) + 1;
  for (int i = 0; i < count; i++) {
    int num = random(4) + 1;
    //开灯
    digitalWrite(GPIO_0, HIGH);
    delay(num * 50);
    //关灯
    digitalWrite(GPIO_0, LOW);
    delay(100);
  }
  delay(time * 500);
}

boolean isConnected() {
  return WiFiMulti.run() == WL_CONNECTED;
}

boolean isExe() {
  currentMillis = millis();
  return currentMillis - previousMillis >= interval;
}

boolean isGoodWeather() {
  return !String("").equals(currWeather) && (String("晴").equals(currWeather) || String("多云").equals(currWeather));
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println(topic);
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  Serial.println(command);
  handlePayload(String(topic), command);
}

//处理命令
String handlePayload(String topic, String payload) {
  if (String(lightTopic).equals(topic)) {
    //light command
    if (String("lightning").equals(payload)) {
      lightning();
    } else if (String("on").equals(payload)) {
      toggleLight(true);
    } else if (String("off").equals(payload)) {
      toggleLight(false);
    }
  }
}

void toggleLight(boolean flag) {
  int lightState = flag ? HIGH : LOW;
  digitalWrite(GPIO_2, lightState);
}

void reconnect() {
  // Loop until we're reconnected
  while (!pubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (pubClient.connect(clientId, mqttUserName, mqttPassword, lightTopic, 1, 0, clientId)) {
      Serial.println("connected");
      pubClient.publish(lightTopic, clientId);
      pubClient.subscribe(lightTopic, 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!pubClient.connected()) {
    reconnect();
  }
  pubClient.loop();

  if (isConnected() && isExe()) {
    previousMillis = currentMillis;
    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, weatherApi)) {
      Serial.print("[HTTP] GET...\n");
      int httpCode = http.GET();

      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          DynamicJsonBuffer jsonBuffer;
          JsonObject& root = jsonBuffer.parseObject(http.getString());
          String weather = root["results"][0]["weather_data"][0]["weather"];
          currWeather = weather;
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.print("[HTTP} Unable to connect\n");
    }
  }

  if (!isGoodWeather()) {
    Serial.print("天气:" + currWeather + "\n");
    lightning();
  }
}
