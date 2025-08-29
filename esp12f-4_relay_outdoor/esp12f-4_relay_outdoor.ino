/******************************************************
 * ESP12F_Relay_X4 – 4 Relais per Web + OTA-Update (Progress)
 * + Version Info + Reboot/Reachability-Check (Polling /about)
 * + WiFiManager AutoConnect AP "ESP12F_Relay_X4"
 * 
 * DE: Diese Version nutzt WiFiManager; feste SSID/Passwort entfallen.
 * EN: This version uses WiFiManager; fixed SSID/password removed.
 * 
 * Board: NodeMCU 1.0 (ESP-12E Module)
 ******************************************************/

#include <Arduino.h>
#include <ESP8266WiFi.h>                 // DE: WLAN-Basis für ESP8266 / EN: WiFi core for ESP8266
#include <ESP8266WebServer.h>            // DE: Einfache HTTP-Server-API / EN: Simple HTTP server API
#include <ESP8266mDNS.h>                 // DE: mDNS (hostname.local) / EN: mDNS (hostname.local)
#include <ESP8266HTTPUpdateServer.h>     // DE: OTA-HTTP-Update / EN: OTA HTTP update
#include <DNSServer.h>                   // DE: Für WiFiManager Captive Portal / EN: For WiFiManager captive portal
#include <WiFiManager.h>                 // DE: WiFiManager Bibliothek / EN: WiFiManager library

// ---------- Server ----------
ESP8266WebServer server(80);             // DE: HTTP-Server auf Port 80 / EN: HTTP server on port 80
ESP8266HTTPUpdateServer httpUpdater;     // DE: OTA-Update-Endpunkt / EN: OTA update endpoint

// ---------- WLAN ----------
const char* HOSTNAME = "esp-terrasse";   // DE: Hostname (nur a-z0-9-) / EN: Hostname (lowercase, digits, hyphen)
// DE: Feste Zugangsdaten entfernt – WiFiManager übernimmt Verbindung/Portal.
// EN: Removed fixed credentials – WiFiManager handles connect/portal.

// ---------- Firmware-Metadaten ----------
const char* FW_NAME    = HOSTNAME;                // DE: Anzeigename = Hostname / EN: Display name = hostname
const char* FW_VERSION = "1.0.1";                 // DE: Version angehoben für WiFiManager-Integration / EN: Bumped for WiFiManager integration
const char* FW_BUILD   = __DATE__ " " __TIME__;   // DE: Kompilierzeit / EN: Compile timestamp

// ---------- OTA-Login ----------
const char* update_username = "esp-admin";   // DE: ändern! / EN: change!
const char* update_password = "esp-admin";   // DE: ändern! / EN: change!

// ---------- Relais-Logik ----------
const bool ACTIVE_LOW = false;                                          // DE: Falls Relais Low-aktiv sind / EN: If relays are active-low
#define RELAY_ON(pin)   digitalWrite((pin), ACTIVE_LOW ? LOW  : HIGH)   // DE: Relais EIN / EN: Relay ON
#define RELAY_OFF(pin)  digitalWrite((pin), ACTIVE_LOW ? HIGH : LOW)    // DE: Relais AUS / EN: Relay OFF

const uint8_t RELAY_PINS[4] = {16, 14, 12, 13}; // DE: R1..R4 Pins / EN: R1..R4 pins
bool state[4] = {false, false, false, false};   // DE: Schattenzustand der Relais / EN: Shadow state of relays

// ---------- Status / Telemetrie ----------
unsigned long lastChange[4] = {0, 0, 0, 0};     // DE: Letzte Änderung (ms) / EN: Last change (ms)

// ---------- Helpers ----------
String formatUptime() {                          // DE: Uptime hh:mm:ss / EN: uptime hh:mm:ss
  unsigned long ms = millis();                   // DE: Laufzeit in ms / EN: runtime in ms
  unsigned long sec = ms / 1000UL;               // DE: Sekunden / EN: seconds
  unsigned int  s = sec % 60UL;                  // DE: Restsekunden / EN: seconds remainder
  unsigned int  m = (sec / 60UL) % 60UL;         // DE: Minuten / EN: minutes
  unsigned int  h = (sec / 3600UL) % 24UL;       // DE: Stunden / EN: hours
  unsigned long d = (sec / 86400UL);             // DE: Tage / EN: days
  char buf[48];                                   // DE: Puffer / EN: buffer
  snprintf(buf, sizeof(buf), "%lud %02u:%02u:%02u", d, h, m, s); // DE: Format / EN: format
  return String(buf);                             // DE: String zurück / EN: return string
}

