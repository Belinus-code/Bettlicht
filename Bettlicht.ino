#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

void setup() {
  Serial.begin(115200);

  // WLAN im Hintergrund starten (nicht blockierend!)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // OTA konfigurieren (läuft dann automatisch los, sobald das WLAN irgendwann da ist)
  ArduinoOTA.setHostname("Bettlampe"); 
  // ArduinoOTA.setPassword("deinpasswort"); 
  ArduinoOTA.begin();

  Serial.println("Setup fertig, WLAN verbindet sich im Hintergrund.");
  
  // ---> Hier kommt später dein ganzes FastLED-Setup und der Touch-Kram hin
}

void loop() {
  // OTA-Updates nur verarbeiten, wenn das WLAN auch wirklich verbunden ist
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle(); 
  }

  // ---> Hier läuft dann deine normale LED-Animation und Touch-Logik 
  // komplett ungestört weiter, egal ob mit oder ohne WLAN!
}