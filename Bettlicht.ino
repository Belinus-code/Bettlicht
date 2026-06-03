#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"
#include <Arduino.h>
#include <Preferences.h>
#include "animations.h"
#include <FastLED.h>
#include <vector>
#include <time.h>
#include <MqttClient.h>

// --- WEB-BIBLIOTHEKEN ---
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* ota_pass = OTA_PASS;

// ===== MQTT Setup =====
const char* mqtt_server = BROKER_HOST_ADRESS;
const int mqtt_port = BROKER_HOST_PORT;
const char* mqtt_user = BROKER_USER;
const char* mqtt_pass = BROKER_PASSWORD;

WiFiClient espClient;
MqttClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;

#define LED_PIN D9
#define TOUCH_PIN D2

CRGB leds[RGB_COUNT];

// ===== Dynamische Variablen =====
int touchThreshold = 28000;
std::vector<String> playlist;
int currentAnimIndex = 0;

Preferences preferences;
AnimationManager animationManager(leds, RGB_COUNT, preferences);
IAnimation* active_animation = nullptr;

// NEU: BCD Uhr Overlay
BCDClockOverlay clockOverlay(leds, RGB_COUNT);
bool clockEnabled = false;

// ===== Touch & Timer Variables =====
bool isTouched = false;
unsigned long touchStartTime = 0;
const unsigned long LONG_PRESS_TIME = 600;
bool longPressHandled = false;
unsigned long lastReleaseTime = 0;
const unsigned long TOUCH_COOLDOWN = 150;
unsigned long cycle_counter = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50;

unsigned long lastMqttPublish = 0;
const unsigned long MqttPublishCycle = 60000;
bool forceMqttPublish = false;

// ===== Webserver =====
AsyncWebServer server(80);

// ===== Hilfsfunktionen für die Playlist =====
void loadPlaylist() {
  String saved = preferences.getString("playlist", "OFF,WARM_1,WARM_2,NIGHT_BLUE,RAINBOW,FEUER");
  playlist.clear();
  int start = 0;
  int end = saved.indexOf(',');
  while (end != -1) {
    playlist.push_back(saved.substring(start, end));
    start = end + 1;
    end = saved.indexOf(',', start);
  }
  playlist.push_back(saved.substring(start));
}

void savePlaylist() {
  String saveStr = "";
  for (size_t i = 0; i < playlist.size(); i++) {
    saveStr += playlist[i];
    if (i < playlist.size() - 1) saveStr += ",";
  }
  preferences.putString("playlist", saveStr);
}

