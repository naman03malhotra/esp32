
#include <WiFi.h> /* WiFi library for ESP32 */
#include <Wire.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

#define PUMP_PIN 23
#define LED_PIN 22
#define SOIL_MOISTURE_PIN 32
#define SYSTEM_UP_PIN 26
#define SOIL_MOISTURE_THRESHOLD 2350
// #define TIME_TO_WATER "01:15:00"
#define SOIL_MALFUNCTION_CONSTANT 4095
#define TIME_TO_PUMP 60
#define TIME_TO_WAIT 60
#define SOIL_READING_INTERVAL 30

#define wifi_ssid "Deco 804 Mesh"
#define wifi_password "yoman33333333"
#define mqtt_server "192.168.68.250"
#define ANOMALY_THRESHOLD 25

#define topic "/home/plant"
#define logs_topic "/home/plant/logs"
#define logs_topic_temp "/home/plant/logs_temp"
#define trigger_topic "/home/plant/trigger"

unsigned long previousMillisOTA = 0;
unsigned long previousMillisSoil = 0;
const long intervalOTA = 2000;                          // 2 second interval
const long intervalSoil = SOIL_READING_INTERVAL * 1000; // Convert to milliseconds

// Function prototypes
void setup_wifi();
void reconnect();

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // 19800 is the offset for IST (GMT+5:30)

// int previousSoilMoisture = 0;
int is_sensor_on = 0;
int soil_moisture = 0;
String msg = "";

void callback(char *topicx, byte *payload, unsigned int length);
void trigger_water_pump_on();
void trigger_water_pump_off();
void send_soil_moisture_reading_and_status();

void setup_ota()
{
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
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
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
      } });

  ArduinoOTA.begin();
  msg = "OTA setup success";
  client.publish(logs_topic, msg.c_str(), true);
}

void log_wifi_status()
{
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  msg = "WiFi connected to Ip: " + WiFi.localIP().toString();
  client.publish(logs_topic, msg.c_str(), true);
}

void setup_wifi()
{
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
}

void callback(char *topicx, byte *payload, unsigned int length)
{
  // Convert payload to a string
  String message;
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  client.publish(logs_topic_temp, message.c_str(), true);
  if (message == "ON")
  {
    trigger_water_pump_on();
  }

  if (message == "OFF")
  {
    trigger_water_pump_off();
  }
}

void setup_mqtt()
{
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  reconnect();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    if (client.connect("ESP32Client"))
    {
      client.subscribe(trigger_topic);
      msg = "Re-connected";
      client.publish(logs_topic, msg.c_str(), true);
    }
    else
    {
      msg = "failed, rc=" + String(client.state()) + " try again in 5 seconds";
      client.publish(logs_topic, msg.c_str(), true);
      delay(5000);
    }
  }
}

void setup()
{
  setup_wifi();
  setup_mqtt();
  log_wifi_status();
  setup_ota();
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(SYSTEM_UP_PIN, OUTPUT);

  digitalWrite(SYSTEM_UP_PIN, HIGH);

  timeClient.begin();

  // Initialize previousSoilMoisture
  soil_moisture = analogRead(SOIL_MOISTURE_PIN);

  is_sensor_on = -1;
  send_soil_moisture_reading_and_status();
  is_sensor_on = 0;
}

void loop_chores()
{
  unsigned long currentMillis = millis();
  previousMillisOTA = currentMillis;

  ArduinoOTA.handle();

  soil_moisture = analogRead(SOIL_MOISTURE_PIN);
  timeClient.update();

  String currentTime = timeClient.getFormattedTime();
  String msg = "{\"chrores\": \"true\", \"status\":" + String(is_sensor_on) + ", \"soil_moisture\":" + String(soil_moisture) + ", \"current_time\":" + currentTime.c_str() + "}";
  client.publish(logs_topic_temp, msg.c_str(), true);

  if (!client.connected())
  {
    msg = "Reconnecting to Wifi...";
    client.publish(logs_topic, msg.c_str(), true);
    reconnect();
  }
  client.loop();
}

void loop()
{
  unsigned long currentMillis = millis();

  // Handle OTA and loop chores every second
  if (currentMillis - previousMillisOTA >= intervalOTA)
  {
    loop_chores();
  }

  // Handle soil moisture reading at defined intervals
  if (currentMillis - previousMillisSoil >= intervalSoil)
  {
    previousMillisSoil = currentMillis;
    send_soil_moisture_reading_and_status();
  }
}

void send_soil_moisture_reading_and_status()
{
  soil_moisture = analogRead(SOIL_MOISTURE_PIN);
  msg = "{\"status\":" + String(is_sensor_on) + ", \"soil_moisture\":" + String(soil_moisture) + "}";
  client.publish(topic, msg.c_str(), true);
}

void trigger_water_pump_on()
{
  is_sensor_on = 1;
  send_soil_moisture_reading_and_status();
  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
}

void trigger_water_pump_off()
{
  is_sensor_on = 0;
  send_soil_moisture_reading_and_status();
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}