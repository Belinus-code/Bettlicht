#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"
#include <Arduino.h>
#include <Preferences.h>
#include "animations.h"
#include <FastLED.h>


const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* ota_pass = OTA_PASS;

#define LED_PIN D9
#define TOUCH_PIN D2

CRGB leds[RGB_COUNT];

// Hier stellst du gleich deinen gemessenen Wert ein:
int touchThreshold = 28000; 

unsigned long lastPrintTime = 0;

Preferences preferences;
AnimationManager animationManager(leds, RGB_COUNT, preferences);
IAnimation* active_animation = nullptr;

const int NUM_ANIMATIONS = 8;
String animationList[NUM_ANIMATIONS] = {
  "OFF", 
  "WARM_1", 
  "WARM_2", 
  "WARM_3", 
  "WARM_4", 
  "WARM_5", 
  "NIGHT_BLUE", 
  "RAINBOW"
};
int currentAnimIndex = 0; 

// ===== Touch Logic Variables =====
bool isTouched = false;
unsigned long touchStartTime = 0;
const unsigned long LONG_PRESS_TIME = 600; 
bool longPressHandled = false;

unsigned long lastReleaseTime = 0;
const unsigned long TOUCH_COOLDOWN = 150; 

// ===== Timer Variables =====
unsigned long cycle_counter = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50;

void setup() {
  Serial.begin(115200);

  // --- WLAN & OTA ---
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  
  ArduinoOTA.setHostname("Bettlampe"); 
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.begin();
  
  preferences.begin("lampe", false); 
  // ---------------------------

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, RGB_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2600);

  animationManager.begin();

  if (animationManager.getAnimationIndex("OFF") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0, 0, "OFF");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("WARM_1") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFE4CE, 255, "WARM_1");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("WARM_2") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFCC88, 200, "WARM_2");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("WARM_3") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFB060, 150, "WARM_3");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("WARM_4") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFF8020, 110, "WARM_4");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("WARM_5") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFF4500, 100, "WARM_5");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("NIGHT_BLUE") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0x0000AA, 120, "NIGHT_BLUE");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  if (animationManager.getAnimationIndex("RAINBOW") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsPalette(0, 1, 2, 150, "RAINBOW");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  currentAnimIndex = preferences.getInt("animIndex", 0); 
  
  if (currentAnimIndex >= NUM_ANIMATIONS || currentAnimIndex < 0) {
    currentAnimIndex = 0; 
  }

  String startAnim = animationList[currentAnimIndex];
  active_animation = animationManager.getAnimationByName(startAnim);
  if (active_animation != nullptr) {
      active_animation->RestartAnimation();
  }
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle(); 
  } else {
    if (millis() - lastWiFiCheck >= 10000) {
      WiFi.disconnect(); 
      WiFi.reconnect();
      lastWiFiCheck = millis();
    }
  }

  // Touch Sensor nativ auslesen
  long sensorValue = touchRead(TOUCH_PIN);
  
  // WICHTIG: Je nach ESP-Framework muss hier < oder > stehen. 
  bool currentlyTouching = (sensorValue > touchThreshold);

  if (currentlyTouching && !isTouched && (millis() - lastReleaseTime >= TOUCH_COOLDOWN)) {
    isTouched = true;
    touchStartTime = millis();
    longPressHandled = false; 
  } 
  else if (currentlyTouching && isTouched) {
    if (!longPressHandled && (millis() - touchStartTime >= LONG_PRESS_TIME)) {
      Serial.println("Langer Druck -> OFF");
      
      currentAnimIndex = 0; 
      preferences.putInt("animIndex", currentAnimIndex); // <-- NEU: Zustand speichern!
      
      active_animation = animationManager.getAnimationByName("OFF"); 
      if (active_animation != nullptr) {
        active_animation->RestartAnimation();
      }
      longPressHandled = true; 
    }
  } 
  else if (!currentlyTouching && isTouched) {
    isTouched = false;
    lastReleaseTime = millis(); 
    
    if (!longPressHandled) {
      currentAnimIndex++;
      if (currentAnimIndex >= NUM_ANIMATIONS) {
        currentAnimIndex = 0;
      }
      
      preferences.putInt("animIndex", currentAnimIndex); // <-- NEU: Zustand speichern!
      
      String nextAnim = animationList[currentAnimIndex];
      Serial.println("Kurzer Druck -> Wechsle zu: " + nextAnim);
      
      active_animation = animationManager.getAnimationByName(nextAnim);
      if (active_animation != nullptr) {
          active_animation->RestartAnimation();
      }
    }
  }

  // ===== Festes 50ms Update-Intervall =====
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis(); 
    
    if (active_animation != nullptr) {
      bool flushRGB = active_animation->Update(cycle_counter);
      if (flushRGB) {
        FastLED.show();
      }
    }
    cycle_counter++;
  }
}