// ===== Das Web-Frontend (HTML/JS) =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bettlicht Config</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #121212; color: #ffffff; padding: 20px; max-width: 800px; margin: auto; }
    .card { background-color: #1e1e1e; padding: 20px; border-radius: 10px; margin-bottom: 20px; }
    input[type=number], input[type=text], select, input[type=color], input[type=range] { width: 100%; padding: 10px; margin-top: 5px; margin-bottom: 15px; box-sizing: border-box; background: #333; color: white; border: 1px solid #555; border-radius: 5px; }
    input[type=color] { height: 50px; padding: 2px; cursor: pointer; }
    button { background: #007bff; color: white; border: none; padding: 10px 15px; margin: 2px; border-radius: 5px; cursor: pointer; font-weight: bold; }
    button:hover { background: #0056b3; }
    button:disabled { background: #555; cursor: not-allowed; }
    .btn-danger { background: #dc3545; }
    .btn-danger:hover { background: #c82333; }
    .btn-success { background: #28a745; }
    .btn-success:hover { background: #218838; }
    h2 { margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 10px; }
    .list-item { display: flex; justify-content: space-between; align-items: center; background: #2a2a2a; padding: 10px; margin-bottom: 5px; border-radius: 5px; transition: 0.3s; }
    .controls { display: flex; gap: 5px; }
    .active-anim { border-left: 4px solid #007bff; background: #223344; }
    
    #editorModal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.8); z-index: 1000; overflow-y: auto;}
    .modal-content { background: #1e1e1e; margin: 5% auto; padding: 20px; width: 90%; max-width: 500px; border-radius: 10px; border: 1px solid #444; }
    .form-group { display: none; background: #2a2a2a; padding: 15px; border-radius: 5px; margin-bottom: 15px; }
    label { font-weight: bold; font-size: 0.9em; color: #aaa; }
  </style>
</head>
<body>
  <h1>Bettlicht Dashboard</h1>
  
  <div class="card">
    <h2>System & Touch</h2>
    <label>Touch Sensor Schwellwert (Threshold)</label>
    <div style="display: flex; gap: 10px; align-items: center;">
      <input type="number" id="threshInput" placeholder="Lade...">
      <button onclick="updateThreshold()">Speichern</button>
    </div>
    <small id="threshStatus" style="color: #888;"></small>

    <hr style="border-color: #333; margin: 15px 0;">
    
    <div style="display: flex; justify-content: space-between; align-items: center;">
      <label style="margin: 0;">BCD Uhr Overlay (letzte 14 LEDs)</label>
      <button id="clockBtn" class="btn-danger" onclick="toggleClock()">Aus</button>
    </div>
  </div>

  <div class="card">
    <h2>Aktuelle Playlist</h2>
    <div id="playlistContainer">Lade...</div>
    <button class="btn-success" onclick="savePlaylist()" style="margin-top: 15px; width: 100%;">Reihenfolge & Playlist speichern</button>
  </div>

  <div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #333; padding-bottom: 10px; margin-bottom: 10px;">
      <h2 style="border: none; padding: 0; margin: 0;">Alle Animationen</h2>
      <button class="btn-success" onclick="openEditor(null)">+ Neu erstellen</button>
    </div>
    <div id="animationsContainer">Lade...</div>
  </div>

  <div id="editorModal">
    <div class="modal-content">
      <h2 id="editorTitle">Animation bearbeiten</h2>
      <input type="hidden" id="animId">
      
      <label>Name (max 13 Zeichen)</label>
      <input type="text" id="animName" maxlength="13">
      
      <label>Helligkeit (0-255)</label>
      <input type="number" id="animBrightness" min="0" max="255" value="150">
      
      <label>Typ</label>
      <select id="animType" onchange="updateFormVisibility()">
        <option value="1">Static Color (Einfarbig)</option>
        <option value="2">Blink</option>
        <option value="3">Palette (Verlauf)</option>
        <option value="4">2D Feuer</option>
      </select>

      <div id="fieldsStatic" class="form-group">
        <label>Farbe</label><input type="color" id="staticColor" value="#ff8800">
      </div>
      <div id="fieldsBlink" class="form-group">
        <label>Farbe AN</label><input type="color" id="blinkColorOn" value="#ff0000">
        <label>Farbe AUS</label><input type="color" id="blinkColorOff" value="#000000">
        <label>Zyklus-Dauer (in Ticks)</label><input type="number" id="blinkCycle" min="1" max="255" value="10">
      </div>
      <div id="fieldsPalette" class="form-group">
        <label>Palette wählen</label>
        <select id="paletteId">
          <option value="0">Regenbogen</option><option value="1">Party (Bunt)</option><option value="2">Ozean (Blau/Weiß)</option><option value="3">Wald (Grün/Braun)</option><option value="4">Hitze (Klassisch)</option><option value="5">Lava (Rot/Orange)</option><option value="6">Matrix (Grün/Schwarz)</option>
        </select>
        <label>Geschwindigkeit (1-255)</label><input type="number" id="paletteSpeed" min="1" max="255" value="10">
        <label>Streckung / Delta (1-255)</label><input type="number" id="paletteDelta" min="1" max="255" value="3">
      </div>
      <div id="fieldsFire" class="form-group">
        <label>Kühlrate (Flammenhöhe, ~20-100)</label>
        <input type="range" id="fireCooling" min="10" max="150" value="45" oninput="document.getElementById('coolVal').innerText = this.value">
        <div style="text-align:right; margin-top:-10px; margin-bottom:10px;"><small id="coolVal">45</small></div>
        
        <label>Funken-Wahrscheinlichkeit (0-255)</label>
        <input type="range" id="fireSparks" min="0" max="255" value="110" oninput="document.getElementById('sparkVal').innerText = this.value">
        <div style="text-align:right; margin-top:-10px; margin-bottom:10px;"><small id="sparkVal">110</small></div>

        <label>Feuer-Farbe (Palette)</label>
        <select id="firePaletteId">
          <option value="4" selected>Standard (Feuer)</option>
          <option value="5">Lava</option>
          <option value="6">Matrix (Grünes Feuer)</option>
          <option value="2">Ozean (Blaues Feuer)</option>
          <option value="0">Regenbogen-Feuer</option>
        </select>
      </div>

      <div style="display: flex; justify-content: flex-end; gap: 10px; margin-top: 20px;">
        <button onclick="closeEditor()" style="background: #555;">Abbrechen</button>
        <button class="btn-success" onclick="saveAnimation()">Auf ESP speichern</button>
      </div>
    </div>
  </div>

  <script>
    let globalPlaylist = [];
    let globalAnimations = [];
    let currentIndex = -1;
    let clockEnabled = false;

    function loadData() {
      fetch('/api/config').then(r => r.json()).then(data => {
        if(!document.getElementById('threshInput').value) {
           document.getElementById('threshInput').value = data.threshold;
        }
        currentIndex = data.currentIndex;
        clockEnabled = data.clockEnabled;
        updateClockBtn();
      });

      fetch('/api/animations').then(r => r.json()).then(data => {
        globalPlaylist = data.playlist || [];
        globalAnimations = data.animations || [];
        renderPlaylist();
        renderAnimations();
      });
    }

    setInterval(() => {
      fetch('/api/config').then(r => r.json()).then(data => {
        if(currentIndex !== data.currentIndex) {
            currentIndex = data.currentIndex;
            renderPlaylist(); 
        }
        if(clockEnabled !== data.clockEnabled) {
            clockEnabled = data.clockEnabled;
            updateClockBtn();
        }
      });
    }, 2000);

    function updateThreshold() {
      let val = document.getElementById('threshInput').value;
      document.getElementById('threshStatus').innerText = "Speichert...";
      fetch('/api/threshold', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ value: parseInt(val) }) })
      .then(() => { document.getElementById('threshStatus').innerText = "Gespeichert!"; setTimeout(() => document.getElementById('threshStatus').innerText = "", 2000); });
    }

    function updateClockBtn() {
      let btn = document.getElementById('clockBtn');
      if(clockEnabled) {
        btn.className = "btn-success";
        btn.innerText = "An";
      } else {
        btn.className = "btn-danger";
        btn.innerText = "Aus";
      }
    }

    function toggleClock() {
      let newState = !clockEnabled;
      fetch('/api/clock', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: newState })
      }).then(() => {
        clockEnabled = newState;
        updateClockBtn();
      });
    }

    function playAnimation(index) {
      fetch('/api/play', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ index: index })
      }).then(response => {
        if(response.ok) {
          currentIndex = index;
          renderPlaylist();
        } else {
          alert("Fehler! Bitte drücke zuerst auf 'Reihenfolge & Playlist speichern', bevor du diese Animation abspielst.");
        }
      });
    }

    function renderPlaylist() {
      let html = "";
      globalPlaylist.forEach((name, index) => {
        let isActive = (index === currentIndex);
        let activeClass = isActive ? "active-anim" : "";
        
        html += `<div class="list-item ${activeClass}">
                   <span><strong>${index + 1}.</strong> ${name} ${isActive ? '<small style="color:#007bff;">(Aktiv)</small>' : ''}</span>
                   <div class="controls">
                     <button class="btn-success" onclick="playAnimation(${index})" ${isActive ? 'disabled' : ''} title="Jetzt abspielen">▶</button>
                     <button onclick="moveUp(${index})">↑</button>
                     <button onclick="moveDown(${index})">↓</button>
                     <button class="btn-danger" onclick="removeFromPlaylist(${index})">X</button>
                   </div>
                 </div>`;
      });
      document.getElementById('playlistContainer').innerHTML = html;
    }

    function moveUp(index) { if(index > 0) { let t = globalPlaylist[index]; globalPlaylist[index] = globalPlaylist[index - 1]; globalPlaylist[index - 1] = t; renderPlaylist(); } }
    function moveDown(index) { if(index < globalPlaylist.length - 1) { let t = globalPlaylist[index]; globalPlaylist[index] = globalPlaylist[index + 1]; globalPlaylist[index + 1] = t; renderPlaylist(); } }
    function removeFromPlaylist(index) { globalPlaylist.splice(index, 1); renderPlaylist(); }
    function addToPlaylist(name) { globalPlaylist.push(name); renderPlaylist(); }
    
    function savePlaylist() {
      let btn = document.querySelector('#playlistContainer + button');
      btn.innerText = "Speichert...";
      fetch('/api/playlist', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(globalPlaylist) })
      .then(() => { btn.innerText = "Reihenfolge & Playlist speichern"; alert('Playlist gesichert!'); });
    }

    function renderAnimations() {
      let html = "";
      globalAnimations.forEach(anim => {
        let typeName = anim.type === 1 ? "Static" : anim.type === 2 ? "Blink" : anim.type === 3 ? "Palette" : "2D Feuer";
        html += `<div class="list-item">
                   <span><strong>${anim.name}</strong> <small>(${typeName})</small></span>
                   <div class="controls">
                     <button class="btn-success" onclick="addToPlaylist('${anim.name}')" title="Zur Playlist hinzufügen">+</button>
                     <button onclick='openEditor(${JSON.stringify(anim)})'>Bearbeiten</button>
                     <button class="btn-danger" onclick="deleteAnimation(${anim.id})">Löschen</button>
                   </div>
                 </div>`;
      });
      document.getElementById('animationsContainer').innerHTML = html;
    }

    function deleteAnimation(id) {
      if(confirm("Wirklich dauerhaft löschen?")) { fetch('/api/animation/delete', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'id=' + id }).then(() => loadData()); }
    }

    function hexToRgb(hex) { let bigint = parseInt(hex.substring(1), 16); return { r: (bigint >> 16) & 255, g: (bigint >> 8) & 255, b: bigint & 255 }; }
    function rgbToHex(r, g, b) { return "#" + (1 << 24 | r << 16 | g << 8 | b).toString(16).slice(1).toUpperCase(); }

    function updateFormVisibility() {
      let type = document.getElementById('animType').value;
      document.getElementById('fieldsStatic').style.display = (type == 1) ? 'block' : 'none';
      document.getElementById('fieldsBlink').style.display = (type == 2) ? 'block' : 'none';
      document.getElementById('fieldsPalette').style.display = (type == 3) ? 'block' : 'none';
      document.getElementById('fieldsFire').style.display = (type == 4) ? 'block' : 'none';
    }

    function openEditor(anim) {
      document.getElementById('editorModal').style.display = 'block';
      if(anim) {
        document.getElementById('editorTitle').innerText = "Bearbeiten: " + anim.name;
        document.getElementById('animId').value = anim.id;
        document.getElementById('animName').value = anim.name;
        document.getElementById('animType').value = anim.type;
        document.getElementById('animBrightness').value = anim.data[0];
        
        if(anim.type == 1) { 
            document.getElementById('staticColor').value = rgbToHex(anim.data[3], anim.data[2], anim.data[1]); 
        } else if(anim.type == 2) { 
            document.getElementById('blinkColorOn').value = rgbToHex(anim.data[3], anim.data[2], anim.data[1]); 
            document.getElementById('blinkColorOff').value = rgbToHex(anim.data[6], anim.data[5], anim.data[4]); 
            document.getElementById('blinkCycle').value = anim.data[7]; 
        } else if(anim.type == 3) { 
            document.getElementById('paletteId').value = anim.data[1]; 
            document.getElementById('paletteSpeed').value = anim.data[2]; 
            document.getElementById('paletteDelta').value = anim.data[3]; 
        } else if(anim.type == 4) {
            document.getElementById('fireCooling').value = anim.data[1]; 
            document.getElementById('coolVal').innerText = anim.data[1]; 
            document.getElementById('fireSparks').value = anim.data[2]; 
            document.getElementById('sparkVal').innerText = anim.data[2]; 
            document.getElementById('firePaletteId').value = anim.data[3]; 
        }
      } else {
        document.getElementById('editorTitle').innerText = "Neue Animation erstellen";
        document.getElementById('animId').value = 255; 
        document.getElementById('animName').value = "Neu_" + Math.floor(Math.random() * 100);
      }
      updateFormVisibility();
    }

    function closeEditor() { document.getElementById('editorModal').style.display = 'none'; }

    function saveAnimation() {
      let type = parseInt(document.getElementById('animType').value);
      let dataBytes = new Array(16).fill(0);
      dataBytes[0] = parseInt(document.getElementById('animBrightness').value);
      
      if(type === 1) { 
          let rgb = hexToRgb(document.getElementById('staticColor').value); 
          dataBytes[1] = rgb.b; dataBytes[2] = rgb.g; dataBytes[3] = rgb.r; 
      } else if(type === 2) { 
          let rgbOn = hexToRgb(document.getElementById('blinkColorOn').value); 
          let rgbOff = hexToRgb(document.getElementById('blinkColorOff').value); 
          dataBytes[1] = rgbOn.b; dataBytes[2] = rgbOn.g; dataBytes[3] = rgbOn.r; 
          dataBytes[4] = rgbOff.b; dataBytes[5] = rgbOff.g; dataBytes[6] = rgbOff.r; 
          dataBytes[7] = parseInt(document.getElementById('blinkCycle').value); 
      } else if(type === 3) { 
          dataBytes[1] = parseInt(document.getElementById('paletteId').value); 
          dataBytes[2] = parseInt(document.getElementById('paletteSpeed').value); 
          dataBytes[3] = parseInt(document.getElementById('paletteDelta').value); 
      } else if(type === 4) {
          dataBytes[1] = parseInt(document.getElementById('fireCooling').value); 
          dataBytes[2] = parseInt(document.getElementById('fireSparks').value); 
          dataBytes[3] = parseInt(document.getElementById('firePaletteId').value); 
      }

      let payload = { id: parseInt(document.getElementById('animId').value), name: document.getElementById('animName').value, type: type, data: dataBytes };
      let btn = document.querySelector('.modal-content .btn-success');
      btn.innerText = "Speichert...";
      fetch('/api/animation', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) })
      .then(() => { btn.innerText = "Auf ESP speichern"; closeEditor(); loadData(); });
    }

    loadData();
  </script>
</body>
</html>
)rawliteral";


