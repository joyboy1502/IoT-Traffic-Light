#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ── WiFi untuk ESP32 ──────────────────────────────────────
const char* WIFI_SSID     = "JARNO MM";
const char* WIFI_PASSWORD = "Milo3838";

// ── HiveMQ Cloud ───────────
const char* MQTT_HOST = "c5b520208d3746bfb7c0837b1908274f.s1.eu.hivemq.cloud";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "hakim";           
const char* MQTT_PASS = "Hakim123";  

// ── OTA WebServer Auth ────────────────────────────────────
const char* OTA_USER = "admin";
const char* OTA_PASS = "admin";
const char* HOSTNAME = "lampujalan";

// ╔══════════════════════════════════════════════════════════╗
// ║                   MQTT TOPICS                           ║
// ╚══════════════════════════════════════════════════════════╝
#define TOPIC_STATUS  "trafficlight/status"   // ESP32 → Dashboard (JSON)
#define TOPIC_SETDUR  "trafficlight/setdur"   // Dashboard → ESP32 (JSON)
#define TOPIC_LOG     "trafficlight/log"      // ESP32 → Dashboard (string)
#define TOPIC_CMD     "trafficlight/cmd"      // Dashboard → ESP32 (string)

// ╔══════════════════════════════════════════════════════════╗
// ║                    PIN MAPPING                          ║
// ╚══════════════════════════════════════════════════════════╝
#define PIN_U_RED 25
#define PIN_U_YEL 26
#define PIN_U_GRN 27
#define PIN_S_RED 14
#define PIN_S_YEL 12
#define PIN_S_GRN 13
#define PIN_T_RED 33
#define PIN_T_YEL 23
#define PIN_T_GRN 22
#define PIN_B_RED 18
#define PIN_B_YEL 19
#define PIN_B_GRN 21

// ╔══════════════════════════════════════════════════════════╗
// ║                  DURASI DEFAULT (ms)                    ║
// ╚══════════════════════════════════════════════════════════╝
volatile unsigned long DUR_RED = 4000;
volatile unsigned long DUR_YEL = 2000;
volatile unsigned long DUR_GRN = 4000;

// ╔══════════════════════════════════════════════════════════╗
// ║                   STATE MACHINE                         ║
// ╚══════════════════════════════════════════════════════════╝
enum Phase {
  PHASE_NS_GREEN,
  PHASE_NS_YELLOW,
  PHASE_EW_GREEN,
  PHASE_EW_YELLOW
};

Phase         currentPhase    = PHASE_NS_GREEN;
unsigned long phaseStart      = 0;
unsigned long lastPublish      = 0;
unsigned long lastReconnect    = 0;

// ╔══════════════════════════════════════════════════════════╗
// ║                      CLIENTS                            ║
// ╚══════════════════════════════════════════════════════════╝
WiFiClientSecure secureClient;
PubSubClient     mqtt(secureClient);
WebServer        server(80);

// ╔══════════════════════════════════════════════════════════╗
// ║               TRAFFIC LIGHT HELPERS                     ║
// ╚══════════════════════════════════════════════════════════╝
struct TrafficPins { uint8_t red, yel, grn; };

TrafficPins UTARA   = {PIN_U_RED, PIN_U_YEL, PIN_U_GRN};
TrafficPins SELATAN = {PIN_S_RED, PIN_S_YEL, PIN_S_GRN};
TrafficPins TIMUR   = {PIN_T_RED, PIN_T_YEL, PIN_T_GRN};
TrafficPins BARAT   = {PIN_B_RED, PIN_B_YEL, PIN_B_GRN};

void setLight(TrafficPins tp, bool r, bool y, bool g) {
  digitalWrite(tp.red, r ? HIGH : LOW);
  digitalWrite(tp.yel, y ? HIGH : LOW);
  digitalWrite(tp.grn, g ? HIGH : LOW);
}

void allRed() {
  setLight(UTARA,   true, false, false);
  setLight(SELATAN, true, false, false);
  setLight(TIMUR,   true, false, false);
  setLight(BARAT,   true, false, false);
}

