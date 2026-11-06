#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "update_manager.h"

WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

struct Config {
  String role;
  String wifiSsid;
  String wifiPass;
  String userId;
  String apiKey;
  String holderApiKey;
  bool handoverPending = false;
} cfg;

const unsigned long WIFI_TIMEOUT_MS = 15000;
const char* AP_SSID = "emlalock";
const char* AP_PASSWORD = "emlalock";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
bool setupAP = false;
String lastError;

void startSetupAP();
bool connectWiFi();
void maintainWiFi();
void sendJson(int code, JsonDocument& d);

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f4f5f7">
<title>EmlaLock</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--text:#151922;--muted:#69707d;--line:#e2e5ea;--accent:#222b3a;--danger:#b42318;--success:#18794e;--field:#fff;--fieldBorder:#cfd4dc;--notice:#f7f8fa}
:root[data-theme="dark"]{color-scheme:dark;--bg:#0b1020;--card:#121a2b;--text:#eef2ff;--muted:#9aa7c0;--line:#26334b;--accent:#6478ff;--danger:#ff8f9b;--success:#65d6a2;--field:#0d1526;--fieldBorder:#34435f;--notice:#17233a}
@media (prefers-color-scheme:dark){:root:not([data-theme]){color-scheme:dark;--bg:#0b1020;--card:#121a2b;--text:#eef2ff;--muted:#9aa7c0;--line:#26334b;--accent:#6478ff;--danger:#ff8f9b;--success:#65d6a2;--field:#0d1526;--fieldBorder:#34435f;--notice:#17233a}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}main{max-width:860px;margin:0 auto;padding:34px 20px 56px}h1{font-size:30px;letter-spacing:-.04em;margin:0 0 26px;font-weight:750}h2{font-size:18px;margin:0 0 18px}h3{font-size:14px;margin:24px 0 10px;font-weight:650}.card{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:24px;margin:16px 0;box-shadow:0 14px 36px rgba(0,0,0,.18)}.hidden{display:none!important}.muted{color:var(--muted)}.small{font-size:13px}.label{display:block;font-size:13px;font-weight:600;margin:14px 0 6px}input,select,button{width:100%;font:inherit;border-radius:9px;padding:11px 12px}input,select{background:var(--field);border:1px solid var(--fieldBorder);color:var(--text)}button{border:1px solid var(--accent);background:linear-gradient(135deg,#7184ff,#5268ee);color:#fff;cursor:pointer;font-weight:650;transition:transform .15s ease,filter .15s ease}button.secondary{background:var(--field);color:var(--text);border-color:var(--fieldBorder)}button.danger{background:var(--field);color:var(--danger);border-color:#e0b4b0}button:hover:not(:disabled){filter:brightness(1.08);transform:translateY(-1px)}button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid #8da0ff;outline-offset:2px}button:disabled{opacity:.55;cursor:wait}.roles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.roles button{background:var(--field);color:var(--text);border-color:var(--fieldBorder);padding:14px 8px}.roles button.selected{background:var(--accent);color:#fff;border-color:var(--accent)}.row{display:grid;grid-template-columns:1fr auto;gap:8px}.row button{width:auto;min-width:88px}.actions{display:grid;grid-template-columns:1fr 1fr;gap:8px}.statusline{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--line);padding:12px 0}.statusline:last-child{border-bottom:0}.statusValue{font-weight:600;text-align:right}.timeLabel{font-size:13px;font-weight:650;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-top:8px}.time{font-size:34px;font-weight:650;letter-spacing:-1px;margin:5px 0 2px;font-variant-numeric:tabular-nums}.mode{font-size:13px;font-weight:650;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}.notice{background:var(--notice);border:1px solid var(--line);border-radius:9px;padding:11px;margin-top:12px;font-size:13px}.success{color:var(--success)}.error{color:var(--danger)}.topline{display:flex;justify-content:space-between;align-items:center;gap:12px}.topline button{width:auto}.footerActions{display:grid;grid-template-columns:1fr 1fr;gap:8px}.loading{opacity:.65}
@media(max-width:500px){main{padding:20px 12px 40px}.roles{grid-template-columns:1fr}.actions{grid-template-columns:1fr}.footerActions{grid-template-columns:1fr}.time{font-size:30px}}
</style>
</head>
<body>
<main>
<h1>EmlaLock</h1>

<section id="setup" class="card hidden">
  <div class="topline"><h2>Setup</h2><button class="secondary" type="button" onclick="scanWifi()" id="scanTop">Scan Wi-Fi</button></div>
  <button id="settingsBack" type="button" class="secondary hidden" onclick="backToDashboard()" style="margin-bottom:12px">Back</button>
  <div id="apInfo" class="notice hidden">Setup network is active. Connect to <b>emlalock</b> with password <b>emlalock</b>, then open <b>192.168.4.1</b>.</div>
  <div id="handoverInfo" class="notice hidden">Handover setup: your Wi-Fi and Key Holder API key are required. The saved wearer User ID and API key remain private on this ESP32 and are not requested or displayed.</div>
  <p id="wifiInfo" class="muted small"></p>

  <h3>Appearance</h3>
  <select id="themeSelect" onchange="setTheme(this.value)">
    <option value="light">Light</option>
    <option value="dark">Dark</option>
    <option value="system">System</option>
  </select>

  <div id="roleSection"><h3>Role</h3>
  <div class="roles">
    <button type="button" id="roleWearer" onclick="chooseRole('wearer')">Wearer</button>
    <button type="button" id="roleHolder" onclick="chooseRole('holder')">Key Holder</button>
   </div>
  <p id="roleText" class="muted small"></p></div>

  <div id="fields" class="hidden">
    <h3>Wi-Fi</h3>
    <div class="row"><select id="ssid"><option value="">Select network</option></select><button type="button" onclick="scanWifi()" id="scanBtn">Scan</button></div>
    <label class="label">Or enter SSID</label>
    <input id="ssidManual" autocomplete="off" placeholder="Network name">
    <label class="label">Wi-Fi password</label>
    <input id="wpass" type="password" autocomplete="off" placeholder="Leave blank to keep saved password">

    <div id="keyFields">
      <h3>EmlaLock</h3>
      <div id="wearerFields">
        <label class="label">Wearer User ID</label>
        <input id="uid" autocomplete="off">
        <label class="label">Wearer API key</label>
        <input id="akey" type="password" autocomplete="off" placeholder="Enter API key">
      </div>
      <div id="holderField" class="hidden">
        <label class="label">Key Holder API key</label>
        <input id="hkey" type="password" autocomplete="off" placeholder="Enter holder API key">
      </div>
    </div>
    <div id="keyLockedNotice" class="notice hidden">API keys are configured and locked. They are never shown here. Use <b>Clear credentials</b> to erase them before entering new API keys.</div>
    <button type="button" onclick="saveConfig()" id="saveBtn" style="margin-top:18px">Save and Connect</button>
    <div id="setupMsg" class="notice hidden"></div>
  </div>
</section>

<section id="dash" class="hidden">
  <div class="card">
    <div class="mode" id="modeName">Wearer Mode</div>
    <div class="timeLabel">Time Passed</div>
    <div class="time" id="timePassed">--</div>
    <div class="timeLabel">Time Left</div>
    <div class="time" id="time">--</div>
    <div id="status" class="small muted">Connecting</div>
  </div>

  <div class="card">
    <div class="statusline"><span class="muted">Account</span><span class="statusValue" id="who">--</span></div>
    <div class="statusline"><span class="muted">Session status</span><span class="statusValue" id="sessionStatus">--</span></div>
    <div class="statusline"><span class="muted">Minimum</span><span class="statusValue" id="minimum">--</span></div>
    <div class="statusline"><span class="muted">Maximum</span><span class="statusValue" id="maximum">--</span></div>
    <div class="statusline"><span class="muted">Session ID</span><span class="statusValue" id="sessionId">--</span></div>
  </div>

  <div class="card">
    <h2>Change Duration</h2>
    <label class="label">Change</label>
    <select id="durationOp">
      <option value="add">Add time</option>
      <option value="sub">Subtract time</option>
    </select>
    <div class="row" style="margin-top:10px">
      <input id="durationAmount" type="number" min="1" step="1" value="1" inputmode="numeric">
      <select id="durationUnit" style="width:auto;min-width:130px">
        <option value="60">Minutes</option>
        <option value="3600" selected>Hours</option>
        <option value="86400">Days</option>
        <option value="604800">Weeks</option>
      </select>
    </div>
    <label class="label">Reason <span class="muted">(required)</span></label>
    <input id="durationReason" maxlength="49" required placeholder="Enter a reason for this change">
    <button type="button" onclick="changeDuration()" id="durationBtn" style="margin-top:12px">Change Duration</button>
    <div id="subNote" class="notice hidden"></div>
    <div id="msg" class="notice hidden"></div>
  </div>


  <div class="card">
    <h2>Software Update</h2>
    <p id="updateStatus" class="muted small">Checking for updates...</p>
    <button type="button" onclick="checkForUpdate()" id="updateCheckBtn" class="secondary">Check for updates</button>
    <button type="button" onclick="startUpdate()" id="updateBtn" style="margin-top:8px" disabled>Update firmware</button>
  </div>

  <div id="handoverCard" class="card hidden">
    <h2>Hand Over</h2>
    <p class="muted small">Preserve the wearer User ID and API key privately, clear this Wi-Fi connection, and let the keyholder configure their own Wi-Fi and API key.</p>
    <button type="button" class="danger" onclick="handover()">Hand Over to Keyholder</button>
  </div>
  <div class="card footerActions">
    <button type="button" class="secondary" onclick="showSettings()">Settings</button>
    <button type="button" id="clearCredentialsBtn" class="danger" onclick="clearCfg()">Clear credentials</button>
  </div>
</section>
</main>

<script>
let selectedRole='';
let startTimeMs=0;
let endTimeMs=0;
const $=id=>document.getElementById(id);

function applyTheme(theme){
  const root=document.documentElement;
  if(theme==='light'||theme==='dark')root.setAttribute('data-theme',theme);
  else root.removeAttribute('data-theme');
  localStorage.setItem('emlalock-theme',theme);
  const meta=document.querySelector('meta[name="theme-color"]');
  if(meta)meta.content=theme==='dark'?'#11151b':theme==='system'&&window.matchMedia('(prefers-color-scheme:dark)').matches?'#11151b':'#f4f5f7';
  const select=$('themeSelect');
  if(select)select.value=theme;
}
function setTheme(theme){applyTheme(theme)}
function initTheme(){applyTheme(localStorage.getItem('emlalock-theme')||'dark')}

function chooseRole(role){
  if(window.handoverPending)role='holder';
  if(window.roleLocked&&role!==selectedRole)return;
  selectedRole=role;
  ['wearer','holder'].forEach(r=>{
    $('role'+r.charAt(0).toUpperCase()+r.slice(1)).classList.toggle('selected',r===role);
  });
  $('fields').classList.remove('hidden');
  $('holderField').classList.toggle('hidden',role!=='holder');
  $('roleSection').classList.toggle('hidden',!!window.roleLocked||!!window.handoverPending);
  $('wearerFields').classList.toggle('hidden',!!window.handoverPending||(!!window.roleLocked&&role==='holder'));
  $('handoverInfo').classList.toggle('hidden',!window.handoverPending);
  $('roleText').textContent=window.handoverPending?'Key Holder handover setup: enter your Wi-Fi and your own Key Holder API key. Wearer credentials are already stored privately.':role==='holder'?'Key Holder mode is locked.':'Wearer mode uses your wearer credentials.';
}

function setMode(role){
  selectedRole=role;
  document.body.classList.toggle('holder-mode',role==='holder');
  $('accountLine').classList.toggle('hidden',role==='holder');
  $('clearCredentialsBtn').classList.toggle('hidden',role==='holder');
  $('handoverCard').classList.toggle('hidden',role!=='wearer');
  $('modeName').textContent=role==='holder'?'Key Holder Mode':'Wearer Mode';
  const holder=role==='holder';
   $('durationOp').value='add';
  $('durationOp').querySelector('option[value="sub"]').disabled=!holder;
  $('subNote').textContent='Subtract time is only available in Key Holder mode.';
  $('subNote').classList.toggle('hidden',holder);
}

async function jsonFetch(url,opts){
  const r=await fetch(url,opts);
  const t=await r.text();
  let d;
  try{d=JSON.parse(t)}catch(e){throw new Error('Invalid response from ESP32')}
  if(!r.ok)throw new Error(d.error||('HTTP '+r.status));
  return d;
}

async function scanWifi(){
  const b=$('scanBtn');
  if(b)b.disabled=true;
  const top=$('scanTop');if(top)top.disabled=true;
  try{
    const x=await jsonFetch('/api/scan');
    const s=$('ssid');
    s.innerHTML='<option value="">Select network</option>';
    (x.networks||[]).forEach(n=>{
      const o=document.createElement('option');
      o.value=n.ssid;o.textContent=n.ssid+'  ('+n.rssi+' dBm)';s.appendChild(o);
    });
    s.onchange=()=>{if(s.value)$('ssidManual').value=s.value};
    $('setupMsg').textContent=(x.networks||[]).length?'Found '+x.networks.length+' network(s).':'No networks found. Enter the SSID manually.';
    $('setupMsg').classList.remove('hidden');
  }catch(e){
    $('setupMsg').textContent='Wi-Fi scan failed: '+e.message;
    $('setupMsg').classList.remove('hidden');
  }finally{
    if(b){b.disabled=false;b.textContent='Scan'}
    if(top)top.disabled=false;
  }
}

function getSsid(){return $('ssid').value||$('ssidManual').value.trim()}

async function saveConfig(){
  if(!selectedRole){alert('Choose a role first.');return}
  if(window.roleLocked){selectedRole=window.savedRole||selectedRole}
  const ssid=getSsid();
  if(!ssid){alert('Choose or enter a Wi-Fi network.');return}
  const b=$('saveBtn');b.disabled=true;b.textContent='Saving...';
  try{
    const payload={role:window.handoverPending?'holder':selectedRole,ssid,wifiPass:$('wpass').value,userId:window.handoverPending?'':$('uid').value,apiKey:window.handoverPending?'':$('akey').value,holderApiKey:$('hkey').value};
    const x=await jsonFetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    if(!x.ok)throw new Error(x.error||'Save failed');
    $('setupMsg').textContent='Saved. Connecting to Wi-Fi...';$('setupMsg').classList.remove('hidden');
    setTimeout(load,1200);
  }catch(e){alert(e.message)}finally{b.disabled=false;b.textContent='Save and Connect'}
}

async function load(){
  try{
    const x=await jsonFetch('/api/status');
    if(!x.configured){
      window.handoverPending=!!x.handoverPending;
      window.roleLocked=window.handoverPending;window.savedRole=window.handoverPending?'holder':'';window.credentialsLocked=false;
      ['Wearer','Holder','Alone'].forEach(r=>$('role'+r).disabled=window.handoverPending);
      $('keyFields').classList.remove('hidden');$('keyLockedNotice').classList.add('hidden');
      startTimeMs=0;
      endTimeMs=0;
      $('settingsBack').classList.add('hidden');
      $('setup').classList.remove('hidden');$('dash').classList.add('hidden');$('apInfo').classList.toggle('hidden',!x.setupAP);$('wifiInfo').textContent=window.handoverPending?'Enter your Wi-Fi and Key Holder API key to finish handover.':(x.setupAP?'Setup access point is active.':(x.ssid?'Saved Wi-Fi: '+x.ssid:'Enter your Wi-Fi and EmlaLock credentials.'));$('ssidManual').value=x.ssid||'';
      if(window.handoverPending){selectedRole='holder';chooseRole('holder');$('roleWearer').disabled=true;$('roleHolder').disabled=true; $('holderField').classList.remove('hidden');}
      else $('handoverInfo').classList.add('hidden');
      return;
    }
    window.handoverPending=!!x.handoverPending;
    $('settingsBack').classList.add('hidden');$('setup').classList.add('hidden');$('dash').classList.remove('hidden');window.savedRole=x.role||'';window.roleLocked=!!window.savedRole;window.credentialsLocked=true;setMode(x.role);refresh();
  }catch(e){
    startTimeMs=0;
    endTimeMs=0;
    $('setup').classList.remove('hidden');$('dash').classList.add('hidden');$('apInfo').classList.remove('hidden');$('wifiInfo').textContent='Unable to contact the ESP32. Open 192.168.4.1 if needed.';
  }
}

function formatTime(v){
  if(v==null)return'--';v=Number(v);if(!Number.isFinite(v))return String(v);
  v=Math.max(0,Math.floor(v));
  let d=Math.floor(v/86400);v%=86400;let h=Math.floor(v/3600);v%=3600;let m=Math.floor(v/60);let s=Math.floor(v%60);
  return `${d}d ${h}h ${m}m ${s}s`;
}

function renderTimeCounters(){
  const now=Date.now();
  if(!startTimeMs){$('timePassed').textContent='--';}
  else $('timePassed').textContent=formatTime(Math.max(0,Math.floor((now-startTimeMs)/1000)));
  if(!endTimeMs){$('time').textContent='--';return;}
  const seconds=Math.max(0,Math.floor((endTimeMs-now)/1000));
  $('time').textContent=formatTime(seconds);
  if(seconds===0){
    $('sessionStatus').textContent='Ended';
  }
}

async function refresh(){
  try{
    const x=await jsonFetch('/api/status');
    if(!x.ok){startTimeMs=0;endTimeMs=0;$('timePassed').textContent='--';$('time').textContent='--';$('status').textContent=x.error||'API error';$('status').className='small error';return}
    setMode(x.role);
    const s=x.chastitysession||{},u=x.user||{};
    $('who').textContent=u.username||x.userId||'--';
    const rawStart=Number(s.startdate);
    const rawEnd=Number(s.enddate);
    startTimeMs=Number.isFinite(rawStart)?(rawStart<100000000000?rawStart*1000:rawStart):0;
    endTimeMs=Number.isFinite(rawEnd)?(rawEnd<100000000000?rawEnd*1000:rawEnd):0;
    renderTimeCounters();
    $('status').textContent='Connected';$('status').className='small success';
    $('sessionStatus').textContent=s.status??'--';
    $('minimum').textContent=formatTime(s.minduration);
    $('maximum').textContent=formatTime(s.maxduration);
    $('sessionId').textContent=s.chastitysessionid||'--';
  }catch(e){$('status').textContent=e.message;$('status').className='small error'}
}

async function changeDuration(){
  const op=$('durationOp').value;
  const amount=Math.floor(Number($('durationAmount').value));
  const multiplier=Number($('durationUnit').value);
  const reason=$('durationReason').value.trim();
  if(!Number.isFinite(amount)||amount<1){$('msg').textContent='Enter a duration of at least 1.';$('msg').classList.remove('hidden');return}
  if(op==='sub'&&selectedRole!=='holder'){ $('msg').textContent='Subtract time is only available in Key Holder mode.';$('msg').classList.remove('hidden');return}
  if(!reason){$('msg').textContent='A reason is required.';$('msg').classList.remove('hidden');$('durationReason').focus();return}
  if(reason.length>49){$('msg').textContent='Reason must be 49 characters or fewer.';$('msg').classList.remove('hidden');return}
  const value=String(amount*multiplier);
  if(!confirm((op==='add'?'Add ':'Subtract ')+formatTime(Number(value))+'?\nReason: '+reason))return;
  const b=$('durationBtn');b.disabled=true;
  try{
    const x=await jsonFetch('/api/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({op,value,reason})});
    $('msg').textContent=x.ok?'Duration changed.':(x.error||'Action failed');$('msg').classList.remove('hidden');
    $('durationReason').value='';
    await refresh();
  }catch(e){$('msg').textContent=e.message;$('msg').classList.remove('hidden')}finally{b.disabled=false}
}
function backToDashboard(){
  $('settingsBack').classList.add('hidden');
  $('setup').classList.add('hidden');
  $('dash').classList.remove('hidden');
  load();
}

async function showSettings(){
  try{
    const x=await jsonFetch('/api/config');
    $('settingsBack').classList.remove('hidden');$('dash').classList.add('hidden');$('setup').classList.remove('hidden');$('apInfo').classList.toggle('hidden',!x.setupAP);window.handoverPending=!!x.handoverPending;selectedRole=x.role||'';window.savedRole=selectedRole;window.roleLocked=!!selectedRole;window.credentialsLocked=!!x.configured;
    if(selectedRole)chooseRole(selectedRole);
    initTheme();
    ['Wearer','Holder'].forEach(r=>$('role'+r).disabled=window.roleLocked);
    $('keyFields').classList.toggle('hidden',window.credentialsLocked);$('keyLockedNotice').classList.toggle('hidden',!window.credentialsLocked);
    $('roleSection').classList.toggle('hidden',!!window.roleLocked||!!window.handoverPending);
    $('wearerFields').classList.toggle('hidden',selectedRole==='holder'||!!window.handoverPending);
    $('holderField').classList.toggle('hidden',selectedRole!=='holder');
    $('ssidManual').value=x.ssid||'';$('uid').value='';
  }catch(e){alert(e.message)}
}

async function handover(){
  if(!confirm('Hand over this ESP32? The wearer User ID and API key will remain stored privately. Wi-Fi settings and any saved keyholder API key will be cleared.'))return;
  try{
    await jsonFetch('/api/handover',{method:'POST'});
    $('dash').classList.add('hidden');$('setup').classList.remove('hidden');
    $('wifiInfo').textContent='Handover enabled. Connect to the emlalock setup Wi-Fi and configure your own network and Key Holder API key.';
    setTimeout(load,700);
  }catch(e){alert(e.message)}
}

async function clearCfg(){
  if(!confirm('Erase saved credentials and configuration?'))return;
  try{await jsonFetch('/api/clear',{method:'POST'});location.href='http://192.168.4.1/'}catch(e){alert(e.message)}
}


async function checkForUpdate(){
  const b=$('updateCheckBtn');const u=$('updateBtn');
  b.disabled=true;$('updateStatus').textContent='Checking GitHub...';u.disabled=true;
  try{
    const x=await jsonFetch('/api/update/check');
    if(!x.ok)throw new Error(x.error||'Update check failed');
    if(x.update){$('updateStatus').textContent='Update available: v'+x.latest+' (current v'+x.current+')';u.disabled=false;}
    else $('updateStatus').textContent='You are up to date (v'+x.current+').';
  }catch(e){$('updateStatus').textContent=e.message;}finally{b.disabled=false;}
}

async function startUpdate(){
  if(!confirm('Install the available firmware update now? The ESP32 will restart.'))return;
  const b=$('updateBtn');b.disabled=true;$('updateCheckBtn').disabled=true;
  const started=Date.now();
  $('updateStatus').textContent='Starting firmware update...';
  try{
    const x=await jsonFetch('/api/update/start',{method:'POST'});
    if(!x.updating)throw new Error(x.error||'No update available');
    $('updateStatus').textContent='Update started. Waiting for ESP32 to restart... 0s';
    const timer=setInterval(()=>{
      const seconds=Math.floor((Date.now()-started)/1000);
      $('updateStatus').textContent='Update started. Waiting for ESP32 to restart... '+seconds+'s';
    },1000);
    setTimeout(()=>{
      clearInterval(timer);
      $('updateStatus').textContent='ESP32 should be restarting. Reconnecting...';
      let attempts=0;
      const poll=setInterval(async()=>{
        attempts++;
        try{
          const r=await fetch('/api/status',{cache:'no-store'});
          if(r.ok){
            clearInterval(poll);
            $('updateStatus').textContent='ESP32 is back online. Refreshing...';
            setTimeout(()=>location.reload(),800);
          }
        }catch(e){}
        if(attempts>=30){
          clearInterval(poll);
          $('updateStatus').textContent='ESP32 has not responded yet. Refresh this page in a few seconds.';
          $('updateCheckBtn').disabled=false;
        }
      },1000);
    },4000);
  }catch(e){$('updateStatus').textContent=e.message;b.disabled=false;$('updateCheckBtn').disabled=false;}
}
initTheme();
window.matchMedia('(prefers-color-scheme:dark)').addEventListener('change',()=>{if((localStorage.getItem('emlalock-theme')||'system')==='system')initTheme()});
window.addEventListener('load',load);
setInterval(()=>{if(!$('dash').classList.contains('hidden'))renderTimeCounters()},1000);
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refresh()},30000);
initTheme();
</script>
</body>
</html>
)HTML";

void loadConfig(){
  cfg=Config();
  if(!prefs.begin("emlalock", true))return;
  cfg.role=prefs.getString("role","");
  cfg.wifiSsid=prefs.getString("ssid","");
  cfg.wifiPass=prefs.getString("wpass","");
  cfg.userId=prefs.getString("uid","");
  cfg.apiKey=prefs.getString("akey","");
  if(prefs.isKey("hkey"))cfg.holderApiKey=prefs.getString("hkey","");
  cfg.handoverPending=prefs.getBool("handover",false);
  prefs.end();
}

void saveConfig(JsonDocument& d){
  String role=d["role"]|"";
  String ssid=d["ssid"]|"";
  String pass=d["wifiPass"]|"";
  String uid=d["userId"]|"";
  String key=d["apiKey"]|"";
  String hkey=d["holderApiKey"]|"";
  prefs.begin("emlalock",false);
  prefs.putString("role",role);
  prefs.putString("ssid",ssid);
  if(pass.length())prefs.putString("wpass",pass);
  prefs.putString("uid",uid);
  if(key.length())prefs.putString("akey",key);
  if(hkey.length())prefs.putString("hkey",hkey); else if(role!="holder"&&prefs.isKey("hkey"))prefs.remove("hkey");
  prefs.end();
  loadConfig();
}

String enc(String s){
  s.replace("%","%25");s.replace(" ","%20");s.replace("+","%2B");s.replace("&","%26");s.replace("?","%3F");s.replace("#","%23");
  return s;
}

bool apiGet(const String& path,String& out){
  WiFiClientSecure client;client.setInsecure();
  HTTPClient http;
  String url="https://api.emlalock.com/"+path;
  if(!http.begin(client,url)){lastError="HTTPS connection could not be started";return false;}
  int code=http.GET();out=http.getString();http.end();
  if(code<200||code>=300){lastError=String("EmlaLock HTTP ")+code+": "+out;return false;}
  return true;
}

void sendJson(int code,JsonDocument& d){String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.sendHeader("X-Content-Type-Options","nosniff");server.send(code,"application/json",out);}

void handleStatus(){
  JsonDocument d;
  bool configured=!cfg.handoverPending&&cfg.role.length()>0&&cfg.userId.length()>0&&cfg.apiKey.length()>0&&cfg.wifiSsid.length()>0;
  d["configured"]=configured;d["handoverPending"]=cfg.handoverPending;d["role"]=cfg.role;d["ssid"]=cfg.wifiSsid;d["setupAP"]=setupAP;
  if(!configured){sendJson(200,d);return;}
  if(WiFi.status()!=WL_CONNECTED){d["ok"]=false;d["error"]="Wi-Fi is not connected";sendJson(200,d);return;}
  String out;
  if(!apiGet("info?userid="+enc(cfg.userId)+"&apikey="+enc(cfg.apiKey),out)){d["ok"]=false;d["error"]=lastError;sendJson(200,d);return;}
  JsonDocument info;
  if(deserializeJson(info,out)){d["ok"]=false;d["error"]="Invalid JSON from EmlaLock";sendJson(200,d);return;}
  d["ok"]=true;
  JsonObject user=info["user"].as<JsonObject>();
  user.remove("userid");
  d["user"]=user;
  JsonObject session=info["chastitysession"].as<JsonObject>();
  session.remove("wearerid");session.remove("holderid");session.remove("creatorid");session.remove("offeruserid");
  d["chastitysession"]=session;
  sendJson(200,d);
}

void handleConfigGet(){
  JsonDocument d;d["role"]=cfg.role;d["ssid"]=cfg.wifiSsid;d["configured"]=!cfg.handoverPending&&cfg.role.length()>0;d["handoverPending"]=cfg.handoverPending;d["setupAP"]=setupAP;sendJson(200,d);
}

void handleConfigPost(){
  JsonDocument d;
  if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json",R"({"ok":false,"error":"Invalid JSON"})");return;}
  String role=d["role"]|"";String ssid=d["ssid"]|"";String pass=d["wifiPass"]|"";String uid=d["userId"]|"";String key=d["apiKey"]|"";String hkey=d["holderApiKey"]|"";
  if(role!="wearer"&&role!="holder"){server.send(400,"application/json",R"({"ok":false,"error":"Invalid role"})");return;}
  if(cfg.handoverPending){
    if(role!="holder"||!ssid.length()||!hkey.length()) {server.send(400,"application/json",R"({"ok":false,"error":"Handover setup requires Wi-Fi and a Key Holder API key."})");return;}
    prefs.begin("emlalock",false);
    prefs.putString("role","holder");prefs.putString("ssid",ssid);prefs.putString("wpass",pass);prefs.putString("hkey",hkey);prefs.putBool("handover",false);
    prefs.end();
    loadConfig();
    if(!connectWiFi()){server.send(200,"application/json",R"({"ok":false,"error":"Saved, but Wi-Fi connection failed. Setup network remains available."})");return;}
    server.send(200,"application/json",R"({"ok":true})");return;
  }
  if(cfg.role.length()&&role!=cfg.role){server.send(409,"application/json",R"({"ok":false,"error":"Role is locked. Use Clear credentials before changing role."})");return;}
  if(cfg.apiKey.length()&&key.length()){server.send(409,"application/json",R"({"ok":false,"error":"Wearer API key is locked. Use Clear credentials before changing it."})");return;}
  if(cfg.holderApiKey.length()&&hkey.length()){server.send(409,"application/json",R"({"ok":false,"error":"Key Holder API key is locked. Use Clear credentials before changing it."})");return;}
  if (!ssid.length()) {
    ssid = cfg.wifiSsid;
  }
  if (!ssid.length()) {
    server.send(400, "application/json", R"({"ok":false,"error":"Wi-Fi SSID is required"})");
    return;
  }
  if(!uid.length()||(!key.length()&&!cfg.apiKey.length())){server.send(400,"application/json",R"({"ok":false,"error":"Wearer User ID and API key are required"})");return;}
  if(role=="holder"&&!hkey.length()&&!cfg.holderApiKey.length()){server.send(400,"application/json",R"({"ok":false,"error":"Holder API key is required for Key Holder mode"})");return;}
  if(!pass.length()&&cfg.wifiSsid!=ssid){server.send(400,"application/json",R"({"ok":false,"error":"Wi-Fi password is required for a new network"})");return;}
  saveConfig(d);
  if(!connectWiFi()){server.send(200,"application/json",R"({"ok":false,"error":"Saved, but Wi-Fi connection failed. Setup network remains available."})");return;}
  server.send(200,"application/json",R"({"ok":true})");
}

void handleScan(){
  int n=WiFi.scanNetworks(false,true);
  JsonDocument d;JsonArray a=d["networks"].to<JsonArray>();
  for(int i=0;i<n;i++){JsonObject o=a.add<JsonObject>();o["ssid"]=WiFi.SSID(i);o["rssi"]=WiFi.RSSI(i);}
  WiFi.scanDelete();sendJson(200,d);
}

void handleHandover(){
  if(cfg.role!="wearer"||!cfg.userId.length()||!cfg.apiKey.length()){
    server.send(409,"application/json",R"({"ok":false,"error":"Hand Over is available after wearer setup."})");return;
  }
  prefs.begin("emlalock",false);
  prefs.putString("role","holder");
  prefs.remove("ssid");prefs.remove("wpass");prefs.remove("hkey");
  prefs.putBool("handover",true);
  prefs.end();
  cfg.role="holder";cfg.wifiSsid="";cfg.wifiPass="";cfg.holderApiKey="";cfg.handoverPending=true;
  server.send(200,"application/json",R"({"ok":true,"handoverPending":true})");
  delay(150);
  WiFi.disconnect(false,false);
  startSetupAP();
}

void handleClear(){
  prefs.begin("emlalock",false);String savedSsid=prefs.getString("ssid","");String savedPass=prefs.getString("wpass","");prefs.clear();if(savedSsid.length())prefs.putString("ssid",savedSsid);if(savedPass.length())prefs.putString("wpass",savedPass);prefs.end();cfg=Config();cfg.wifiSsid=savedSsid;cfg.wifiPass=savedPass;startSetupAP();server.send(200,"application/json",R"({"ok":true})");
}

void handleAction(){
  JsonDocument d;
  if(deserializeJson(d,server.arg("plain"))){server.send(400,"application/json",R"({"ok":false,"error":"Invalid JSON"})");return;}
  String op=d["op"]|"";String value=d["value"]|"";String reason=d["reason"]|"";reason.trim();
  if(op!="add"&&op!="sub"){server.send(400,"application/json",R"({"ok":false,"error":"Invalid action"})");return;}
  if(!value.length()){server.send(400,"application/json",R"({"ok":false,"error":"Value is required"})");return;}
  if(!reason.length()){server.send(400,"application/json",R"({"ok":false,"error":"Reason is required"})");return;}
  if(reason.length()>49){server.send(400,"application/json",R"({"ok":false,"error":"Reason must be 49 characters or fewer"})");return;}
  if(op=="sub"&&cfg.role!="holder"){server.send(403,"application/json",R"({"ok":false,"error":"Subtract time is only available in Key Holder mode"})");return;}
  if(op=="sub"&&cfg.role=="holder"&&!cfg.holderApiKey.length()){server.send(400,"application/json",R"({"ok":false,"error":"Holder API key is required"})");return;}
  if(WiFi.status()!=WL_CONNECTED){server.send(503,"application/json",R"({"ok":false,"error":"Wi-Fi is not connected"})");return;}
  String path=op+"?userid="+enc(cfg.userId)+"&apikey="+enc(cfg.apiKey)+"&value="+enc(value)+"&text="+enc(reason);
  if(op=="sub"&&cfg.holderApiKey.length())path+="&holderapikey="+enc(cfg.holderApiKey);
  String out;
  if(!apiGet(path,out)){JsonDocument e;e["ok"]=false;e["error"]=lastError;sendJson(502,e);return;}
  JsonDocument r;r["ok"]=true;sendJson(200,r);
}

void captive(){server.sendHeader("Location","http://192.168.4.1/",true);server.send(302,"text/plain","");}

void noContent(){server.send(204,"text/plain","");}

void startSetupAP(){
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP,AP_GATEWAY,AP_SUBNET);
  if(!setupAP){
    if(WiFi.softAP(AP_SSID,AP_PASSWORD)){setupAP=true;Serial.println("Setup AP ready at 192.168.4.1");}
    else Serial.println("Setup AP failed");
  }
  if(!dnsServer.start(53,"*",AP_IP))Serial.println("Setup DNS start failed");
}

