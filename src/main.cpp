
#include <WiFi.h>            /* WiFi library for ESP32 */
#include <Wire.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <ArduinoOTA.h>

#define PUMP_PIN 22
#define LED_PIN 23

#define wifi_ssid "Deco 804 Mesh"
#define wifi_password "yoman33333333"
#define mqtt_server "192.168.68.250"

#define topic "/home/plant"

// Function prototypes
void setup_wifi();
void reconnect();

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


    // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
    
  if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(921600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  ArduinoOTA.handle();
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(2000);
  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  
  int is_sensor_on = 1;
  
  Serial.print("\nOn: ");
  Serial.print(is_sensor_on);

  String msg = "{\"st\":\"on\",\"status\":" + String(is_sensor_on) + "}";

  client.publish(topic, msg.c_str(), true);

  delay(2000);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  is_sensor_on = 0;
  Serial.print("\nOff: ");
  Serial.print(is_sensor_on);

  msg = "{\"st\":\"off\",\"status\":" + String(is_sensor_on) + "}";

  client.publish(topic, msg.c_str(), true);
}