void applyPhase(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:
      setLight(UTARA,   false, false, true);
      setLight(SELATAN, false, false, true);
      setLight(TIMUR,   true,  false, false);
      setLight(BARAT,   true,  false, false);
      Serial.println("[FASE] U+S=HIJAU | T+B=MERAH");
      break;
    case PHASE_NS_YELLOW:
      setLight(UTARA,   false, true,  false);
      setLight(SELATAN, false, true,  false);
      setLight(TIMUR,   true,  false, false);
      setLight(BARAT,   true,  false, false);
      Serial.println("[FASE] U+S=KUNING | T+B=MERAH");
      break;
    case PHASE_EW_GREEN:
      setLight(UTARA,   true,  false, false);
      setLight(SELATAN, true,  false, false);
      setLight(TIMUR,   false, false, true);
      setLight(BARAT,   false, false, true);
      Serial.println("[FASE] T+B=HIJAU | U+S=MERAH");
      break;
    case PHASE_EW_YELLOW:
      setLight(UTARA,   true,  false, false);
      setLight(SELATAN, true,  false, false);
      setLight(TIMUR,   false, true,  false);
      setLight(BARAT,   false, true,  false);
      Serial.println("[FASE] T+B=KUNING | U+S=MERAH");
      break;
  }
}

unsigned long phaseDuration(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:  return DUR_GRN;
    case PHASE_NS_YELLOW: return DUR_YEL;
    case PHASE_EW_GREEN:  return DUR_GRN;
    case PHASE_EW_YELLOW: return DUR_YEL;
    default:              return DUR_RED;
  }
}

Phase nextPhase(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:  return PHASE_NS_YELLOW;
    case PHASE_NS_YELLOW: return PHASE_EW_GREEN;
    case PHASE_EW_GREEN:  return PHASE_EW_YELLOW;
    case PHASE_EW_YELLOW: return PHASE_NS_GREEN;
    default:              return PHASE_NS_GREEN;
  }
}

// ╔══════════════════════════════════════════════════════════╗
// ║                  MQTT FUNCTIONS                         ║
// ╚══════════════════════════════════════════════════════════╝

// Publish status ke broker
void publishStatus() {
  if (!mqtt.connected()) return;

  StaticJsonDocument<200> doc;
  doc["phase"]   = (int)currentPhase;
  doc["elapsed"] = millis() - phaseStart;
  doc["red"]     = DUR_RED;
  doc["yel"]     = DUR_YEL;
  doc["grn"]     = DUR_GRN;
  doc["ip"]      = WiFi.localIP().toString();
  doc["rssi"]    = WiFi.RSSI();

  char buf[200];
  serializeJson(doc, buf);
  mqtt.publish(TOPIC_STATUS, buf, true); // retained=true
}

// Publish log message
void mqttLog(const char* msg, bool isError = false) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<128> doc;
  doc["msg"]   = msg;
  doc["type"]  = isError ? "err" : "info";
  doc["ts"]    = millis();
  char buf[128];
  serializeJson(doc, buf);
  mqtt.publish(TOPIC_LOG, buf);
  Serial.printf("[LOG] %s\n", msg);
}

// Callback: terima pesan dari dashboard
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[MQTT IN] %s : %s\n", topic, msg.c_str());

  // ── Topic: set durasi ──────────────────────────────────
  if (String(topic) == TOPIC_SETDUR) {
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) {
      mqttLog("JSON error pada setdur", true);
      return;
    }

    bool changed = false;
    if (doc.containsKey("red")) {
      unsigned long v = constrain((unsigned long)doc["red"], 1000UL, 60000UL);
      if (v != DUR_RED) { DUR_RED = v; changed = true; }
    }
    if (doc.containsKey("yel")) {
      unsigned long v = constrain((unsigned long)doc["yel"], 1000UL, 60000UL);
      if (v != DUR_YEL) { DUR_YEL = v; changed = true; }
    }
    if (doc.containsKey("grn")) {
      unsigned long v = constrain((unsigned long)doc["grn"], 1000UL, 60000UL);
      if (v != DUR_GRN) { DUR_GRN = v; changed = true; }
    }

    if (changed) {
      Serial.printf("[DUR] Merah=%lums Kuning=%lums Hijau=%lums\n",
                    DUR_RED, DUR_YEL, DUR_GRN);
      char logMsg[80];
      snprintf(logMsg, sizeof(logMsg),
               "Durasi diperbarui: M=%lus K=%lus H=%lus",
               DUR_RED/1000, DUR_YEL/1000, DUR_GRN/1000);
      mqttLog(logMsg);
      publishStatus(); // langsung publish status terbaru
    }
  }

  // ── Topic: command ─────────────────────────────────────
  if (String(topic) == TOPIC_CMD) {
    if (msg == "status") {
      publishStatus();
    } else if (msg == "allred") {
      allRed();
      mqttLog("All RED command received");
    } else if (msg == "reset") {
      mqttLog("Resetting...");
      delay(500);
      ESP.restart();
    }
  }
}

