#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"
#include <Arduino.h>
#include <Preferences.h>
#include "animations.h"
#include <FastLED.h>
#include <vector>

// --- NEUE WEB-BIBLIOTHEKEN ---
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* ota_pass = OTA_PASS;

#define LED_PIN D9
#define TOUCH_PIN D2

CRGB leds[RGB_COUNT];

// ===== Dynamische Variablen =====
int touchThreshold = 28000; 
std::vector<String> playlist; // Ersetzt das alte statische Array
int currentAnimIndex = 0; 

Preferences preferences;
AnimationManager animationManager(leds, RGB_COUNT, preferences);
IAnimation* active_animation = nullptr;

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

// ===== Webserver =====
AsyncWebServer server(80);

// ===== Hilfsfunktionen für die Playlist =====
void loadPlaylist() {
  // Lädt die Liste als String, Standardwert falls leer:
  String saved = preferences.getString("playlist", "OFF,WARM_1,WARM_2,WARM_3,WARM_4,WARM_5,NIGHT_BLUE,RAINBOW");
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
    input[type=number], input[type=text], select, input[type=color] { width: 100%; padding: 10px; margin-top: 5px; margin-bottom: 15px; box-sizing: border-box; background: #333; color: white; border: 1px solid #555; border-radius: 5px; }
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
          <option value="0">Regenbogen</option><option value="1">Party (Bunt)</option><option value="2">Ozean (Blau/Weiß)</option><option value="3">Wald (Grün/Braun)</option><option value="4">Hitze (Feuer)</option><option value="5">Lava (Rot/Orange)</option><option value="6">Matrix (Grün/Schwarz)</option>
        </select>
        <label>Geschwindigkeit (1-255)</label><input type="number" id="paletteSpeed" min="1" max="255" value="10">
        <label>Streckung / Delta (1-255)</label><input type="number" id="paletteDelta" min="1" max="255" value="3">
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

    function loadData() {
      fetch('/api/config').then(r => r.json()).then(data => {
        if(!document.getElementById('threshInput').value) {
           document.getElementById('threshInput').value = data.threshold;
        }
        currentIndex = data.currentIndex;
      });

      fetch('/api/animations').then(r => r.json()).then(data => {
        globalPlaylist = data.playlist || [];
        globalAnimations = data.animations || [];
        renderPlaylist();
        renderAnimations();
      });
    }

    // Polling: Fragt alle 2 Sekunden unauffällig nach, ob sich das Licht geändert hat (z.B. durch Touch am Bett)
    setInterval(() => {
      fetch('/api/config').then(r => r.json()).then(data => {
        if(currentIndex !== data.currentIndex) {
            currentIndex = data.currentIndex;
            renderPlaylist(); // Update UI
        }
      });
    }, 2000);

    function updateThreshold() {
      let val = document.getElementById('threshInput').value;
      document.getElementById('threshStatus').innerText = "Speichert...";
      fetch('/api/threshold', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ value: parseInt(val) }) })
      .then(() => { document.getElementById('threshStatus').innerText = "Gespeichert!"; setTimeout(() => document.getElementById('threshStatus').innerText = "", 2000); });
    }

    // --- LIVE STEUERUNG ---
    function playAnimation(index) {
      fetch('/api/play', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ index: index })
      }).then(response => {
        // Prüfen, ob der ESP32 den Befehl wirklich akzeptiert hat
        if (response.ok) {
          currentIndex = index;
          renderPlaylist();
        } else {
          // Wenn der ESP32 400 sendet (Index Fehler), weise den Nutzer darauf hin
          alert("Fehler! Bitte drücke zuerst auf 'Reihenfolge & Playlist speichern', bevor du neue Einträge abspielst.");
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
        let typeName = anim.type === 1 ? "Static" : anim.type === 2 ? "Blink" : "Palette";
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
    }

    function openEditor(anim) {
      document.getElementById('editorModal').style.display = 'block';
      if(anim) {
        document.getElementById('editorTitle').innerText = "Bearbeiten: " + anim.name;
        document.getElementById('animId').value = anim.id;
        document.getElementById('animName').value = anim.name;
        document.getElementById('animType').value = anim.type;
        document.getElementById('animBrightness').value = anim.data[0];
        if(anim.type == 1) { document.getElementById('staticColor').value = rgbToHex(anim.data[3], anim.data[2], anim.data[1]); } 
        else if(anim.type == 2) { document.getElementById('blinkColorOn').value = rgbToHex(anim.data[3], anim.data[2], anim.data[1]); document.getElementById('blinkColorOff').value = rgbToHex(anim.data[6], anim.data[5], anim.data[4]); document.getElementById('blinkCycle').value = anim.data[7]; } 
        else if(anim.type == 3) { document.getElementById('paletteId').value = anim.data[1]; document.getElementById('paletteSpeed').value = anim.data[2]; document.getElementById('paletteDelta').value = anim.data[3]; }
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
      if(type === 1) { let rgb = hexToRgb(document.getElementById('staticColor').value); dataBytes[1] = rgb.b; dataBytes[2] = rgb.g; dataBytes[3] = rgb.r; } 
      else if(type === 2) { let rgbOn = hexToRgb(document.getElementById('blinkColorOn').value); let rgbOff = hexToRgb(document.getElementById('blinkColorOff').value); dataBytes[1] = rgbOn.b; dataBytes[2] = rgbOn.g; dataBytes[3] = rgbOn.r; dataBytes[4] = rgbOff.b; dataBytes[5] = rgbOff.g; dataBytes[6] = rgbOff.r; dataBytes[7] = parseInt(document.getElementById('blinkCycle').value); } 
      else if(type === 3) { dataBytes[1] = parseInt(document.getElementById('paletteId').value); dataBytes[2] = parseInt(document.getElementById('paletteSpeed').value); dataBytes[3] = parseInt(document.getElementById('paletteDelta').value); }

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
  
  ArduinoOTA.setHostname("Bettlampe"); 
  ArduinoOTA.setPassword(ota_pass);
  ArduinoOTA.begin();
  
  preferences.begin("lampe", false); 

  // --- Werte laden ---
  touchThreshold = preferences.getInt("threshold", 28000);
  loadPlaylist();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, RGB_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2600);

  animationManager.begin();

  // Standard-Animationen erstellen (nur wenn nicht vorhanden)
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
  // (Ich habe WARM_2 bis WARM_5 hier im Setup gekürzt, sie sind ja eh in den Preferences gespeichert, 
  // falls du sie schon mal laufen hattest. Das spart Code.)

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
  
  // 1. Liefert die HTML Seite aus
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // 2. API: Liefert den aktuellen Status als JSON
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<200> doc;
    doc["threshold"] = touchThreshold;
    doc["currentAnim"] = playlist[currentAnimIndex];
    doc["currentIndex"] = currentAnimIndex; // <-- NEU: Verrät der Website, wo wir gerade sind
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // 3. API: Empfängt einen neuen Threshold
  AsyncCallbackJsonWebHandler* threshHandler = new AsyncCallbackJsonWebHandler("/api/threshold", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if(jsonObj.containsKey("value")) {
      touchThreshold = jsonObj["value"].as<int>();
      preferences.putInt("threshold", touchThreshold); // Direkt dauerhaft speichern!
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.addHandler(threshHandler);

  // 4. API: Alle Animationen und die Playlist abrufen
  server.on("/api/animations", HTTP_GET, [](AsyncWebServerRequest *request){
    // Wir brauchen einen größeren Puffer für alle Animationen
    DynamicJsonDocument doc(4096); 
    
    // Die Playlist laden
    JsonArray pl = doc.createNestedArray("playlist");
    for(size_t i = 0; i < playlist.size(); i++) {
      pl.add(playlist[i]);
    }

    // Alle gespeicherten Animationen auslesen
    JsonArray anims = doc.createNestedArray("animations");
    for(int i = 0; i < 100; i++) {
      IAnimation* anim = animationManager.getAnimation(i);
      if(anim != nullptr) {
        AnimationSetting s;
        anim->getAnimationSetting(&s);
        
        JsonObject obj = anims.createNestedObject();
        obj["id"] = s.id;
        obj["name"] = String(s.name);
        obj["type"] = s.type; // 1=Static, 2=Blink, 3=Palette
        
        // Die 16 Daten-Bytes rüberschieben
        JsonArray dataArr = obj.createNestedArray("data");
        for(int d = 0; d < 16; d++) {
          dataArr.add(s.data[d]);
        }
      }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // 5. API: Eine neue Playlist (Reihenfolge) speichern
  AsyncCallbackJsonWebHandler* playlistHandler = new AsyncCallbackJsonWebHandler("/api/playlist", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonArray jsonArray = json.as<JsonArray>();
    playlist.clear();
    for(JsonVariant v : jsonArray) {
      playlist.push_back(v.as<String>());
    }
    savePlaylist(); // Deine Hilfsfunktion von vorhin
    
    // Aktuelle Animation sicherheitshalber auf 0 setzen, falls wir eine gelöscht haben
    currentAnimIndex = 0;
    preferences.putInt("animIndex", currentAnimIndex);
    
    request->send(200, "text/plain", "Playlist gespeichert");
  });
  server.addHandler(playlistHandler);

  // 6. API: Eine Animation erstellen oder überschreiben
  AsyncCallbackJsonWebHandler* saveAnimHandler = new AsyncCallbackJsonWebHandler("/api/animation", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    AnimationSetting s;
    
    // Wenn ID = 255 mitgeschickt wird, werten wir das als "Neue Animation"
    s.id = obj["id"] | 255; 
    String name = obj["name"].as<String>();
    strncpy(s.name, name.c_str(), 13);
    s.type = obj["type"].as<int>();
    
    JsonArray dataArr = obj["data"].as<JsonArray>();
    for(int i=0; i<16; i++) {
      s.data[i] = dataArr[i] | 0;
    }

    if(s.id == 255 || animationManager.getAnimation(s.id) == nullptr) {
      // Neu anlegen
      animationManager.createAnimation(&s, true); 
    } else {
      // Bestehende überschreiben
      IAnimation* anim = animationManager.getAnimation(s.id);
      anim->applyAnimationSetting(&s);
      animationManager.saveAnimation(&s);
    }
    request->send(200, "text/plain", "Animation gespeichert");
  });
  server.addHandler(saveAnimHandler);

  // 7. API: Eine Animation komplett löschen
  server.on("/api/animation/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("id", true)) {
      int id = request->getParam("id", true)->value().toInt();
      animationManager.deleteAnimation(id);
      request->send(200, "text/plain", "Gelöscht");
    } else {
      request->send(400, "text/plain", "ID fehlt");
    }
  });

  // 8. API: Live-Steuerung (Aktuelle Animation umschalten)
  AsyncCallbackJsonWebHandler* playHandler = new AsyncCallbackJsonWebHandler("/api/play", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    if(jsonObj.containsKey("index")) {
      int idx = jsonObj["index"].as<int>();
      if(idx >= 0 && idx < playlist.size()) {
        currentAnimIndex = idx;
        preferences.putInt("animIndex", currentAnimIndex);
        
        active_animation = animationManager.getAnimationByName(playlist[currentAnimIndex]);
        if(active_animation != nullptr) {
          active_animation->RestartAnimation();
        }
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Index Fehler");
      }
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.addHandler(playHandler);

  // Server starten
  server.begin();
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

  long sensorValue = touchRead(TOUCH_PIN);
  bool currentlyTouching = (sensorValue > touchThreshold); // Bei dir war es >

  if (currentlyTouching && !isTouched && (millis() - lastReleaseTime >= TOUCH_COOLDOWN)) {
    isTouched = true;
    touchStartTime = millis();
    longPressHandled = false; 
  } 
  else if (currentlyTouching && isTouched) {
    if (!longPressHandled && (millis() - touchStartTime >= LONG_PRESS_TIME)) {
      currentAnimIndex = 0; // Gehe zu Index 0 (OFF)
      preferences.putInt("animIndex", currentAnimIndex); 
      active_animation = animationManager.getAnimationByName(playlist[0]); 
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
      if (currentAnimIndex >= playlist.size()) {
        currentAnimIndex = 0;
      }
      
      preferences.putInt("animIndex", currentAnimIndex); 
      
      active_animation = animationManager.getAnimationByName(playlist[currentAnimIndex]);
      if (active_animation != nullptr) {
          active_animation->RestartAnimation();
      }
    }
  }

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