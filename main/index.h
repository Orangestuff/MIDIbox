#ifndef INDEX_H
#define INDEX_H

    const char* INDEX_HTML = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset='UTF-8'> <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
    /* ... rest of your CSS ... */
    body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: #121212; color: #e0e0e0; padding: 20px; max-width: 1200px; margin: auto; }
    h2 { color: #00d1b2; text-align: center; text-transform: uppercase; letter-spacing: 2px; margin-bottom: 30px; }
    
    .wifi-card { background: #1e1e1e; padding: 20px; border-radius: 10px; border-left: 5px solid #ffa502; margin-bottom: 30px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    .power-card { background: #1e1e1e; padding: 20px; border-radius: 10px; border-left: 5px solid #02ff0f; margin-bottom: 30px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    .exp-card { background: #1e1e1e; padding: 20px; border-radius: 10px; border-left: 5px solid #e74c3c; margin-bottom: 30px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    
    .wifi-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 15px; }
    
    .bank-bar { display: flex; gap: 10px; margin-bottom: 20px; justify-content: center; }
    .bank-btn { flex: 1; padding: 15px; background: #333; color: #aaa; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; font-size: 1.1em; transition: 0.3s; text-transform: uppercase; border-bottom: 4px solid transparent; }
    .bank-btn:hover { background: #444; }

    .control-box { background: #333; padding: 15px; border-radius: 5px; margin-bottom: 20px; display: flex; align-items: center; justify-content: space-between; }
    input[type=range] { width: 100%; margin: 0 15px; accent-color: #00d1b2; cursor: pointer; }
    
    #sws { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; margin-bottom: 30px; }
    
    .sw { background: #1e1e1e; padding: 15px; border-radius: 8px; border-top: 5px solid #00d1b2; box-shadow: 0 4px 8px rgba(0,0,0,0.4); transition: 0.3s; }
    .sw h3 { margin: 0 0 10px 0; font-size: 0.9em; text-align: center; color: #00d1b2; border-bottom: 1px solid #333; padding-bottom: 5px; }

    .grid-section { margin-bottom: 10px; }
    .label-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 2px; }
    label { font-size: 0.65em; color: #888; text-transform: uppercase; }
    .input-group { display: grid; grid-template-columns: 1.5fr 1fr 1fr; gap: 4px; margin-bottom: 8px; transition: 0.3s; }
    .disabled { opacity: 0.2; filter: grayscale(100%); pointer-events: none; }

    select, input[type=number], input[type=text], input[type=password] { background: #2d2d2d; color: #fff; border: 1px solid #444; padding: 6px; border-radius: 4px; width: 100%; font-size: 0.85em; box-sizing: border-box; }
    input:focus, select:focus { border-color: #00d1b2; outline: none; }
    input[type=checkbox] { accent-color: #00d1b2; cursor: pointer; width: 20px; height: 20px; }

    button { border: 0; padding: 12px; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.2s; text-transform: uppercase; }
    .btn-main { background: #00d1b2; color: #121212; width: 100%; font-size: 1em; height: 50px; position: sticky; bottom: 10px; box-shadow: 0 -5px 15px rgba(0,0,0,0.5); }
    .btn-wifi { background: #ffa502; color: #121212; }
    .btn-scan { background: #444; color: #eee; margin-bottom: 10px; }
    .btn-cal { background: #444; color: #fff; font-size: 0.7em; padding: 6px; margin-top: 4px; width: 100%; }
    .btn-cal:hover { background: #666; }
    
    @media (max-width: 1000px) { #sws { grid-template-columns: repeat(2, 1fr); } }
    @media (max-width: 600px) { #sws { grid-template-columns: 1fr; } .wifi-grid { grid-template-columns: 1fr; } }

    /* --- STYLES FOR ACCORDION & SMART HIDING --- */
    details { background: #252525; padding: 5px; border-radius: 4px; margin-top: 5px; }
    summary { cursor: pointer; font-size: 0.75em; color: #aaa; outline: none; padding: 5px; font-weight: bold; text-transform: uppercase; list-style: none; }
    summary::-webkit-details-marker { display: none; } /* Hide default triangle */
    summary:after { content: '+'; float: right; font-weight: bold; }
    details[open] summary:after { content: '-'; }
    
    .hidden-input { display: none !important; }
</style></head>
<body>
    <h2>MIDI Pedal Master Config</h2>
    
    <div class='wifi-card'>
        <h3>Connectivity</h3>
        <div class='wifi-grid'>
            <div id='ssid-container'><label>SSID</label><input type='text' id='ssid'></div>
            <div><label>Password</label><input type='password' id='pass'></div>
        </div>
        <button class='btn-scan' id='scan-btn' onclick='scan()'>Scan WiFi</button>
        <button class='btn-wifi' onclick='saveWifi()'>Save WiFi & Reboot</button>
    </div>
<div class='power-card'>
        <h3>Power Saving</h3>
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
            <label>Auto-Sleep Enabled</label>
            <div class="toggle-switch">
                <input type="checkbox" id="ds_en" onchange="updGlob('ds_en', this.checked)">
                <span class="slider"></span>
            </div>
        </div>
        <div>
            <label>Idle Timeout (Minutes)</label>
            <input type="number" id="ds_min" min="1" max="120" style="width:100%;" onchange="updGlob('ds_min', this.value)">
        </div>
    </div>
<div class='exp-card'>
        <h3 style="margin-top:0;">Expression Config (Bank <span id="exp_bank_num"></span>)</h3>
        
        <div class='wifi-grid'>
            <div><label>Channel (1-16)</label><input type='number' id='exp_chan' min='1' max='16' onchange="updExp('ch', this.value)"></div>
            
            <div>
                <label>Pedal Function</label>
                <select id="exp_func" onchange="updExp('cc', this.value)">
                    <option value="11">Expression (CC 11)</option>
                    <option value="7">Volume (CC 7)</option>
                    <option value="1">Modulation (CC 1)</option>
                    <option value="74">Filter Cutoff (CC 74)</option>
                    </select>
            </div>
        </div>

        <div style="margin-bottom:15px;">
             <label>Response Curve 

[Image of sigmoid function]
</label>
             <select id="exp_curve" onchange="updExp('crv', this.value)">
                 <option value="0">Linear (Standard)</option>
                 <option value="1">Exponential (Slow Start / Swell)</option>
                 <option value="2">Logarithmic (Fast Start)</option>
             </select>
        </div>
        
        <div style="background: #252525; padding: 10px; border-radius: 5px;">
            <div style="display:flex; justify-content:space-between; margin-bottom:5px;">
                <label style="color:#aaa;">Calibration (This Bank)</label>
                <label style="color:#e74c3c;">Live Raw: <span id="exp_live_val" style="font-weight:bold;">---</span></label>
            </div>
            <div class='wifi-grid'>
                <div>
                    <label>Heel (Min)</label>
                    <input type='number' id='exp_min' onchange="updExp('min', this.value)">
                    <button class="btn-cal" onclick="setExpMin()">Set to Current</button>
                </div>
                <div>
                    <label>Toe (Max)</label>
                    <input type='number' id='exp_max' onchange="updExp('max', this.value)">
                    <button class="btn-cal" onclick="setExpMax()">Set to Current</button>
                </div>
            </div>
        </div>
    </div>

    <div class="control-box">
        <label style="font-size: 1em; color: white;">Battery Status</label>
        <span id="bat_val" style="font-weight: bold; color: #00d1b2; font-size: 1.2em;">-- V</span>
    </div>

    <div class="control-box">
        <label style="font-size: 1em; color: white; white-space: nowrap;">LED Brightness</label>
        <input type="range" id="bri_slider" min="1" max="255" oninput="updateBri(this.value)">
        <span id="bri_val" style="font-weight: bold; width: 40px; text-align: right;">127</span>
    </div>

    <div class="bank-bar">
        <button id="btn-b0" class="bank-btn" onclick="userSelBank(0)">Bank 1</button>
        <button id="btn-b1" class="bank-btn" onclick="userSelBank(1)">Bank 2</button>
        <button id="btn-b2" class="bank-btn" onclick="userSelBank(2)">Bank 3</button>
        <button id="btn-b3" class="bank-btn" onclick="userSelBank(3)">Bank 4</button>
    </div>

    <div id='sws'></div>
    <button class='btn-main' onclick='save()'>Save All Configuration</button>

<script>
    const types = { 144: 'Note On', 128: 'Note Off', 176: 'CC', 192: 'PC', 250: 'Bank Cycle Rev', 251: 'Bank Cycle Fwd' };
    const bankColors = ['#FF0000', '#00FF00', '#0055FF', '#FF00FF'];
    const textColors = ['#FFFFFF', '#000000', '#FFFFFF', '#FFFFFF'];

    let fullData = null; 
    let curBank = 0;
    let liveExpVal = 0; 

    // 1. GENERATOR FOR SHORT PRESS (Includes Banks)
    function genMainTypes(val) {
        const opts = [
            {v:0, t:"None"},
            {v:144, t:"Note On"},
            {v:128, t:"Note Off"},
            {v:176, t:"CC"},
            {v:192, t:"PC"},
            {v:251, t:"Bank Fwd"},
            {v:250, t:"Bank Rev"},
            {v:252, t:"Bank 1"},
            {v:253, t:"Bank 2"},
            {v:254, t:"Bank 3"},
            {v:255, t:"Bank 4"}
        ];
        let h = "";
        opts.forEach(o => {
            h += `<option value='${o.v}' ${val==o.v?"selected":""}>${o.t}</option>`;
        });
        return h;
    }

    // 2. GENERATOR FOR LP/RELEASE (No Banks)
    function genSecTypes(val) {
        const opts = [
            {v:0, t:"None"},
            {v:144, t:"Note On"},
            {v:128, t:"Note Off"},
            {v:176, t:"CC"},
            {v:192, t:"PC"}
        ];
        let h = "";
        opts.forEach(o => {
            h += `<option value='${o.v}' ${val==o.v?"selected":""}>${o.t}</option>`;
        });
        return h;
    }

    // --- BITMASK HELPERS ---
    function fromMask(mask) {
        let grps = [];
        for(let i=0; i<8; i++) {
            if((mask >> i) & 1) grps.push(i+1);
        }
        return grps.join(', ');
    }
    
    function toMask(str) {
        let mask = 0;
        if(!str) return 0;
        str.toString().split(',').forEach(s => {
            let v = parseInt(s.trim());
            if(!isNaN(v) && v >= 1 && v <= 8) mask |= (1 << (v-1));
        });
        return mask;
    }

    async function load() {
        try {
            const r = await fetch('/api/settings');
            fullData = await r.json();
            document.getElementById('ssid').value = fullData.wifi.ssid;
            document.getElementById('pass').value = fullData.wifi.pass;
            
            const bri = fullData.brightness || 127;
            document.getElementById('bri_slider').value = bri;
            document.getElementById('bri_val').innerText = bri;

            if(fullData.exp) {
                document.getElementById('exp_chan').value = (fullData.exp.chan || 0) + 1;
                document.getElementById('exp_func').value = fullData.exp.cc || 11;
                document.getElementById('exp_min').value = fullData.exp.min || 100;
                document.getElementById('exp_max').value = fullData.exp.max || 4000;
            }

            render();
            updateBankClasses();
            setInterval(pollStatus, 800); 
        } catch (e) { console.error("Load failed", e); }
    }

    function updateBri(val) {
        document.getElementById('bri_val').innerText = val;
        if(fullData) fullData.brightness = parseInt(val);
    }

    // FIXED: Now updates both the Input Box AND the Data Model
    function setExpMin() { 
        const val = liveExpVal;
        document.getElementById('exp_min').value = val; 
        updExp('min', val); 
    }

    function setExpMax() { 
        const val = liveExpVal;
        document.getElementById('exp_max').value = val; 
        updExp('max', val); 
    }

    async function pollStatus() {
        if (document.activeElement.tagName === "INPUT" && document.activeElement.id !== "exp_chan") return; 
        try {
            const r = await fetch('/api/status');
            if(r.ok) {
                const d = await r.json();
                if(d.bank !== curBank) {
                    curBank = d.bank;
                    updateBankClasses();
                    render();
                }
                if(d.bat !== undefined) {
                    document.getElementById('bat_val').innerText = d.bat.toFixed(2) + " V";
                    if (d.bat > 3.8) document.getElementById('bat_val').style.color = "#2ecc71"; 
                    else if (d.bat > 3.5) document.getElementById('bat_val').style.color = "#f1c40f"; 
                    else document.getElementById('bat_val').style.color = "#e74c3c"; 
                }
                if(d.exp_raw !== undefined) {
                    liveExpVal = d.exp_raw;
                    document.getElementById('exp_live_val').innerText = liveExpVal;
                }
            }
        } catch(e) {}
    }

    async function userSelBank(b) {
        curBank = b;
        updateBankClasses();
        await fetch('/api/set_bank', { method: 'POST', body: b.toString() }); 
        render();
    }

    function updateBankClasses() {
        for(let i=0; i<4; i++) {
            const btn = document.getElementById('btn-b'+i);
            if(i === curBank) {
                btn.style.background = bankColors[i];
                btn.style.color = textColors[i];
                btn.style.borderBottom = '4px solid #fff';
                btn.style.boxShadow = `0 0 15px ${bankColors[i]}`;
                btn.style.transform = "scale(1.05)";
            } else {
                btn.style.background = '#333';
                btn.style.color = '#aaa';
                btn.style.borderBottom = `4px solid ${bankColors[i]}`;
                btn.style.boxShadow = 'none';
                btn.style.transform = "scale(1)";
            }
        }
    }
    window.updGlob = function(key, val) {
        if(!fullData) return;
        // Check if the key is the sleep boolean
        if (key === 'ds_en') fullData.ds_en = val; // Boolean is passed directly
        else if (key === 'ds_min') fullData.ds_min = parseInt(val);
        else fullData[key] = parseInt(val); // Standard int handling for others
    }
    // Helper to update Expression settings in the JSON
    window.updExp = function(key, val) {
        if(!fullData) return;
        let v = parseInt(val);
        if(key === 'ch') v = v - 1; // 0-indexed
        fullData.banks[curBank].exp[key] = v;
    }

    window.upd = function(swIdx, cat, valIdx, val) {
        if(!fullData) return;
        let v = parseInt(val);
        if (valIdx === 1) v = v - 1; 
        fullData.banks[curBank].switches[swIdx][cat][valIdx] = v;
        if(cat === 'p' && valIdx === 0) render(); 
    }
    
    window.updBool = function(swIdx, key, checked) {
        if(!fullData) return;
        fullData.banks[curBank].switches[swIdx][key] = checked;
        render(); 
    }
    
    // Updated updVal to handle keys correctly
    window.updVal = function(swIdx, key, val) {
        if(!fullData) return;
        // Check for ALL mask keys
        if(['incl', 'pe', 'pm', 'lpe', 'lpm', 'le', 'lm'].includes(key)) {
            fullData.banks[curBank].switches[swIdx][key] = toMask(val);
        } else {
            fullData.banks[curBank].switches[swIdx][key] = parseInt(val);
        }
    }

    function render() {
        if(!fullData) return;
        const bank = fullData.banks[curBank];
        
        // --- 1. RENDER EXPRESSION CARD (Per Bank) ---
        // Ensure exp object exists in JSON
        if (!bank.exp) bank.exp = {ch:0, cc:11, min:0, max:4095, crv:0};
        
        document.getElementById('exp_chan').value = bank.exp.ch + 1;
        document.getElementById('exp_func').value = bank.exp.cc;
        document.getElementById('exp_min').value = bank.exp.min;
        document.getElementById('exp_max').value = bank.exp.max;
        document.getElementById('exp_curve').value = bank.exp.crv || 0; // Curve Dropdown
        document.getElementById('ds_en').checked = fullData.ds_en;
        document.getElementById('ds_min').value = fullData.ds_min;

        // --- 2. RENDER SWITCHES ---
        let html = '';
        bank.switches.forEach((s, i) => {
            const isBank = (s.p[0] >= 250);
            const disClass = isBank ? "disabled" : ""; 
            const disAttr = isBank ? "disabled" : "";
            const inclText = (s.incl !== undefined) ? fromMask(s.incl) : ""; 
            const togEnabled = (s.tog !== undefined) ? s.tog : false; 
            const edgeVal = (s.edge !== undefined) ? s.edge : 0; 
            const openLp = (!isBank && (s.lp[0] !== 0 || s.lp_en)) ? "open" : "";
            const openRel = (!isBank && (s.l[0] !== 0)) ? "open" : "";

            const mkInputs = (type, ch, val, ex, lead, k_type, k_ex, k_lead) => {
                // MODIFIED: Only hide MIDI Channel/Value if Type is 0. 
                // Group box is NOT hidden.
                const hideMidi = (type === 0) ? "hidden-input" : "";
                const hideExcl = (k_type === 'l') ? "visibility:hidden;" : "";
                
                // Adjust Grid Columns: If type=0, we only show Select + Groups (2 cols). Else 3 cols.
                const gridStyle = (type === 0) ? "grid-template-columns: 1.5fr 1fr;" : "grid-template-columns: 1.5fr 1fr 1fr;";

                return `
                <div class='input-group' style="${gridStyle}">
                    <select onchange="upd(${i},'${k_type}',0,this.value)">
                        ${(k_type=='p'?genMainTypes(type):genSecTypes(type))}
                    </select>
                    
                    <input class="${hideMidi} ${disClass}" ${disAttr} type='number' value='${ch + 1}' onchange="upd(${i},'${k_type}',1,this.value)" min='1' max='16' title="Channel">
                    <input class="${hideMidi} ${disClass}" ${disAttr} type='number' value='${val}' onchange="upd(${i},'${k_type}',2,this.value)" min='0' max='127' title="Value">
                    
                    <div class="" style="display:flex; align-items:center; gap:2px; margin-left:5px; border-left:1px solid #444; padding-left:5px;">
                        <input class="${disClass}" ${disAttr} type="text" placeholder="Ex" title="Exclusive Mask (🛡️)" style="width:30px; border-color:#e74c3c; ${hideExcl}" value="${fromMask(ex)}" onchange="updVal(${i}, '${k_ex}', this.value)">
                        <input class="${disClass}" ${disAttr} type="text" placeholder="Ld" title="Lead/Master Mask (⚡)" style="width:30px; border-color:#f1c40f;" value="${fromMask(lead)}" onchange="updVal(${i}, '${k_lead}', this.value)">
                    </div>
                </div>`;
            };

            html += `
            <div class='sw'>
                <div style="display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #333; padding-bottom:10px; margin-bottom:10px;">
                    <h3 style="margin:0; border:none; font-size:1em;">SWITCH ${i+1}</h3>
                    <div style="display:flex; align-items:center; gap:10px;">
                        <div style="display:flex; align-items:center; gap:4px;" title="Groups this switch belongs to (Slave)">
                            <label style="font-size:1.2em; margin:0;">🔗</label>
                            <input class="${disClass}" ${disAttr} type="text" style="width:40px; border:1px solid #2ecc71; text-align:center;" value="${inclText}" onchange="updVal(${i}, 'incl', this.value)">
                        </div>
                        <div style="display:flex; align-items:center; background:#252525; padding:2px 6px; border-radius:4px;">
                            <label style="font-size:0.7em; margin-right:4px; font-weight:bold;">TOGGLE</label>
                            <input class="${disClass}" ${disAttr} type="checkbox" ${togEnabled ? "checked" : ""} onchange="updBool(${i}, 'tog', this.checked)">
                        </div>
                    </div>
                </div>
                <div class='grid-section'>
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:5px;">
                        <label style="color:#00d1b2; font-weight:bold;">Short Press</label>
                        <select class="${disClass}" ${disAttr} style="width:auto; padding:0 5px; font-size:0.7em; height:20px;" onchange="updVal(${i}, 'edge', this.value)">
                            <option value="0" ${edgeVal==0?"selected":""}>Trig: Press</option>
                            <option value="1" ${edgeVal==1?"selected":""}>Trig: Release</option>
                        </select>
                    </div>
                    ${mkInputs(s.p[0], s.p[1], s.p[2], s.pe, s.pm, 'p', 'pe', 'pm')}
                </div>
                <details ${openLp} class="${disClass}" ${disAttr}>
                    <summary>Long Press Options</summary>
                    <div style="padding-top:5px;">
                        <div class="label-row">
                            <label>Enable Long Press</label>
                            <input type="checkbox" ${s.lp_en ? "checked" : ""} onchange="updBool(${i}, 'lp_en', this.checked)">
                        </div>
                        <div style="${s.lp_en ? '' : 'opacity:0.5; pointer-events:none;'}">
                            ${mkInputs(s.lp[0], s.lp[1], s.lp[2], s.lpe, s.lpm, 'lp', 'lpe', 'lpm')}
                        </div>
                    </div>
                </details>
                <details ${openRel} class="${disClass}" ${disAttr}>
                    <summary>Release / Off Options</summary>
                    <div style="padding-top:5px;">
                        ${mkInputs(s.l[0], s.l[1], s.l[2], s.le, s.lm, 'l', 'le', 'lm')}
                    </div>
                </details>
            </div>`;
        });
        document.getElementById('sws').innerHTML = html;
    }

    async function save() {
        const btn = document.querySelector('.btn-main');
        const oldText = btn.innerText;
        btn.innerText = "Saving...";
        btn.disabled = true;

        if(!fullData.exp) fullData.exp = {};
        fullData.exp.chan = parseInt(document.getElementById('exp_chan').value) - 1;
        fullData.exp.cc = parseInt(document.getElementById('exp_func').value);
        fullData.exp.min = parseInt(document.getElementById('exp_min').value);
        fullData.exp.max = parseInt(document.getElementById('exp_max').value);

        try {
            const r = await fetch('/api/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(fullData)
            });
            if(r.ok) {
                btn.style.background = "#2ecc71";
                btn.innerText = "Saved Successfully!";
                setTimeout(() => {
                    btn.style.background = "#00d1b2";
                    btn.innerText = oldText;
                    btn.disabled = false;
                }, 2000);
            }
        } catch (e) { 
            alert('Save error'); 
            btn.disabled = false;
            btn.innerText = oldText;
        }
    }

    async function scan() {
        const btn = document.getElementById('scan-btn');
        btn.innerText = 'Scanning...';
        try {
            const r = await fetch('/api/scan');
            const ssids = await r.json();
            const sel = document.createElement('select'); sel.id = 'ssid';
            [...new Set(ssids)].filter(s=>s).forEach(s => {
                const opt = document.createElement('option'); opt.value=s; opt.innerText=s; sel.appendChild(opt);
            });
            document.getElementById('ssid-container').innerHTML = '<label>SSID</label>';
            document.getElementById('ssid-container').appendChild(sel);
        } catch (e) { alert("Scan failed"); }
        btn.innerText = 'Scan WiFi';
    }

    async function saveWifi() {
        const ssid = document.getElementById('ssid').value;
        const pass = document.getElementById('pass').value;
        if(!ssid) return alert("SSID required");
        if(confirm("Save WiFi and Reboot?")) {
            await fetch('/api/save_wifi', {
                method: 'POST', headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid, pass})
            });
            alert("Settings saved. Device is rebooting...");
        }
    }

    load();
</script></body></html>
)=====";

#endif