// ---------- API-Status JSON ----------
String makeStateJson() {                         // DE: Kompaktes Status-JSON / EN: compact status JSON
  String j = "{";                                // DE: Start / EN: begin
  j += "\"uptime\":\"" + formatUptime() + "\","; // DE: Uptime Feld / EN: uptime field
  j += "\"relays\":[";                           // DE: Relais-Array / EN: relays array
  for (int i = 0; i < 4; i++) {                  // DE: Schleife / EN: loop
    j += (state[i] ? "true" : "false");          // DE: Bool als Text / EN: bool text
    if (i < 3) j += ",";                         // DE: Komma / EN: comma
  }
  j += "]}";                                     // DE: Ende / EN: end
  return j;                                      // DE: Rückgabe / EN: return
}

// ---------- JSON & CORS Utilities ----------
void sendJson(int code, const String& body) {                 // DE: JSON senden mit CORS / EN: send JSON with CORS
  server.sendHeader("Access-Control-Allow-Origin", "*");      // DE: CORS / EN: CORS
  server.sendHeader("Cache-Control", "no-store");             // DE: Kein Cache / EN: no cache
  server.send(code, "application/json; charset=utf-8", body); // DE: Antwort / EN: response
}

void sendCorsPreflight() {                       // DE: OPTIONS-Antwort / EN: OPTIONS reply
  server.sendHeader("Access-Control-Allow-Origin", "*");         // DE: CORS / EN: CORS
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS"); // DE/EN: methods
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");     // DE/EN: headers
  server.send(204);                              // DE: No Content / EN: no content
}

bool getArgInt(const String& name, int &out) {   // DE: Int-Query lesen / EN: read int query
  if (!server.hasArg(name)) return false;        // DE: fehlt? / EN: missing?
  out = server.arg(name).toInt();                // DE: konvertieren / EN: convert
  return true;                                   // DE: ok / EN: ok
}

bool getArgBool(const String& name, bool &out) { // DE: Bool-Query robust / EN: robust bool query
  if (!server.hasArg(name)) return false;        // DE: fehlt? / EN: missing?
  String v = server.arg(name); v.toLowerCase();  // DE: normalize / EN: normalize
  if (v == "1" || v == "true" || v == "on")  { out = true;  return true; }  // DE/EN: true
  if (v == "0" || v == "false"|| v == "off") { out = false; return true; }  // DE/EN: false
  return false;                                  // DE: unklar / EN: unclear
}

String makeErrorJson(int code, const String& msg) { // DE: Fehler-JSON / EN: error JSON
  String j = "{";                                   // DE/EN: begin
  j += "\"ok\":false,";                             // DE/EN: flag
  j += "\"code\":" + String(code) + ",";            // DE/EN: code
  j += "\"error\":\"" + msg + "\"";                 // DE/EN: message
  j += "}";                                         // DE/EN: end
  return j;                                         // DE/EN: return
}

