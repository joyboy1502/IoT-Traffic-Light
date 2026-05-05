#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// ── WiFi ──────────────────────────────────────────────────────
const char* WIFI_SSID     = "JARNO MM";
const char* WIFI_PASSWORD = "Milo3838";

// ── OTA Auth ──────────────────────────────────────────────────
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWORD = "admin";
const char* HOSTNAME     = "lampujalan";

// ── Pin Mapping ───────────────────────────────────────────────
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

// ── Durasi (ms) ───────────────────────────────────────────────
volatile unsigned long DUR_RED = 2000;
volatile unsigned long DUR_YEL = 1000;
volatile unsigned long DUR_GRN = 2000;

// ── State Machine ─────────────────────────────────────────────
enum Phase { PHASE_NS_GREEN, PHASE_NS_YELLOW, PHASE_EW_GREEN, PHASE_EW_YELLOW };
Phase currentPhase = PHASE_NS_GREEN;
unsigned long phaseStart = 0;

WebServer server(80);

// ─────────────────────────────────────────────────────────────
//  Dashboard HTML
// ─────────────────────────────────────────────────────────────
const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Traffic Light Control</title>
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;500;600&family=DM+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{
  font-family:'DM Sans',sans-serif;background:#f5f5f0;
  color:#1a1a1a;min-height:100vh;padding:24px 16px;
}
.wrap{max-width:480px;margin:0 auto;}

