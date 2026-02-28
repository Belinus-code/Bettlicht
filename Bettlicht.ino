#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"
#include <Arduino.h>



const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* ota_pass = OTA_PASS;

#define LED_PIN D9
#define TOUCH_PIN D2

int touchThreshold = 30000; 

unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // HIGH = LED ist AUS beim XIAO

  // --- WLAN & OTA ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  ArduinoOTA.setHostname("Bettlampe"); 
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.begin();
}

void loop() {
  // OTA im Hintergrund am Leben halten
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle(); 
  }

  // Den rohen Touch-Wert direkt vom Pin lesen
  long sensorValue = touchRead(TOUCH_PIN);

  // --- LED LOGIK ---
  // WICHTIG: Prüfe im Serial Monitor, ob der Wert beim Anfassen GRÖSSER (>) oder KLEINER (<) wird!
  // Beim S3 wird er oft größer. Passe das ">" Zeichen hier unten entsprechend an:
  if (sensorValue > touchThreshold) {
    digitalWrite(LED_BUILTIN, LOW);  // LOW = LED AN
  } else {
    digitalWrite(LED_BUILTIN, HIGH); // HIGH = LED AUS
  }

  // --- SERIAL MONITOR AUSGABE ---
  // Damit der Monitor nicht komplett überflutet wird, drucken wir den Wert nur alle 100ms
  if (millis() - lastPrintTime >= 100) {
    Serial.print("Touch Wert: ");
    Serial.println(sensorValue);
    lastPrintTime = millis();
  }
}