String makeAboutJson() {                        // DE: System-/Build-Infos / EN: system/build info
  static String md5 = ESP.getSketchMD5();       // DE: MD5 einmal holen / EN: cache MD5
  String j = "{";                                // DE/EN: begin
  j += "\"name\":\"" + String(FW_NAME) + "\",";  // DE/EN: name
  j += "\"version\":\"" + String(FW_VERSION) + "\","; // DE/EN: version
  j += "\"build\":\"" + String(FW_BUILD) + "\",";     // DE/EN: build
  j += "\"md5\":\"" + md5 + "\",";               // DE/EN: md5
  j += "\"chip_id\":" + String(ESP.getChipId()) + ","; // DE/EN: chip id
  j += "\"flash_size\":" + String(ESP.getFlashChipRealSize()) + ","; // DE/EN: flash
  j += "\"sketch_size\":" + String(ESP.getSketchSize()) + ",";       // DE/EN: sketch size
  j += "\"free_sketch_space\":" + String(ESP.getFreeSketchSpace()) + ","; // DE/EN: free space
  j += "\"uptime\":\"" + formatUptime() + "\","; // DE/EN: uptime
  j += "\"active_low\":"; j += (ACTIVE_LOW ? "true" : "false"); j += ","; // DE/EN: active_low
  j += "\"relay_pins\":[";                      // DE/EN: pins
  for (int i=0;i<4;i++){ j += String(RELAY_PINS[i]); if(i<3) j += ","; } // DE/EN: list
  j += "],\"relays\":[";
  for (int i=0;i<4;i++){ j += (state[i] ? "true" : "false"); if(i<3) j += ","; } // DE/EN: states
  j += "],\"last_change_ms\":[";
  for (int i=0;i<4;i++){ j += String(lastChange[i]); if(i<3) j += ","; }  // DE/EN: timestamps
  j += "]}";                                // DE/EN: end
  return j;                                  // DE/EN: return
}

// ---------- Startseite ----------
String page() {                                // DE: HTML-UI / EN: HTML UI
  String md5 = ESP.getSketchMD5();             // DE: MD5 anzeigen / EN: show MD5
  String s =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>" // DE/EN: head
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>" + String(FW_NAME) + "</title>"
    "<style>"
    "body{font-family:system-ui,Arial;max-width:720px;margin:24px auto;padding:0 12px;text-align:center}"
    "h1{font-size:1.5rem;margin:0 0 .25rem}"
    ".muted{color:#666}"
    ".grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin-top:16px}"
    "a.btn{display:block;padding:16px;border-radius:12px;text-decoration:none;border:1px solid #ccc}"
    ".on{background:#eaffea}.off{background:#ffeeee}"
    ".row{margin-top:18px}"
    ".link{display:inline-block;padding:12px 16px;border-radius:10px;border:1px solid #ccc;margin:4px}"
    "code{font-family:ui-monospace,Consolas,monospace}"
    ".foot{margin-top:18px;color:#666;font-size:.9rem}"
    "</style></head><body>";                 // DE/EN: styles

  s += "<h1>" + String(FW_NAME) + "</h1>";    // DE: Überschrift / EN: header
  s += "<div class='muted'>Firmware: v" + String(FW_VERSION) + " &bull; Build: " + String(FW_BUILD) + "</div>"; // DE/EN: version
  s += "<div class='muted'>MD5: <code>" + md5 + "</code> &bull; Uptime: " + formatUptime() + "</div>"; // DE/EN: meta

  s += "<p class='muted'>Relais schalten (Ein/Aus)</p><div class='grid'>"; // DE/EN: grid
  for (int i = 0; i < 4; i++) {               // DE/EN: loop controls
    bool on = state[i];                       // DE/EN: current state
    s += "<a class='btn ";                    // DE/EN: button start
    s += on ? "on" : "off";                   // DE/EN: color class
    s += "' href='/toggle?ch=" + String(i+1) + "'>"; // DE/EN: link
    s += "Relais " + String(i+1) + ": ";      // DE/EN: label
    s += on ? "AUS / Off" : "EIN / On";       // DE/EN: action
    s += "</a>";                              // DE/EN: end
  }
  s += "</div>";                              // DE/EN: end grid

  s +=
    "<div class='row'>"
    "<a class='link' href='/fw'>🔁 Firmware-Update</a>"
    "<a class='link' href='/about'>ℹ️ System-Info (JSON)</a>"
    "</div>"
    "<div class='foot'>R1=GPIO16, R2=GPIO14, R3=GPIO12, R4=GPIO13"
    "<br>Hinweis: R1 (GPIO16) kann beim Start kurz einschalten.</div>"
    "</body></html>";                         // DE/EN: footer
  return s;                                   // DE/EN: return
}