void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // NEU: Zeitserver einrichten (Deutsche Zeit: CET/CEST)
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  ArduinoOTA.setHostname("Bettlampe");
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.begin();

  preferences.begin("lampe", false);

  touchThreshold = preferences.getInt("threshold", 28000);

  // Uhr-Status laden
  clockEnabled = preferences.getBool("clockEnabled", false);
  clockOverlay.setEnabled(clockEnabled);

  loadPlaylist();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, RGB_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2600);

  animationManager.begin();

  // Standard-Animationen erstellen
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
  // NEU: Das Feuer standardmäßig anlegen, falls noch nicht da
  if (animationManager.getAnimationIndex("FEUER") == -1) {
    AnimationSetting* newSettings = animationManager.createSettingsFire2D(45, 110, 4, 255, "FEUER");
    animationManager.createAnimation(newSettings);
    delete newSettings;
  }

  // Aktuellen Index laden
  currentAnimIndex = preferences.getInt("animIndex", 0);
  if (currentAnimIndex >= playlist.size() || currentAnimIndex < 0) {
    currentAnimIndex = 0;
  }

  active_animation = animationManager.getAnimationByName(playlist[currentAnimIndex]);
  if (active_animation != nullptr) {
    active_animation->RestartAnimation();
  }

  // ===== WEBSERVER ROUTEN =====

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<200> doc;
    doc["threshold"] = touchThreshold;
    doc["currentAnim"] = playlist[currentAnimIndex];
    doc["currentIndex"] = currentAnimIndex;
    doc["clockEnabled"] = clockEnabled;  // NEU: Uhr-Status mitsenden
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  AsyncCallbackJsonWebHandler* threshHandler = new AsyncCallbackJsonWebHandler("/api/threshold", [](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if (jsonObj.containsKey("value")) {
      touchThreshold = jsonObj["value"].as<int>();
      preferences.putInt("threshold", touchThreshold);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.addHandler(threshHandler);

  // NEU: Route für den Uhr-Schalter
  AsyncCallbackJsonWebHandler* clockHandler = new AsyncCallbackJsonWebHandler("/api/clock", [](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if (jsonObj.containsKey("enabled")) {
      clockEnabled = jsonObj["enabled"].as<bool>();
      clockOverlay.setEnabled(clockEnabled);
      if (!clockOverlay.isEnabled()) active_animation->RestartAnimation();
      preferences.putBool("clockEnabled", clockEnabled);
      forceMqttPublish = true;
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.addHandler(clockHandler);

  server.on("/api/animations", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(4096);

    JsonArray pl = doc.createNestedArray("playlist");
    for (size_t i = 0; i < playlist.size(); i++) {
      pl.add(playlist[i]);
    }

    JsonArray anims = doc.createNestedArray("animations");
    for (int i = 0; i < 100; i++) {
      IAnimation* anim = animationManager.getAnimation(i);
      if (anim != nullptr) {
        AnimationSetting s;
        anim->getAnimationSetting(&s);

        JsonObject obj = anims.createNestedObject();
        obj["id"] = s.id;
        obj["name"] = String(s.name);
        obj["type"] = s.type;

        JsonArray dataArr = obj.createNestedArray("data");
        for (int d = 0; d < 16; d++) {
          dataArr.add(s.data[d]);
        }
      }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  AsyncCallbackJsonWebHandler* playlistHandler = new AsyncCallbackJsonWebHandler("/api/playlist", [](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonArray jsonArray = json.as<JsonArray>();
    playlist.clear();
    for (JsonVariant v : jsonArray) {
      playlist.push_back(v.as<String>());
    }
    savePlaylist();
    currentAnimIndex = 0;
    preferences.putInt("animIndex", currentAnimIndex);
    request->send(200, "text/plain", "Playlist gespeichert");
  });
  server.addHandler(playlistHandler);

  AsyncCallbackJsonWebHandler* saveAnimHandler = new AsyncCallbackJsonWebHandler("/api/animation", [](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject obj = json.as<JsonObject>();
    AnimationSetting s;

    s.id = obj["id"] | 255;
    String name = obj["name"].as<String>();
    strncpy(s.name, name.c_str(), 13);
    s.type = obj["type"].as<int>();

    JsonArray dataArr = obj["data"].as<JsonArray>();
    for (int i = 0; i < 16; i++) {
      s.data[i] = dataArr[i] | 0;
    }

    if (s.id == 255 || animationManager.getAnimation(s.id) == nullptr) {
      animationManager.createAnimation(&s, true);
    } else {
      IAnimation* anim = animationManager.getAnimation(s.id);
      anim->applyAnimationSetting(&s);
      animationManager.saveAnimation(&s);
    }
    request->send(200, "text/plain", "Animation gespeichert");
  });
  server.addHandler(saveAnimHandler);

  server.on("/api/animation/delete", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("id", true)) {
      int id = request->getParam("id", true)->value().toInt();
      animationManager.deleteAnimation(id);
      request->send(200, "text/plain", "Gelöscht");
    } else {
      request->send(400, "text/plain", "ID fehlt");
    }
  });

  AsyncCallbackJsonWebHandler* playHandler = new AsyncCallbackJsonWebHandler("/api/play", [](AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if (jsonObj.containsKey("index")) {
      int idx = jsonObj["index"].as<int>();
      if (idx >= 0 && idx < playlist.size()) {
        currentAnimIndex = idx;
        preferences.putInt("animIndex", currentAnimIndex);

        active_animation = animationManager.getAnimationByName(playlist[currentAnimIndex]);
        if (active_animation != nullptr) {
          active_animation->RestartAnimation();
        }
        forceMqttPublish = true;
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Index Fehler");
      }
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.addHandler(playHandler);

  server.begin();
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    UpdateMqtt();
  } else {
    if (millis() - lastWiFiCheck >= 10000) {
      WiFi.disconnect();
      WiFi.reconnect();
      lastWiFiCheck = millis();
      forceMqttPublish = true;
    }
  }

  long sensorValue = touchRead(TOUCH_PIN);
  bool currentlyTouching = (sensorValue > touchThreshold);

  if (currentlyTouching && !isTouched && (millis() - lastReleaseTime >= TOUCH_COOLDOWN)) {
    isTouched = true;
    touchStartTime = millis();
    longPressHandled = false;
  } else if (currentlyTouching && isTouched) {
    if (!longPressHandled && (millis() - touchStartTime >= LONG_PRESS_TIME)) {
      forceMqttPublish = true;
      if (currentAnimIndex == 0) {
        clockEnabled = !clockEnabled;
        clockOverlay.setEnabled(clockEnabled);
        if (!clockEnabled) active_animation->RestartAnimation();
        preferences.putBool("clockEnabled", clockEnabled);
      } else {
        // Lampe ist AN -> Wir schalten die Lampe AUS (Index 0)
        currentAnimIndex = 0;
        preferences.putInt("animIndex", currentAnimIndex);
        active_animation = animationManager.getAnimationByName(playlist[0]);
        if (active_animation != nullptr) {
          active_animation->RestartAnimation();
        }
      }
      longPressHandled = true;
    }
  } else if (!currentlyTouching && isTouched) {
    isTouched = false;
    lastReleaseTime = millis();

    if (!longPressHandled) {
      currentAnimIndex++;
      if (currentAnimIndex >= playlist.size()) {
        currentAnimIndex = 0;
      }

      preferences.putInt("animIndex", currentAnimIndex);

      active_animation = animationManager.getAnimationByName(playlist[currentAnimIndex]);
      if (active_animation != nullptr) {
        active_animation->RestartAnimation();
      }
      forceMqttPublish = true;
    }
  }

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();

    bool flushRGB = false;

    // 1. Normale Animation berechnen
    if (active_animation != nullptr) {
      flushRGB = active_animation->Update(cycle_counter);
    }

    // 2. Uhr-Overlay drüberstempeln (falls aktiviert)
    if (clockOverlay.isEnabled()) {
      struct tm timeinfo;
      // Holt die Zeit asynchron, blockiert nicht (Timeout 0)
      if (getLocalTime(&timeinfo, 0)) {
        // Sekundentakt berechnen: Die ersten 1000ms der Sekunde leuchten, danach aus
        bool blinkTick = (millis() % 2000) < 1000;
        clockOverlay.UpdateAndDraw(timeinfo.tm_hour, timeinfo.tm_min, blinkTick);
        flushRGB = true;  // Wir haben etwas auf die LEDs geschrieben, also sicherstellen, dass show() aufgerufen wird
      }
    }

    // 3. Wenn sich etwas geändert hat -> Update an Streifen senden
    if (flushRGB) {
      FastLED.show();
    }

    cycle_counter++;
  }
}

// ===== MQTT Callback (Empfangen) =====
void mqttCallback(int messageSize) {
  String topic = mqttClient.messageTopic();
  String payload = "";
  while (mqttClient.available()) {
    payload += (char)mqttClient.read();
  }

  payload.trim();
  topic.trim();

  Serial.print("Received message on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(payload);

  if (topic == "linus/haydn17/kellerzimmer/bettlicht/command")
  {
    // Check if the requested animation exists in the AnimationManager
    IAnimation* requestedAnim = animationManager.getAnimationByName(payload);
    
    if (requestedAnim != nullptr)
    {
      // Set and start the new animation
      active_animation = requestedAnim;
      active_animation->RestartAnimation();
      
      // Find the matching index in the playlist to keep the Web-UI in sync
      for (size_t i = 0; i < playlist.size(); i++)
      {
        if (playlist[i] == payload)
        {
          currentAnimIndex = i;
          preferences.putInt("animIndex", currentAnimIndex);
          break;
        }
      }
    }
    else
    {
      Serial.println("MQTT Error: Animation not found -> " + payload);
    }
    forceMqttPublish = true;
  }
  else if (topic == "linus/haydn17/kellerzimmer/bettlicht/clock_command")
  {
    if (payload == "ON")
    {
      clockEnabled = true;
      clockOverlay.setEnabled(clockEnabled);
      if (!clockOverlay.isEnabled()) active_animation->RestartAnimation();
      preferences.putBool("clockEnabled", clockEnabled);
    }
    else if (payload == "OFF")
    {
      clockEnabled = false;
      clockOverlay.setEnabled(clockEnabled);
      if (!clockOverlay.isEnabled()) active_animation->RestartAnimation();
      preferences.putBool("clockEnabled", clockEnabled);
    }
    forceMqttPublish = true;
  }
}

void PublishData()
{
  if (active_animation != nullptr) {
      mqttClient.beginMessage("linus/haydn17/kellerzimmer/bettlicht/status", true, 1);  // topic, retained, qos
      mqttClient.print(active_animation->GetName());
      mqttClient.endMessage();

      mqttClient.beginMessage("linus/haydn17/kellerzimmer/bettlicht/status_dig", true, 1);  // topic, retained, qos
      mqttClient.print(active_animation->GetName() == "OFF" ? "0" : "1");
      mqttClient.endMessage();

      mqttClient.beginMessage("linus/haydn17/kellerzimmer/bettlicht/status_clock", true, 1);  // topic, retained, qos
      mqttClient.print(clockEnabled ? "1" : "0");
      mqttClient.endMessage();
    }
    // Dont need else because if animation is nullptr than something is fucked up
}

void UpdateMqtt() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT Connection lost. Reconnect.");
    ConnectMqtt();
  }
  mqttClient.poll();
  if (millis() - lastMqttPublish >= MqttPublishCycle || forceMqttPublish) {
    lastMqttPublish = millis();
    forceMqttPublish = false;
    PublishData();
  }
}

void ConnectMqtt() {
  mqttClient.onMessage(mqttCallback);
  mqttClient.setUsernamePassword(mqtt_user, mqtt_pass);

  String clientId = "Bettlicht-ESP-" + String(random(0xffff), HEX);
  mqttClient.setId(clientId);

  Serial.print("Connecting to MQTT broker '");
  Serial.print(mqtt_server);
  Serial.print("'...");
  while (!mqttClient.connect(mqtt_server, mqtt_port)) {
    Serial.print(".");
    delay(700);
  }
  Serial.println("\nMQTT connected!");

  mqttClient.subscribe("linus/haydn17/kellerzimmer/bettlicht/command", 2);
  mqttClient.subscribe("linus/haydn17/kellerzimmer/bettlicht/clock_command", 2);
  forceMqttPublish = true;
}