// Reconnect ke MQTT broker (non-blocking)
bool mqttReconnect() {
  if (mqtt.connected()) return true;

  // Throttle: coba reconnect tiap 5 detik
  if (millis() - lastReconnect < 5000) return false;
  lastReconnect = millis();

  Serial.print("[MQTT] Connecting to broker...");
  String clientId = "ESP32TL-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.println(" OK!");
    mqtt.subscribe(TOPIC_SETDUR);
    mqtt.subscribe(TOPIC_CMD);
    mqttLog("ESP32 Traffic Light online");
    publishStatus();
    return true;
  } else {
    Serial.printf(" GAGAL (rc=%d)\n", mqtt.state());
    return false;
  }
}

// ╔══════════════════════════════════════════════════════════╗
// ║              OTA WEBSERVER HANDLERS                     ║
// ╚══════════════════════════════════════════════════════════╝

// Halaman OTA sederhana (hanya untuk upload .bin)
const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<title>Traffic Light — OTA</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#050810;color:#c8deff;font-family:'Courier New',monospace;
  display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px}
.wrap{width:100%;max-width:480px}
.title{font-size:14px;font-weight:700;color:#00ffe0;letter-spacing:.2em;margin-bottom:4px}
.sub{font-size:11px;color:#3a5080;margin-bottom:24px}
.card{background:#090d1a;border:1px solid #162040;border-radius:6px;padding:20px;margin-bottom:12px;position:relative}
.card::before{content:'';position:absolute;top:0;left:0;right:0;height:1px;
  background:linear-gradient(90deg,transparent,#00ffe0,transparent);opacity:.3}
.label{font-size:10px;color:#3a5080;letter-spacing:.15em;margin-bottom:10px}
.status-dot{width:8px;height:8px;border-radius:50%;background:#00ff88;
  box-shadow:0 0 8px #00ff88;display:inline-block;margin-right:6px;
  animation:pulse 1.8s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.info{font-size:12px;color:#c8deff;margin-bottom:6px}
.info span{color:#00ffe0}
.dz{border:1px dashed #162040;border-radius:4px;padding:30px;text-align:center;
  cursor:pointer;transition:border-color .2s,background .2s;margin-bottom:12px}
.dz.drag{border-color:#00ffe0;background:rgba(0,255,224,.04)}
.dz.has-file{border-color:rgba(0,255,136,.3);background:rgba(0,255,136,.03)}
.dz-t{font-size:12px;color:#3a5080;margin-bottom:4px}
.dz-s{font-size:10px;color:#00ffe0;letter-spacing:.1em}
.file-info{display:none;font-size:11px;color:#00ff88;margin-bottom:10px;word-break:break-all}
.prog{display:none;margin-bottom:10px}
.prog-track{height:3px;background:#162040;border-radius:2px;overflow:hidden}
.prog-bar{height:100%;width:0;background:linear-gradient(90deg,#0088ff,#00ffe0);transition:width .2s}
.prog-txt{font-size:10px;color:#3a5080;margin-top:4px;display:flex;justify-content:space-between}
.result{display:none;padding:10px;border-radius:4px;font-size:11px;text-align:center;margin-bottom:10px}
.result.ok{background:rgba(0,255,136,.07);border:1px solid rgba(0,255,136,.2);color:#00ff88}
.result.fail{background:rgba(255,45,85,.07);border:1px solid rgba(255,45,85,.2);color:#ff2d55}
.btn{width:100%;padding:12px;background:rgba(0,255,224,.07);border:1px solid #00ffe0;
  color:#00ffe0;border-radius:4px;cursor:pointer;font-family:monospace;font-size:11px;
  letter-spacing:.2em;transition:background .15s}
.btn:hover:not(:disabled){background:rgba(0,255,224,.15)}
.btn:disabled{border-color:#162040;color:#3a5080;cursor:not-allowed;background:transparent}
.note{font-size:10px;color:#3a5080;margin-top:8px;line-height:1.6}
input[type=file]{position:absolute;width:1px;height:1px;opacity:0}
</style>
</head>
<body>
<div class="wrap">
  <div class="title">TRAFFIC LIGHT // OTA</div>
  <div class="sub">Firmware upload endpoint</div>

  <div class="card">
    <div class="label">DEVICE STATUS</div>
    <div class="info"><span class="status-dot"></span>Online</div>
    <div class="info">IP: <span id="ipAddr">–</span></div>
    <div class="info">Hostname: <span>trafficlight.local</span></div>
    <div class="note">Untuk monitoring &amp; kontrol durasi: gunakan dashboard MQTT<br>
    (dashboard.html — buka dari jaringan mana saja)</div>
  </div>

  <div class="card">
    <div class="label">UPLOAD FIRMWARE (.BIN)</div>
    <input type="file" id="fi" accept=".bin">
    <label class="dz" id="dz" for="fi">
      <div class="dz-t">DROP FILE DI SINI</div>
      <div class="dz-s">[ KLIK UNTUK BROWSE ]</div>
    </label>
    <div class="file-info" id="fInfo"></div>
    <div class="prog" id="prog">
      <div class="prog-track"><div class="prog-bar" id="pb"></div></div>
      <div class="prog-txt"><span id="pct">0%</span><span id="spd">–</span></div>
    </div>
    <div class="result" id="res"></div>
    <button class="btn" id="flashBtn" disabled onclick="doFlash()">▶ FLASH FIRMWARE</button>
    <div class="note">Catatan: untuk OTA, hubungkan HP ke WiFi yang sama dengan ESP32<br>
    atau gunakan ngrok: ngrok http 80</div>
  </div>
</div>
<script>
document.getElementById('ipAddr').textContent = location.hostname;
var fi=document.getElementById('fi'),dz=document.getElementById('dz');
var fb=document.getElementById('flashBtn'),res=document.getElementById('res');
var prog=document.getElementById('prog'),pb=document.getElementById('pb');
var fInfo=document.getElementById('fInfo');
var selectedFile=null;

function fmt(b){if(b<1024)return b+'B';if(b<1048576)return(b/1024).toFixed(1)+'KB';return(b/1048576).toFixed(2)+'MB';}

function onFile(f){
  if(!f||!f.name.match(/\.bin$/i)){alert('Pilih file .bin!');return;}
  selectedFile=f;
  dz.classList.add('has-file');
  fInfo.style.display='block';
  fInfo.textContent='📦 '+f.name+' ('+fmt(f.size)+')';
  fb.disabled=false;res.className='result';
}

fi.onchange=function(){if(fi.files[0])onFile(fi.files[0]);};
['dragenter','dragover'].forEach(e=>dz.addEventListener(e,function(ev){ev.preventDefault();dz.classList.add('drag');}));
dz.addEventListener('dragleave',function(){dz.classList.remove('drag');});
dz.addEventListener('drop',function(ev){ev.preventDefault();dz.classList.remove('drag');var f=ev.dataTransfer.files[0];if(f){try{var dt=new DataTransfer();dt.items.add(f);fi.files=dt.files;}catch(e){}onFile(f);}});

function doFlash(){
  if(!selectedFile)return;
  fb.disabled=true;prog.style.display='block';res.className='result';
  var fd=new FormData();fd.append('update',selectedFile);
  var xhr=new XMLHttpRequest(),t0=Date.now();
  xhr.upload.onprogress=function(e){
    if(!e.lengthComputable)return;
    var p=Math.round(e.loaded/e.total*100);
    var rate=e.loaded/((Date.now()-t0)/1000||.001);
    pb.style.width=p+'%';
    document.getElementById('pct').textContent=p+'%';
    document.getElementById('spd').textContent=fmt(Math.round(rate))+'/s';
  };
  xhr.onload=function(){
    if(xhr.status===200){res.className='result ok';res.style.display='block';res.textContent='✓ UPDATE SUKSES — Device sedang reboot';}
    else{res.className='result fail';res.style.display='block';res.textContent='✗ GAGAL HTTP '+xhr.status;fb.disabled=false;}
  };
  xhr.onerror=function(){res.className='result fail';res.style.display='block';res.textContent='✗ Connection error';fb.disabled=false;};
  xhr.open('POST','/update',true);
  xhr.setRequestHeader('Authorization','Basic '+btoa('admin:admin'));
  xhr.send(fd);
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  if (!server.authenticate(OTA_USER, OTA_PASS))
    return server.requestAuthentication();
  server.send(200, "text/html", OTA_PAGE);
}

void handleOTAUpload() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
      allRed(); // semua merah saat OTA
      mqttLog("OTA Update dimulai...");
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        mqttLog("OTA begin gagal!", true);
      }
      break;

    case UPLOAD_FILE_WRITE:
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
      break;

    case UPLOAD_FILE_END:
      if (Update.end(true)) {
        Serial.printf("[OTA] Sukses: %u bytes\n", upload.totalSize);
        mqttLog("OTA selesai, rebooting...");
      } else {
        Update.printError(Serial);
        mqttLog("OTA gagal!", true);
      }
      break;

    default:
      break;
  }
}

void handleOTAResult() {
  if (!server.authenticate(OTA_USER, OTA_PASS))
    return server.requestAuthentication();
  bool ok = !Update.hasError();
  server.send(200, "text/plain", ok ? "OK" : "FAIL");
  delay(1000);
  if (ok) ESP.restart();
}

// ╔══════════════════════════════════════════════════════════╗
// ║                       SETUP                             ║
// ╚══════════════════════════════════════════════════════════╝
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== Traffic Light MQTT Boot ===");

  // Init semua pin output
  uint8_t pins[] = {
    PIN_U_RED, PIN_U_YEL, PIN_U_GRN,
    PIN_S_RED, PIN_S_YEL, PIN_S_GRN,
    PIN_T_RED, PIN_T_YEL, PIN_T_GRN,
    PIN_B_RED, PIN_B_YEL, PIN_B_GRN
  };
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  allRed();
  delay(500);

  // Koneksi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n[WiFi] GAGAL! Cek SSID/password.");
    // Lanjut saja, traffic light tetap jalan
  }

  // Setup MQTT (TLS tanpa verifikasi sertifikat — cukup untuk tugas)
  secureClient.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqttReconnect();

  // mDNS
  if (MDNS.begin(HOSTNAME))
    Serial.printf("[mDNS] http://%s.local\n", HOSTNAME);

  // WebServer untuk OTA
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/update", HTTP_POST, handleOTAResult, handleOTAUpload);
  server.begin();
  Serial.println("[HTTP] WebServer OK — port 80");

  // Mulai traffic light
  currentPhase = PHASE_NS_GREEN;
  phaseStart   = millis();
  applyPhase(currentPhase);

  Serial.println("[READY] Traffic Light running!");
  Serial.printf("[READY] Durasi: Merah=%lus Kuning=%lus Hijau=%lus\n",
                DUR_RED/1000, DUR_YEL/1000, DUR_GRN/1000);
}

// ╔══════════════════════════════════════════════════════════╗
// ║                        LOOP                             ║
// ╚══════════════════════════════════════════════════════════╝
void loop() {
  // ── MQTT ──────────────────────────────────────────────
  if (!mqtt.connected()) {
    mqttReconnect();
  }
  mqtt.loop();

  // ── Publish status setiap 2 detik ─────────────────────
  unsigned long now = millis();
  if (now - lastPublish >= 2000) {
    publishStatus();
    lastPublish = now;
  }

  // ── WebServer (OTA) ────────────────────────────────────
  server.handleClient();

  // ── State machine traffic light ────────────────────────
  now = millis();
  if (now - phaseStart >= phaseDuration(currentPhase)) {
    currentPhase = nextPhase(currentPhase);
    phaseStart   = now;
    applyPhase(currentPhase);
  }
}