// ---------- OTA-Seite mit Fortschritt ----------
String fwPage() {                              // DE: OTA-Webseite / EN: OTA web page
  String s =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Firmware Update</title>"
    "<style>"
    "body{font-family:system-ui,Arial;max-width:640px;margin:24px auto;padding:0 12px}"
    "h1{text-align:center;font-size:1.4rem;margin-bottom:.5rem}"
    ".card{border:1px solid #ddd;border-radius:12px;padding:16px}"
    "#barwrap{width:100%;height:16px;background:#eee;border-radius:8px;overflow:hidden}"
    "#bar{height:100%;width:0%}"
    "#status{color:#444;font-family:ui-monospace,Consolas,monospace;white-space:pre-wrap}"
    "button{padding:10px 14px;border-radius:10px;border:1px solid #ccc;background:#fafafa;cursor:pointer}"
    "a{color:#06f;text-decoration:none}"
    "</style></head><body>"
    "<h1>Firmware-Update (OTA)</h1>"
    "<div class='card'>"
    "<input type='file' id='file' accept='.bin'><br><br>"
    "<button id='go'>Upload & Flash</button> <a href='/'>&larr; Zurück</a>"
    "<div style='margin:12px 0'><div id='barwrap'><div id='bar'></div></div></div>"
    "<div id='status'>Bereit.</div>"
    "<div style='margin-top:8px'><small>Hinweis: Feldname <code>update</code> &amp; gleiche Auth wie bei <code>/update</code>.</small></div>"
    "</div>"
    "<script>"
    "let oldMD5=null, oldVer=null, oldBuild=null;"
    "fetch('/about',{cache:'no-store'}).then(r=>r.ok?r.json():null).then(j=>{"
      "if(j){oldMD5=j.md5; oldVer=j.version; oldBuild=j.build;}"
    "}).catch(()=>{});"

    "const go=document.getElementById('go');"
    "const file=document.getElementById('file');"
    "const bar=document.getElementById('bar');"
    "const status=document.getElementById('status');"

    "function setBar(p){bar.style.width=p+'%';bar.style.background='linear-gradient(90deg,#cfe8ff,#b3d4ff)';}"
    "function disableUI(d){go.disabled=d; file.disabled=d;}"

    "function pollBack(timeoutMs){"
      "const started=Date.now();"
      "status.textContent+='\\nNeustart… warte auf Gerät';"
      "(function tick(){"
        "fetch('/about?cb='+Date.now(),{cache:'no-store'}).then(r=>r.ok?r.json():Promise.reject()).then(j=>{"
          "setBar(100);"
          "if(oldMD5 && j.md5 && j.md5!==oldMD5){"
            "status.textContent+='\\nGerät wieder online ✔\\nUpdate verifiziert (MD5 geändert)\\nVersion: v'+j.version+' • Build: '+j.build;"
          "}else{"
            "status.textContent+='\\nGerät wieder online ✔\\nHinweis: MD5 unverändert (ggf. gleiche Version geflasht)\\nVersion: v'+j.version+' • Build: '+j.build;"
          "}"
          "setTimeout(()=>{location.href='/'},8000);"
        "}).catch(()=>{"
          "if(Date.now()-started<timeoutMs){setTimeout(tick,1500);}else{"
            "status.textContent+='\\nZeitüberschreitung – Gerät nicht erreichbar. Bitte Seite neu laden oder Netzwerk prüfen.';"
            "disableUI(false);"
          "}"
        "});"
      "})();"
    "}"

    "go.onclick=function(){"
      "if(!file.files.length){alert('Bitte .bin-Datei wählen');return;}"
      "disableUI(true);"
      "const fd=new FormData();fd.append('update',file.files[0],file.files[0].name);"
      "const xhr=new XMLHttpRequest();xhr.open('POST','/update',true);"
      "xhr.upload.onprogress=function(e){if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);setBar(p);status.textContent='Upload: '+p+'%';}};"
      "xhr.onreadystatechange=function(){if(xhr.readyState===4){if(xhr.status===200){"
      "status.textContent+='\\nUpdate erfolgreich. Neustart wird ausgeführt…';"
      "setBar(100);"
      "setTimeout(()=>pollBack(90000), 1500);"
      "}else if (xhr.status===401){"
      "status.textContent='401 Unauthorized – bitte einmal auf /update einloggen, dann erneut versuchen.';"
      "window.open('/update','_blank');"
      "disableUI(false);"
      "}else{"
      "status.textContent='Fehler: '+xhr.status+' '+xhr.responseText;"
      "disableUI(false);"
      "}}};"

      "status.textContent='Starte Upload…';"
      "xhr.send(fd);"
    "};"
    "</script></body></html>";
  return s;                                      // DE/EN: return
}