/* Header */
h1{font-size:18px;font-weight:600;letter-spacing:-.3px;margin-bottom:4px;}
.sub{font-size:13px;color:#888;margin-bottom:24px;font-family:'DM Mono',monospace;}

/* Card */
.card{background:#fff;border-radius:12px;border:1px solid #e8e8e4;padding:20px;margin-bottom:12px;}
.card-title{font-size:11px;font-weight:600;letter-spacing:.08em;color:#aaa;text-transform:uppercase;margin-bottom:16px;}

/* Traffic lights */
.tl-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px;}
.tl-box{background:#fafaf8;border:1px solid #e8e8e4;border-radius:8px;padding:12px;text-align:center;}
.tl-name{font-size:10px;font-weight:600;letter-spacing:.1em;color:#bbb;text-transform:uppercase;margin-bottom:10px;}
.lights{display:flex;flex-direction:column;gap:5px;align-items:center;}
.bulb{width:20px;height:20px;border-radius:50%;transition:background .25s,box-shadow .25s;}
.r-off{background:#f0e8e8;}.r-on{background:#f03030;box-shadow:0 0 12px rgba(240,48,48,.4);}
.y-off{background:#f0ede0;}.y-on{background:#f0b800;box-shadow:0 0 12px rgba(240,184,0,.4);}
.g-off{background:#e0f0e8;}.g-on{background:#18c464;box-shadow:0 0 12px rgba(24,196,100,.4);}

/* Phase bar */
.phase-row{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:8px;}
.phase-label{font-size:13px;font-weight:500;}
.phase-timer{font-family:'DM Mono',monospace;font-size:22px;font-weight:500;letter-spacing:-.5px;}
.prog-track{height:3px;background:#eee;border-radius:2px;overflow:hidden;}
.prog-fill{height:100%;background:#1a1a1a;border-radius:2px;transition:width .5s linear;}

/* Duration control */
.dur-row{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;}
.dur-item{text-align:center;}
.dur-label{font-size:10px;font-weight:600;letter-spacing:.08em;color:#aaa;text-transform:uppercase;margin-bottom:8px;}
.dur-val{font-family:'DM Mono',monospace;font-size:28px;font-weight:500;margin-bottom:8px;line-height:1;}
.dur-val.red{color:#e03030;}.dur-val.yel{color:#d4a000;}.dur-val.grn{color:#16b05a;}
.dur-btns{display:flex;gap:6px;justify-content:center;}
.dur-btn{
  width:30px;height:30px;background:#fafaf8;border:1px solid #e8e8e4;
  border-radius:6px;cursor:pointer;font-size:18px;color:#555;
  display:flex;align-items:center;justify-content:center;
  transition:background .15s,border-color .15s;
}
.dur-btn:hover{background:#f0f0eb;border-color:#ccc;}
.edit-notice{
  display:none;font-size:11px;color:#d4a000;background:#fffbeb;
  border:1px solid #f0e0a0;border-radius:6px;padding:7px 12px;
  text-align:center;margin-bottom:12px;
}
.edit-notice.show{display:block;}
.btn-apply{
  width:100%;padding:11px;background:#1a1a1a;color:#fff;border:none;
  border-radius:8px;font-family:'DM Sans',sans-serif;font-size:13px;font-weight:600;
  cursor:pointer;letter-spacing:.02em;transition:background .15s,opacity .15s;margin-top:14px;
}
.btn-apply:hover{background:#333;}
.btn-apply.changed{background:#d4a000;color:#fff;}
.apply-msg{font-size:12px;text-align:center;margin-top:8px;min-height:16px;color:#888;}
.apply-msg.ok{color:#16b05a;}.apply-msg.err{color:#e03030;}

/* OTA Upload */
#fileInput{display:none;}
.dropzone{
  border:1.5px dashed #ddd;border-radius:8px;padding:24px;
  text-align:center;cursor:pointer;transition:border-color .2s,background .2s;
  margin-bottom:10px;
}
.dropzone:hover,.dropzone.drag{border-color:#aaa;background:#fafaf8;}
.dropzone.ready{border-color:#18c464;background:#f0fdf4;}
.dz-text{font-size:13px;color:#999;margin-bottom:3px;}
.dz-file{font-size:12px;font-weight:500;color:#16b05a;display:none;}
.btn-flash{
  width:100%;padding:11px;background:#fff;color:#1a1a1a;
  border:1.5px solid #1a1a1a;border-radius:8px;
  font-family:'DM Sans',sans-serif;font-size:13px;font-weight:600;
  cursor:pointer;transition:background .15s;
}
.btn-flash:hover:not([disabled]){background:#f5f5f0;}
.btn-flash[disabled]{opacity:.35;cursor:not-allowed;}
.prog-wrap{display:none;margin-top:10px;}
.prog-wrap.show{display:block;}
.prog-label{display:flex;justify-content:space-between;font-size:11px;color:#aaa;margin-bottom:4px;}
.result{display:none;text-align:center;font-size:13px;font-weight:500;padding:9px;border-radius:8px;margin-top:10px;}
.result.ok{display:block;background:#f0fdf4;color:#16b05a;}
.result.fail{display:block;background:#fff0f0;color:#e03030;}

/* Log */
.log-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;}
.log-body{font-family:'DM Mono',monospace;font-size:11px;line-height:2;max-height:120px;overflow-y:auto;}
.log-body::-webkit-scrollbar{width:2px;}
.log-body::-webkit-scrollbar-thumb{background:#ddd;}
.log-clear{font-size:11px;color:#aaa;background:none;border:1px solid #e8e8e4;border-radius:4px;padding:2px 8px;cursor:pointer;}
.le{display:flex;gap:8px;}
.le-ts{color:#ccc;}.le-ok{color:#18c464;}.le-err{color:#e03030;}.le-warn{color:#d4a000;}.le-info{color:#888;}
</style>
</head>
<body>
<div class="wrap">
  <h1>Traffic Light Control</h1>
  <div class="sub" id="hostLabel">–</div>

  <!-- Status -->
  <div class="card">
    <div class="card-title">Status Lampu</div>
    <div class="tl-grid">
      <div class="tl-box">
        <div class="tl-name">Utara</div>
        <div class="lights">
          <div class="bulb r-off" id="u-r"></div>
          <div class="bulb y-off" id="u-y"></div>
          <div class="bulb g-off" id="u-g"></div>
        </div>
      </div>
      <div class="tl-box">
        <div class="tl-name">Selatan</div>
        <div class="lights">
          <div class="bulb r-off" id="s-r"></div>
          <div class="bulb y-off" id="s-y"></div>
          <div class="bulb g-off" id="s-g"></div>
        </div>
      </div>
      <div class="tl-box">
        <div class="tl-name">Timur</div>
        <div class="lights">
          <div class="bulb r-off" id="t-r"></div>
          <div class="bulb y-off" id="t-y"></div>
          <div class="bulb g-off" id="t-g"></div>
        </div>
      </div>
      <div class="tl-box">
        <div class="tl-name">Barat</div>
        <div class="lights">
          <div class="bulb r-off" id="b-r"></div>
          <div class="bulb y-off" id="b-y"></div>
          <div class="bulb g-off" id="b-g"></div>
        </div>
      </div>
    </div>
    <div class="phase-row">
      <div class="phase-label" id="phaseText">NS Hijau</div>
      <div class="phase-timer" id="phaseTimer">–</div>
    </div>
    <div class="prog-track"><div class="prog-fill" id="phaseProg" style="width:0%"></div></div>
  </div>

  <!-- Durasi -->
  <div class="card">
    <div class="card-title">Durasi</div>
    <div class="edit-notice" id="editNotice">Mode edit aktif — tekan Terapkan untuk simpan</div>
    <div class="dur-row">
      <div class="dur-item">
        <div class="dur-label">Merah (s)</div>
        <div class="dur-val red" id="redVal">2</div>
        <div class="dur-btns">
          <button class="dur-btn" onclick="adj('red',-1)">−</button>
          <button class="dur-btn" onclick="adj('red',+1)">+</button>
        </div>
      </div>
      <div class="dur-item">
        <div class="dur-label">Kuning (s)</div>
        <div class="dur-val yel" id="yelVal">1</div>
        <div class="dur-btns">
          <button class="dur-btn" onclick="adj('yel',-1)">−</button>
          <button class="dur-btn" onclick="adj('yel',+1)">+</button>
        </div>
      </div>
      <div class="dur-item">
        <div class="dur-label">Hijau (s)</div>
        <div class="dur-val grn" id="grnVal">2</div>
        <div class="dur-btns">
          <button class="dur-btn" onclick="adj('grn',-1)">−</button>
          <button class="dur-btn" onclick="adj('grn',+1)">+</button>
        </div>
      </div>
    </div>
    <button class="btn-apply" id="btnApply" onclick="applyDur()">Terapkan</button>
    <div class="apply-msg" id="applyMsg"></div>
  </div>

  <!-- OTA -->
  <div class="card">
    <div class="card-title">Update Firmware (OTA)</div>
    <input type="file" id="fileInput" accept=".bin">
    <label class="dropzone" id="dropzone" for="fileInput">
      <div class="dz-text">Klik atau seret file .bin ke sini</div>
      <div class="dz-file" id="dzFile"></div>
    </label>
    <div class="prog-wrap" id="progWrap">
      <div class="prog-label"><span id="progPct">0%</span><span id="progSpd"></span></div>
      <div class="prog-track"><div class="prog-fill" id="progBar" style="width:0%"></div></div>
    </div>
    <div class="result" id="otaResult"></div>
    <button class="btn-flash" id="flashBtn" disabled onclick="startUpload()">Flash Firmware</button>
  </div>

  <!-- Log -->
  <div class="card">
    <div class="log-head">
      <div class="card-title" style="margin:0">Log</div>
      <button class="log-clear" onclick="clearLog()">Bersihkan</button>
    </div>
    <div class="log-body" id="logBody"></div>
  </div>
</div>

<script>
(function(){
  var inp = document.getElementById('fileInput');
  var dz  = document.getElementById('dropzone');
  var dzF = document.getElementById('dzFile');

  document.getElementById('hostLabel').textContent = location.hostname || 'esp32.local';

  var dur = {red:2, yel:1, grn:2};
  var isEditing = false, editTimer = null;

  function enterEdit(){
    isEditing = true;
    document.getElementById('editNotice').classList.add('show');
    document.getElementById('btnApply').classList.add('changed');
    if(editTimer) clearTimeout(editTimer);
    editTimer = setTimeout(function(){
      exitEdit(false);
      log('Edit timeout — nilai dikembalikan','warn');
    }, 15000);
  }

  function exitEdit(applied){
    isEditing = false;
    if(editTimer){ clearTimeout(editTimer); editTimer=null; }
    document.getElementById('editNotice').classList.remove('show');
    document.getElementById('btnApply').classList.remove('changed');
    if(!applied) pollStatus();
  }

  window.adj = function(c, d){
    enterEdit();
    dur[c] = Math.max(1, Math.min(60, dur[c]+d));
    document.getElementById(c+'Val').textContent = dur[c];
  };

  window.applyDur = function(){
    var msg = document.getElementById('applyMsg');
    msg.className='apply-msg'; msg.textContent='';
    fetch('/setdur',{
      method:'POST',
      headers:{'Content-Type':'application/json','Authorization':'Basic '+btoa('admin:admin')},
      body:JSON.stringify({red:dur.red*1000,yel:dur.yel*1000,grn:dur.grn*1000})
    }).then(function(r){return r.text();}).then(function(t){
      if(t==='OK'){
        msg.className='apply-msg ok'; msg.textContent='Durasi berhasil diperbarui';
        log('Durasi: M='+dur.red+'s K='+dur.yel+'s H='+dur.grn+'s','ok');
        exitEdit(true);
      } else {
        msg.className='apply-msg err'; msg.textContent='Gagal menerapkan durasi';
        log('Gagal apply','err');
      }
    }).catch(function(){
      msg.className='apply-msg err'; msg.textContent='Connection error';
      log('Connection error','err');
    });
  };

  // Traffic light state
  var LIGHTS = {
    0:[0,0,1,0,0,1,1,0,0,1,0,0],
    1:[0,1,0,0,1,0,1,0,0,1,0,0],
    2:[1,0,0,1,0,0,0,0,1,0,0,1],
    3:[1,0,0,1,0,0,0,1,0,0,1,0]
  };
  var IDS = ['u-r','u-y','u-g','s-r','s-y','s-g','t-r','t-y','t-g','b-r','b-y','b-g'];
  var CLS = ['r','y','g','r','y','g','r','y','g','r','y','g'];
  var PHASE_LABELS = ['NS Hijau','NS Kuning','EW Hijau','EW Kuning'];
  var curPhase=0, phaseMs=0, phaseDur=2000;

  function updateLights(p){
    var L=LIGHTS[p];
    for(var i=0;i<12;i++){
      document.getElementById(IDS[i]).className='bulb '+(L[i]?CLS[i]+'-on':CLS[i]+'-off');
    }
    document.getElementById('phaseText').textContent=PHASE_LABELS[p];
  }

  function getDur(p){
    return (p===0||p===2) ? dur.grn*1000 : dur.yel*1000;
  }

  function pollStatus(){
    fetch('/status',{headers:{'Authorization':'Basic '+btoa('admin:admin')}})
    .then(function(r){return r.json();})
    .then(function(d){
      curPhase=d.phase; phaseMs=d.elapsed;
      if(!isEditing){
        dur.red=Math.round(d.red/1000);
        dur.yel=Math.round(d.yel/1000);
        dur.grn=Math.round(d.grn/1000);
        document.getElementById('redVal').textContent=dur.red;
        document.getElementById('yelVal').textContent=dur.yel;
        document.getElementById('grnVal').textContent=dur.grn;
      }
      phaseDur=getDur(curPhase);
      updateLights(curPhase);
    }).catch(function(){});
  }

  setInterval(function(){
    phaseMs+=500;
    var rem=Math.max(0,phaseDur-phaseMs);
    document.getElementById('phaseTimer').textContent=(rem/1000).toFixed(1)+'s';
    document.getElementById('phaseProg').style.width=Math.min(100,(phaseMs/phaseDur)*100)+'%';
  },500);

  pollStatus();
  setInterval(pollStatus,2000);

  // Log
  function ts(){
    return [new Date().getHours(),new Date().getMinutes(),new Date().getSeconds()]
      .map(function(n){return ('0'+n).slice(-2);}).join(':');
  }
  function log(msg,type){
    var cls={ok:'le-ok',err:'le-err',warn:'le-warn',info:'le-info'}[type||'info'];
    var el=document.createElement('div'); el.className='le';
    el.innerHTML='<span class="le-ts">'+ts()+'</span><span class="'+cls+'">'+(type||'info')+'</span><span>'+msg+'</span>';
    var lb=document.getElementById('logBody');
    lb.appendChild(el); lb.scrollTop=lb.scrollHeight;
  }
  window.clearLog=function(){document.getElementById('logBody').innerHTML='';};

  function fmt(b){
    return b<1024?b+' B':b<1048576?(b/1024).toFixed(1)+' KB':(b/1048576).toFixed(2)+' MB';
  }

  // File input
  function onFile(f){
    if(!f||!f.name.match(/\.bin$/i)){log('File harus .bin','err');return;}
    dz.classList.add('ready');
    dzF.style.display='block'; dzF.textContent=f.name+' ('+fmt(f.size)+')';
    document.getElementById('flashBtn').disabled=false;
    document.getElementById('otaResult').className='result';
    log('File: '+f.name,'ok');
  }
  inp.addEventListener('change',function(){if(inp.files[0])onFile(inp.files[0]);});
  ['dragenter','dragover'].forEach(function(e){dz.addEventListener(e,function(ev){ev.preventDefault();dz.classList.add('drag');});});
  dz.addEventListener('dragleave',function(){dz.classList.remove('drag');});
  dz.addEventListener('drop',function(ev){
    ev.preventDefault(); dz.classList.remove('drag');
    var f=ev.dataTransfer&&ev.dataTransfer.files[0];
    if(f){try{var dt=new DataTransfer();dt.items.add(f);inp.files=dt.files;}catch(e){}onFile(f);}
  });

  window.startUpload=function(){
    var f=inp.files&&inp.files[0];
    if(!f)return;
    document.getElementById('flashBtn').disabled=true;
    document.getElementById('progWrap').classList.add('show');
    document.getElementById('otaResult').className='result';
    log('Memulai OTA...','info');
    var fd=new FormData(); fd.append('update',f);
    var xhr=new XMLHttpRequest(); var t0=Date.now();
    xhr.upload.onprogress=function(e){
      if(!e.lengthComputable)return;
      var p=Math.round(e.loaded/e.total*100);
      var rate=e.loaded/((Date.now()-t0)/1000||.001);
      document.getElementById('progBar').style.width=p+'%';
      document.getElementById('progPct').textContent=p+'%';
      document.getElementById('progSpd').textContent=fmt(Math.round(rate))+'/s';
    };
    xhr.onload=function(){
      var r=document.getElementById('otaResult');
      if(xhr.status===200){
        r.className='result ok'; r.textContent='Update berhasil — perangkat reboot';
        log('Flash OK, rebooting...','ok');
      } else {
        r.className='result fail'; r.textContent='Gagal — HTTP '+xhr.status;
        log('Flash gagal: '+xhr.status,'err');
        document.getElementById('flashBtn').disabled=false;
      }
    };
    xhr.onerror=function(){
      document.getElementById('otaResult').className='result fail';
      document.getElementById('otaResult').textContent='Connection error';
      log('Connection error','err');
      document.getElementById('flashBtn').disabled=false;
    };
    xhr.open('POST','/update',true);
    xhr.setRequestHeader('Authorization','Basic '+btoa('admin:admin'));
    xhr.send(fd);
  };

  log('Dashboard siap','info');
})();
</script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────────────────────
//  Traffic Light Helpers
// ─────────────────────────────────────────────────────────────
struct TrafficPins { uint8_t red, yel, grn; };

TrafficPins UTARA   = {PIN_U_RED, PIN_U_YEL, PIN_U_GRN};
TrafficPins SELATAN = {PIN_S_RED, PIN_S_YEL, PIN_S_GRN};
TrafficPins TIMUR   = {PIN_T_RED, PIN_T_YEL, PIN_T_GRN};
TrafficPins BARAT   = {PIN_B_RED, PIN_B_YEL, PIN_B_GRN};

void setLight(TrafficPins tp, bool r, bool y, bool g) {
  digitalWrite(tp.red, r); digitalWrite(tp.yel, y); digitalWrite(tp.grn, g);
}

void allRed() {
  setLight(UTARA, 1,0,0); setLight(SELATAN, 1,0,0);
  setLight(TIMUR, 1,0,0); setLight(BARAT,   1,0,0);
}

void applyPhase(Phase p) {
  switch(p) {
    case PHASE_NS_GREEN:
      setLight(UTARA,0,0,1); setLight(SELATAN,0,0,1);
      setLight(TIMUR,1,0,0); setLight(BARAT,1,0,0);
      Serial.println("[FASE] U+S=HIJAU | T+B=MERAH"); break;
    case PHASE_NS_YELLOW:
      setLight(UTARA,0,1,0); setLight(SELATAN,0,1,0);
      setLight(TIMUR,1,0,0); setLight(BARAT,1,0,0);
      Serial.println("[FASE] U+S=KUNING | T+B=MERAH"); break;
    case PHASE_EW_GREEN:
      setLight(UTARA,1,0,0); setLight(SELATAN,1,0,0);
      setLight(TIMUR,0,0,1); setLight(BARAT,0,0,1);
      Serial.println("[FASE] T+B=HIJAU | U+S=MERAH"); break;
    case PHASE_EW_YELLOW:
      setLight(UTARA,1,0,0); setLight(SELATAN,1,0,0);
      setLight(TIMUR,0,1,0); setLight(BARAT,0,1,0);
      Serial.println("[FASE] T+B=KUNING | U+S=MERAH"); break;
  }
}

unsigned long phaseDuration(Phase p) {
  return (p==PHASE_NS_GREEN||p==PHASE_EW_GREEN) ? DUR_GRN :
         (p==PHASE_NS_YELLOW||p==PHASE_EW_YELLOW) ? DUR_YEL : DUR_RED;
}

Phase nextPhase(Phase p) {
  switch(p) {
    case PHASE_NS_GREEN:  return PHASE_NS_YELLOW;
    case PHASE_NS_YELLOW: return PHASE_EW_GREEN;
    case PHASE_EW_GREEN:  return PHASE_EW_YELLOW;
    default:              return PHASE_NS_GREEN;
  }
}

// ─────────────────────────────────────────────────────────────
//  HTTP Handlers
// ─────────────────────────────────────────────────────────────
void handleRoot() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) return server.requestAuthentication();
  server.send(200, "text/html", OTA_PAGE);
}

void handleStatus() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) return server.requestAuthentication();
  String json = "{\"phase\":"+String((int)currentPhase)+
    ",\"elapsed\":"+String(millis()-phaseStart)+
    ",\"red\":"+String(DUR_RED)+
    ",\"yel\":"+String(DUR_YEL)+
    ",\"grn\":"+String(DUR_GRN)+"}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void handleSetDur() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) return server.requestAuthentication();
  if (!server.hasArg("plain")) { server.send(400,"text/plain","No body"); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) { server.send(400,"text/plain","JSON error"); return; }
  if (doc.containsKey("red")) DUR_RED = constrain((unsigned long)doc["red"], 1000UL, 60000UL);
  if (doc.containsKey("yel")) DUR_YEL = constrain((unsigned long)doc["yel"], 1000UL, 60000UL);
  if (doc.containsKey("grn")) DUR_GRN = constrain((unsigned long)doc["grn"], 1000UL, 60000UL);
  Serial.printf("[DUR] M=%lums K=%lums H=%lums\n", DUR_RED, DUR_YEL, DUR_GRN);
  server.send(200,"text/plain","OK");
}

void handleOTAUpload() {
  HTTPUpload& u = server.upload();
  if (u.status==UPLOAD_FILE_START) {
    Serial.printf("OTA: %s\n", u.filename.c_str());
    allRed();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (u.status==UPLOAD_FILE_WRITE) {
    if (Update.write(u.buf, u.currentSize) != u.currentSize) Update.printError(Serial);
  } else if (u.status==UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("OTA OK: %u bytes\n", u.totalSize);
    else Update.printError(Serial);
  }
}

void handleOTAResult() {
  if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) return server.requestAuthentication();
  server.send(200,"text/plain", Update.hasError()?"FAIL":"OK");
  delay(1000);
  ESP.restart();
}

// ─────────────────────────────────────────────────────────────
//  Setup & Loop
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  uint8_t pins[] = {
    PIN_U_RED,PIN_U_YEL,PIN_U_GRN,PIN_S_RED,PIN_S_YEL,PIN_S_GRN,
    PIN_T_RED,PIN_T_YEL,PIN_T_GRN,PIN_B_RED,PIN_B_YEL,PIN_B_GRN
  };
  for (uint8_t p : pins) { pinMode(p,OUTPUT); digitalWrite(p,LOW); }
  allRed(); delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status()!=WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());
  if (MDNS.begin(HOSTNAME)) Serial.printf("mDNS: http://%s.local\n", HOSTNAME);

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/setdur", HTTP_POST, handleSetDur);
  server.on("/update", HTTP_POST, handleOTAResult, handleOTAUpload);
  server.begin();
  Serial.println("Server OK");

  currentPhase = PHASE_NS_GREEN;
  phaseStart   = millis();
  applyPhase(currentPhase);
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - phaseStart >= phaseDuration(currentPhase)) {
    currentPhase = nextPhase(currentPhase);
    phaseStart   = now;
    applyPhase(currentPhase);
  }
}
