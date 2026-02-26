#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ===== WIFI =====
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASS"

// ===== FIREBASE =====
#define API_KEY "YOUR_API_KEY"
#define DATABASE_URL "YOUR_DATABASE_URL"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== PINS =====
#define SOIL_PIN A0
#define PUMP_PIN D1
#define FAN_PIN D2
#define LIGHT_PIN D3
#define DHT_PIN D4

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ===== Threshold =====
#define SOIL_THRESHOLD 600
#define TEMP_THRESHOLD 30

// ===== Light Schedule =====
#define LIGHT_ON_HOUR 18
#define LIGHT_OFF_HOUR 6

unsigned long sendDataPrevMillis = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200); // GMT+7

void setup() {
  Serial.begin(9600);
  dht.begin();
  timeClient.begin();

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {

  timeClient.update();
  int currentHour = timeClient.getHours();

  // ===== Read Sensors =====
  int soil = analogRead(SOIL_PIN);
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  bool pumpStatus = false;
  bool fanStatus = false;
  bool lightStatus = false;

  // ===== Soil Auto Pump =====
  if (soil > SOIL_THRESHOLD) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpStatus = true;
  } else {
    digitalWrite(PUMP_PIN, LOW);
  }

  // ===== Temp Auto Fan =====
  if (temp > TEMP_THRESHOLD) {
    digitalWrite(FAN_PIN, HIGH);
    fanStatus = true;
  } else {
    digitalWrite(FAN_PIN, LOW);
  }

  // ===== Light Control =====
String lightMode = "AUTO";

if (Firebase.RTDB.getString(&fbdo, "/control/lightMode")) {
  lightMode = fbdo.stringData();
}

if (lightMode == "AUTO") {

  if (currentHour >= LIGHT_ON_HOUR || currentHour < LIGHT_OFF_HOUR) {
    digitalWrite(LIGHT_PIN, HIGH);
    lightStatus = true;
  } else {
    digitalWrite(LIGHT_PIN, LOW);
    lightStatus = false;
  }

}
else if (lightMode == "ON") {
  digitalWrite(LIGHT_PIN, HIGH);
  lightStatus = true;
}
else if (lightMode == "OFF") {
  digitalWrite(LIGHT_PIN, LOW);
  lightStatus = false;
}

  // ===== Manual Override From Web =====
  if (Firebase.RTDB.getBool(&fbdo, "/control/pump")) {
    digitalWrite(PUMP_PIN, fbdo.boolData());
    pumpStatus = fbdo.boolData();
  }

  if (Firebase.RTDB.getBool(&fbdo, "/control/fan")) {
    digitalWrite(FAN_PIN, fbdo.boolData());
    fanStatus = fbdo.boolData();
  }

  // ===== Send Data Every 10 sec =====
  if (millis() - sendDataPrevMillis > 10000) {
    sendDataPrevMillis = millis();

    // Sensor Data
    Firebase.RTDB.setFloat(&fbdo, "/sensorData/temperature", temp);
    Firebase.RTDB.setFloat(&fbdo, "/sensorData/humidity", hum);
    Firebase.RTDB.setInt(&fbdo, "/sensorData/soil", soil);

    // Device Status
    Firebase.RTDB.setBool(&fbdo, "/deviceStatus/pump", pumpStatus);
    Firebase.RTDB.setBool(&fbdo, "/deviceStatus/fan", fanStatus);
    Firebase.RTDB.setBool(&fbdo, "/deviceStatus/light", lightStatus);

    // Timestamp
    String timestamp = timeClient.getFormattedTime();

    // History Log (store every 10 sec)
    FirebaseJson json;
    json.set("temperature", temp);
    json.set("humidity", hum);
    json.set("soil", soil);
    json.set("pump", pumpStatus);
    json.set("fan", fanStatus);
    json.set("light", lightStatus);
    json.set("time", timestamp);

    Firebase.RTDB.pushJSON(&fbdo, "/history", &json);
  }
}