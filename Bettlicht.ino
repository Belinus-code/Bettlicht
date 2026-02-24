#include <Arduino.h>
#include <CapacitiveSensor.h>
#include <FastLED.h>
#include <EEPROM.h> // <-- NEU: Für den permanenten Speicher
#include "animations.h"

// ===== Hardware Setup =====
#define LED_PIN 9

CRGB leds[RGB_COUNT];

// Touch Sensor Setup (1 Megohm zwischen Pin 4 und 2, Folie an Pin 2)
CapacitiveSensor touchSensor = CapacitiveSensor(4, 2);
long touchThreshold = 150; 

// ===== Animations Setup =====
AnimationManager animationManager(leds, RGB_COUNT);
IAnimation* active_animation = nullptr;

// Neue Liste mit Abstufungen von Weiß zu Orange
const int NUM_ANIMATIONS = 8;
String animationList[NUM_ANIMATIONS] = {
  "OFF", 
  "WARM_1", // Neutrales Weiß zum Lesen
  "WARM_2", // Angenehmes Warmweiß
  "WARM_3", // Klassische Glühbirne
  "WARM_4", // Bernstein / Kerzenschein
  "WARM_5", // Tiefes, weiches Orange
  "NIGHT_BLUE", 
  "RAINBOW"
};
int currentAnimIndex = 0; 

// ===== Touch Logic Variables =====
bool isTouched = false;
unsigned long touchStartTime = 0;
const unsigned long LONG_PRESS_TIME = 800; // 800 Millisekunden für einen langen Druck
bool longPressHandled = false;

// Cooldown / Entprellen
unsigned long lastReleaseTime = 0;
const unsigned long TOUCH_COOLDOWN = 200; // 200ms Sperrzeit nach dem Loslassen

// ===== Timer Variables =====
unsigned long cycle_counter = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50; // Animationen alle 50ms updaten

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, RGB_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2400);

  animationManager.begin();

  // 0. OFF
  if (animationManager.getAnimationIndex("OFF") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0, 0, "OFF");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }
  
  // 1. WARM_1 (Heller! Helligkeit jetzt auf 255)
  if (animationManager.getAnimationIndex("WARM_1") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFE4CE, 255, "WARM_1");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 2. WARM_2 (Helligkeit jetzt auf 200)
  if (animationManager.getAnimationIndex("WARM_2") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFCC88, 200, "WARM_2");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 3. WARM_3 (Helligkeit jetzt auf 150)
  if (animationManager.getAnimationIndex("WARM_3") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFFB060, 150, "WARM_3");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 4. WARM_4 (Bernstein / Kerzenschein)
  if (animationManager.getAnimationIndex("WARM_4") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFF8020, 110, "WARM_4");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 5. WARM_5 (Tiefes, dunkles Orange)
  if (animationManager.getAnimationIndex("WARM_5") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0xFF4500, 100, "WARM_5");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 6. NIGHT_BLUE 
  if (animationManager.getAnimationIndex("NIGHT_BLUE") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsStaticColor(0x0000AA, 120, "NIGHT_BLUE");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // 7. RAINBOW 
  if (animationManager.getAnimationIndex("RAINBOW") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsPalette(0, 1, 2, 150, "RAINBOW");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // ===== Letzten Zustand aus dem EEPROM laden =====
  currentAnimIndex = EEPROM.read(0); // Lese Adresse 0
  
  // Sicherheitscheck: Wenn das EEPROM ganz frisch ist, steht da oft 255 drin.
  if (currentAnimIndex >= NUM_ANIMATIONS || currentAnimIndex < 0) {
    currentAnimIndex = 0; // Dann gehen wir sicherheitshalber auf OFF
  }

  // Startzustand aus dem geladenen Index setzen
  String startAnim = animationList[currentAnimIndex];
  active_animation = animationManager.getAnimationByName(startAnim);
  if (active_animation != nullptr) {
      active_animation->RestartAnimation();
  }
}

void loop() {
  long sensorValue = touchSensor.capacitiveSensor(30);
  bool currentlyTouching = (sensorValue > touchThreshold);

  if (currentlyTouching && !isTouched && (millis() - lastReleaseTime >= TOUCH_COOLDOWN)) {
    isTouched = true;
    touchStartTime = millis();
    longPressHandled = false; 
  } 
  else if (currentlyTouching && isTouched) {
    if (!longPressHandled && (millis() - touchStartTime >= LONG_PRESS_TIME)) {
      Serial.println("Langer Druck -> OFF");
      
      currentAnimIndex = 0; // Zurück auf OFF
      EEPROM.update(0, currentAnimIndex); // <-- NEU: Zustand speichern!
      
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
      
      EEPROM.update(0, currentAnimIndex); // <-- NEU: Zustand speichern!
      
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