// ---------- Relaisfunktionen ----------
void setRelay(uint8_t idx, bool on) {            // DE: Relais setzen / EN: set relay
  if (idx > 3) return;                           // DE/EN: bounds guard
  state[idx] = on;                               // DE/EN: shadow state
  if (on) RELAY_ON(RELAY_PINS[idx]); else RELAY_OFF(RELAY_PINS[idx]); // DE/EN: drive pin
  lastChange[idx] = millis();                    // DE/EN: remember time
}

void toggleRelay(uint8_t idx) { setRelay(idx, !state[idx]); } // DE/EN: toggle

// ---------- Setup ----------
void setup() {                                   // DE: Initialisierung / EN: initialization
  Serial.begin(115200);                          // DE/EN: serial debug
  Serial.setDebugOutput(true);  // DE: Kernel/WiFi-Debug auf die serielle Ausgabe legen
                                // EN: Route kernel/WiFi debug to the serial output
  // DE: Diagnoseausgabe für Flash-/OTA-Layout – hilft, OTA-Probleme schnell zu erkennen.
  // EN: Diagnostic printout for flash/OTA layout – quickly reveals OTA configuration issues.
  Serial.printf(
    "Flash real:%u, ide:%u, sketch:%u, free:%u\n",   // DE: Formatstring: reale Flashgröße, IDE-konfigurierte Größe, Sketch-Größe, freier OTA-Speicher
                                                      // EN: Format string: real flash size, IDE-configured size, sketch size, free OTA space
    ESP.getFlashChipRealSize(),                       // DE: Tatsächliche physische Flashgröße (z. B. 4194304 = 4 MB)
                                                      // EN: Actual physical flash size (e.g., 4194304 = 4 MB)
    ESP.getFlashChipSize(),                           // DE: Von der Toolchain/IDE erwartete Flashgröße – muss 'real' entsprechen
                                                      // EN: Flash size expected by toolchain/IDE – must match 'real'
    ESP.getSketchSize(),                              // DE: Größe des aktuell laufenden Sketches (Bytes)
                                                      // EN: Size of the currently running sketch (bytes)
    ESP.getFreeSketchSpace()                          // DE: Freier Platz für OTA-Sketch (Bytes) – neue .bin muss kleiner sein
                                                      // EN: Free space available for OTA sketch (bytes) – new .bin must be smaller
  );

// DE: Warnung ausgeben, wenn IDE-Flashgröße nicht zur realen Chipgröße passt (häufige OTA-Ursache).
// EN: Warn if IDE flash size does not match the real chip size (common OTA failure cause).
if (ESP.getFlashChipRealSize() != ESP.getFlashChipSize()) {
  Serial.println(
    "WARN: IDE flash size mismatch -> Tools/Flash Size auf 4M stellen!" // DE: Hinweis zur Korrektur in der Arduino-IDE
                                                                         // EN: Hint to fix settings in Arduino IDE (set Flash Size to 4M)
  );
}

  Serial.println();                              // DE/EN: newline
  Serial.println(F("ESP12F_Relay_X4 starting (OTA+Progress+Poll+WiFiManager)…")); // DE/EN: banner

  for (int i=0;i<4;i++){                         // DE: Pins vorbereiten / EN: init pins
    pinMode(RELAY_PINS[i],OUTPUT);               // DE/EN: as output
    setRelay(i,false);                            // DE/EN: default off
  }

  // --- WiFi via WiFiManager ---
  WiFi.mode(WIFI_STA);                           // DE: Station-Modus / EN: station mode
  WiFi.hostname(HOSTNAME);                       // DE: DHCP-Hostname setzen / EN: set DHCP hostname

  WiFiManager wifiManager;                       // DE: Manager-Objekt / EN: manager
  wifiManager.setConfigPortalTimeout(180);       // DE: Portal Timeout 180s / EN: portal timeout 180s
  wifiManager.setBreakAfterConfig(true);         // DE: Nach Konfig. zurückgeben / EN: return after config
  // Optional: Callback bei Portalstart / Optional: portal start callback
  wifiManager.setAPCallback([](WiFiManager* wm){
    Serial.println(F("WiFiManager: Config Portal active AP=ESP12F_Relay_X4")); // DE/EN: log
  });

  bool ok = wifiManager.autoConnect("ESP12F_Relay_X4"); // DE: AP-Name; verbindet oder startet Portal / EN: AP name; connects or opens portal
  if (!ok) {                                       // DE: Verbindung scheiterte / EN: failed
    Serial.println(F("WiFiManager: connection failed, rebooting…")); // DE/EN: log
    delay(500); ESP.restart();                     // DE: Neustart / EN: reboot
  }
  Serial.print(F("WiFi connected, IP: "));         // DE/EN: log
  Serial.println(WiFi.localIP());                  // DE/EN: ip

  // --- mDNS ---
  if (MDNS.begin(HOSTNAME)) {                      // DE: mDNS starten / EN: start mDNS
    Serial.printf("mDNS: http://%s.local\n", HOSTNAME); // DE/EN: info
  }

  // --- OTA Updater ---
  httpUpdater.setup(&server,"/update",update_username,update_password); // DE: /update mit Basic-Auth / EN: /update basic auth
  // DE: OTA-Progress auf der Seriellen (optional).
  // EN: Serial progress for OTA (optional).
  Update.onProgress([](size_t cur, size_t total){
    Serial.printf("OTA: %u / %u bytes\r\n", (unsigned)cur, (unsigned)total);
  });

  // ---------- Web UI ----------
  server.on("/",[](){ server.send(200,"text/html; charset=utf-8",page()); }); // DE/EN: root page

  server.on("/toggle",[](){                        // DE: Toggle per Link / EN: toggle via link
    if(!server.hasArg("ch")){ server.send(400,"text/plain","Missing ch"); return; } // DE/EN: check
    int ch=server.arg("ch").toInt();               // DE/EN: parse
    if(ch<1||ch>4){ server.send(400,"text/plain","ch out of range"); return; } // DE/EN: bounds
    toggleRelay(ch-1);                              // DE/EN: toggle
    server.send(200,"text/html; charset=utf-8",page()); // DE/EN: refresh
  });

  server.on("/on", [](){                           // DE: Einschalten / EN: turn on
    int ch=server.arg("ch").toInt();               // DE/EN: parse
    if(ch>=1&&ch<=4) setRelay(ch-1,true);          // DE/EN: set
    server.send(200,"text/html; charset=utf-8",page()); // DE/EN: refresh
  });

  server.on("/off",[](){                           // DE: Ausschalten / EN: turn off
    int ch=server.arg("ch").toInt();               // DE/EN: parse
    if(ch>=1&&ch<=4) setRelay(ch-1,false);         // DE/EN: set
    server.send(200,"text/html; charset=utf-8",page()); // DE/EN: refresh
  });

  server.on("/about",[](){ server.send(200,"application/json; charset=utf-8",makeAboutJson()); }); // DE/EN: about JSON
  // DE: /fw nur nach Login ausliefern, damit Browser Basic-Auth-Creds cachen.
  // EN: Protect /fw so the browser caches Basic-Auth creds for later XHR to /update.
  server.on("/fw", [](){
    if (!server.authenticate(update_username, update_password)) {
      return server.requestAuthentication(); // DE: Browser-Login-Popup / EN: login prompt
    }
  server.send(200, "text/html; charset=utf-8", fwPage());
  }); 

  // ---------- Lightweight JSON state ----------
  server.on("/state", [](){                       // DE: Schnellstatus / EN: quick status
    server.send(200, "application/json; charset=utf-8", makeStateJson()); // DE/EN: send
  });

  // ---------- API: JSON control & state (GET/POST) ----------
  server.on("/api/get", HTTP_OPTIONS, [](){ sendCorsPreflight(); }); // DE/EN: CORS
  server.on("/api/set", HTTP_OPTIONS, [](){ sendCorsPreflight(); }); // DE/EN: CORS
  server.on("/api/get/", HTTP_OPTIONS, [](){ sendCorsPreflight(); }); // DE/EN: CORS
  server.on("/api/set/", HTTP_OPTIONS, [](){ sendCorsPreflight(); }); // DE/EN: CORS

  server.on("/api/get", HTTP_GET, [](){           // DE: Status lesen / EN: read status
    sendJson(200, makeStateJson());               // DE/EN: send json
  });
  server.on("/api/get/", HTTP_GET, [](){          // DE: trailing slash / EN: alias
    sendJson(200, makeStateJson());               // DE/EN: send json
  });

  auto handleSet = [](){                          // DE: /api/set Handler / EN: /api/set handler
    if (!server.hasArg("ch") || !server.hasArg("on")) {
      sendJson(400, makeErrorJson(400, "params ch and on required")); return; // DE/EN: check
    }
    int ch = server.arg("ch").toInt();            // DE/EN: parse ch
    if (ch < 1 || ch > 4) {
      sendJson(400, makeErrorJson(400, "param ch must be 1..4")); return; // DE/EN: bounds
    }
    String vs = server.arg("on"); vs.toLowerCase(); // DE/EN: parse on
    bool on = (vs == "1" || vs == "true" || vs == "on"); // DE/EN: bool
    setRelay((uint8_t)(ch - 1), on);              // DE/EN: set relay
    sendJson(200, makeStateJson());               // DE/EN: echo state
  };
  server.on("/api/set",  HTTP_GET, handleSet);    // DE/EN: GET variant
  server.on("/api/set/", HTTP_GET, handleSet);    // DE/EN: GET alias

  server.on("/api/set", HTTP_POST, [](){          // DE: POST Body / EN: POST body
    int ch = -1; bool on = false; bool ok = false; // DE/EN: init

    if (server.hasArg("plain")) {                 // DE: JSON-Body vorhanden? / EN: JSON body?
      String body = server.arg("plain");          // DE/EN: get body
      int ich = body.indexOf("\"ch\"");           // DE/EN: find ch
      int ion = body.indexOf("\"on\"");           // DE/EN: find on
      if (ich >= 0 && ion >= 0) {                 // DE/EN: both found?
        int cpos = body.indexOf(':', ich);        // DE/EN: ch colon
        int opos = body.indexOf(':', ion);        // DE/EN: on colon
        if (cpos > 0) ch = body.substring(cpos+1).toInt(); // DE/EN: parse ch
        String vs = body.substring(opos+1); vs.toLowerCase(); // DE/EN: parse on
        on = (vs.indexOf("true")>=0 || vs.indexOf("1")>=0);  // DE/EN: bool
        ok = true;                                  // DE/EN: mark ok
      }
    }
    if (!ok && server.hasArg("ch") && server.hasArg("on")) { // DE: Fallback Form / EN: form fallback
      ch = server.arg("ch").toInt();              // DE/EN: parse
      String vs = server.arg("on"); vs.toLowerCase(); // DE/EN: parse
      on = (vs=="1"||vs=="true"||vs=="on");       // DE/EN: bool
      ok = true;                                  // DE/EN: ok
    }

    if (!ok || ch < 1 || ch > 4) {                // DE: Validierung / EN: validate
      sendJson(400, makeErrorJson(400, "Need JSON {ch:int,on:bool} or form ch,on")); return; // DE/EN: error
    }
    setRelay((uint8_t)(ch-1), on);                // DE/EN: set
    sendJson(200, makeStateJson());               // DE/EN: reply
  });

  server.onNotFound([](){                         // DE: 404-Handler / EN: 404 handler
    server.send(404, "text/plain; charset=utf-8", "Not found: " + server.uri()); // DE/EN: msg
  });

  server.begin();                                  // DE: Server starten / EN: start server
  MDNS.addService("http","tcp",80);                // DE: mDNS HTTP-Service / EN: mDNS HTTP
  Serial.println(F("Ready."));                     // DE/EN: ready
}

// ---------- Loop ----------
void loop() {                                      // DE: Hauptschleife / EN: main loop
  server.handleClient();                           // DE: HTTP bedienen / EN: handle HTTP
  MDNS.update();                                   // DE: mDNS warten / EN: service mDNS
}