bool connectWiFi(){
  if(!cfg.wifiSsid.length())return false;
  WiFi.mode(WIFI_AP_STA);
  if(WiFi.status()==WL_CONNECTED){
    if(WiFi.SSID()==cfg.wifiSsid){setupAP=false;return true;}
    WiFi.disconnect(false,false);
    delay(50);
  }
  WiFi.begin(cfg.wifiSsid.c_str(),cfg.wifiPass.c_str());
  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED&&millis()-start<WIFI_TIMEOUT_MS){server.handleClient();delay(50);}
  if(WiFi.status()==WL_CONNECTED){setupAP=false;Serial.print("Wi-Fi connected: ");Serial.println(WiFi.localIP());return true;}
  Serial.println("Wi-Fi connection failed");
  startSetupAP();
  return false;
}

void maintainWiFi(){
  static unsigned long lastAttempt=0;
  if(!cfg.wifiSsid.length())return;
  if(WiFi.status()==WL_CONNECTED)return;
  if(millis()-lastAttempt<10000)return;
  lastAttempt=millis();
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(cfg.wifiSsid.c_str(),cfg.wifiPass.c_str());
}

void setup(){
  Serial.begin(115200);delay(300);loadConfig();
  WiFi.mode(WIFI_AP_STA);startSetupAP();
  server.on("/",HTTP_GET,[](){server.send(200,"text/html",INDEX_HTML);});
  server.on("/favicon.ico",HTTP_GET,noContent);
  server.on("/robots.txt",HTTP_GET,noContent);
  server.on("/generate_204",HTTP_GET,captive);
  server.on("/hotspot-detect.html",HTTP_GET,captive);
  server.on("/connecttest.txt",HTTP_GET,captive);
  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/config",HTTP_GET,handleConfigGet);
  server.on("/api/config",HTTP_POST,handleConfigPost);
  server.on("/api/scan",HTTP_GET,handleScan);
  server.on("/api/clear",HTTP_POST,handleClear);
  server.on("/api/handover",HTTP_POST,handleHandover);
  server.on("/api/action",HTTP_POST,handleAction);
  initUpdateManager();
  server.onNotFound(captive);
  if(!cfg.handoverPending&&cfg.role.length()&&cfg.userId.length()&&cfg.apiKey.length()&&cfg.wifiSsid.length())connectWiFi();
  server.begin();
  Serial.println("HTTP server started");
}

void loop(){dnsServer.processNextRequest();server.handleClient();delay(2);}
