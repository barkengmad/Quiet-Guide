#include "webserver.h"
#include "storage.h"
#include "network.h"
#include "session.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include "rtc_time.h"

WiFiServer server(80);
bool webServerRunning = false;

String formatDuration(int seconds) {
    if (seconds < 60) {
        return String(seconds) + "s";
    } else {
        int minutes = seconds / 60;
        int remainingSeconds = seconds % 60;
        if (remainingSeconds == 0) {
            return String(minutes) + "m";
        } else {
            return String(minutes) + "m " + String(remainingSeconds) + "s";
        }
    }
}

String getStatusString() {
    SessionState state = getCurrentState();
    switch (state) {
        case SessionState::IDLE: return "Ready - Device is idle";
        case SessionState::DEEP_BREATHING: return "Active - Deep breathing phase";
        case SessionState::BREATH_HOLD: return "Active - Breath hold phase"; 
        case SessionState::RECOVERY: return "Active - Recovery phase";
        case SessionState::SILENT: return "Active - Silent meditation";
        case SessionState::CUSTOM_RUNNING: return "Active - Custom pattern running";
        case SessionState::DYNAMIC_TEACHING: return "Active - Dynamic cadence: teaching";
        case SessionState::DYNAMIC_GUIDED: return "Active - Dynamic cadence: guided";
        case SessionState::BOOTING: return "Starting up...";
    }
    return "Unknown";
}

String getTrainingDescription() {
    SessionState state = getCurrentState();
    AppConfig config = loadConfig();
    switch (state) {
        case SessionState::IDLE:
            return "<h3>🏠 IDLE - Ready to Begin</h3>"
                   "<p><strong>What's happening:</strong> The device is ready for meditation. Use short button presses to select your desired number of rounds (1-" + String(config.maxRounds) + ").</p>"
                   "<p><strong>Why it matters:</strong> Choosing the right number of rounds helps build your breath-holding capacity gradually. Start with fewer rounds and increase as you improve.</p>"
                   "<p><strong>Next phase:</strong> Long press the button to start your breathing session with the selected rounds.</p>";
                   
        case SessionState::DEEP_BREATHING:
            return "<h3>🫁 DEEP BREATHING - Round " + String(getCurrentSessionRound()) + " of " + String(getTotalRounds()) + "</h3>"
                   "<p><strong>What's happening:</strong> Take deep, controlled breaths to saturate your blood with oxygen. Breathe in through your nose, out through your mouth.</p>"
                   "<p><strong>Why it matters:</strong> This hyperventilation phase increases oxygen levels and decreases CO2, preparing your body for the breath hold. It triggers physiological changes that improve breath-holding capacity.</p>"
                   "<p><strong>Next phase:</strong> This will automatically proceed after " + String(config.deepBreathingSeconds) + "s with a long vibration, or when you feel fully oxygenated (tingling, slight dizziness is normal), short press. <strong>When proceeding: breathe out completely and hold your breath</strong> - this starts the breath hold phase immediately.</p>";
                   
        case SessionState::BREATH_HOLD:
            return "<h3>🛑 BREATH HOLD - Round " + String(getCurrentSessionRound()) + " of " + String(getTotalRounds()) + "</h3>"
                   "<p><strong>What's happening:</strong> You are now holding your breath after exhaling completely. Stay relaxed, don't force it. Your body will signal when it's time to breathe.</p>"
                   "<p><strong>Why it matters:</strong> This activates your mammalian dive reflex, trains CO2 tolerance, and builds mental resilience. It's where the real benefits of the Wim Hof method occur.</p>"
                   "<p><strong>Next phase:</strong> Trust your body - it will tell you when it's time to breathe. When you feel the urge, try to hold for a few seconds more, then take a deep breath in, hold it for 10-15 seconds, and short press to start the recovery phase. Everyone is different, and with practice you'll be able to hold for longer.</p>";
                   
        case SessionState::RECOVERY: {
            int currentRound = getCurrentSessionRound();
            int totalRounds = getTotalRounds();
            String nextPhaseInfo;
            
            if (currentRound < totalRounds) {
                nextPhaseInfo = "This will automatically proceed after " + String(config.recoverySeconds) + "s with a long vibration, or short press when ready. <strong>Next: Round " + String(currentRound + 1) + " will start with " + String(currentRound + 1) + " short buzzes</strong> to indicate the round number.";
            } else {
                nextPhaseInfo = "This will automatically proceed after " + String(config.recoverySeconds) + "s with a long vibration, or short press when ready. <strong>Next: Silent meditation will start with one long buzz</strong> to indicate the final phase.";
            }
            
            return "<h3>💨 RECOVERY - Round " + String(currentRound) + " of " + String(totalRounds) + "</h3>"
                   "<p><strong>What's happening:</strong> You are holding a deep recovery breath for 10-15 seconds. When ready, exhale slowly and relax.</p>"
                   "<p><strong>Why it matters:</strong> This phase helps integrate the physiological changes from the breath hold and prepares you for the next round (or silent phase if finished).</p>"
                   "<p><strong>Next phase:</strong> " + nextPhaseInfo + "</p>";
        }
                   
        case SessionState::SILENT:
            return "<h3>🧘 SILENT MEDITATION - Final Phase</h3>"
                   "<p><strong>What's happening:</strong> Enjoy the heightened state of awareness after completing " + String(getTotalRounds()) + " breathing rounds. Meditate in silence, observing your inner experience.</p>"
                   "<p><strong>Why it matters:</strong> This phase allows you to experience the full benefits of the practice - increased focus, calmness, and bodily awareness that follows the breathing technique.</p>"
                   "<p><strong>Next phase:</strong> Stay as long as feels right (maximum " + String(config.silentPhaseMaxMinutes) + " minutes), or short press when ready to end your session and return to idle.</p>";
                   
        case SessionState::BOOTING:
            return "<h3>⚡ STARTING UP - System Initialization</h3>"
                   "<p><strong>What's happening:</strong> The device is initializing its systems, connecting to WiFi, and preparing for your meditation session.</p>"
                   "<p><strong>Why it matters:</strong> All systems need to be ready to provide accurate timing and feedback during your practice.</p>"
                   "<p><strong>Next phase:</strong> Once initialization is complete, you'll enter the idle state where you can select your rounds.</p>";
    }
    return "<p>Unknown state - please check device status.</p>";
}

// ---- Moon cycle helpers ----
static const double SYNODIC_PERIOD_DAYS = 29.530588; // Average lunar synodic month

// Reference new moon: 2000-01-06 18:14:00 (local time reference)
time_t getReferenceNewMoon() {
    struct tm t = {0};
    t.tm_year = 2000 - 1900;
    t.tm_mon = 0;   // Jan
    t.tm_mday = 6;
    t.tm_hour = 18;
    t.tm_min = 14;
    t.tm_sec = 0;
    return mktime(&t);
}

// Normalize to local midnight
time_t toLocalMidnight(time_t ts) {
    struct tm * ti = localtime(&ts);
    struct tm m = *ti;
    m.tm_hour = 0; m.tm_min = 0; m.tm_sec = 0;
    return mktime(&m);
}

String formatDateYMD(time_t ts) {
    char buf[11];
    struct tm * ti = localtime(&ts);
    strftime(buf, sizeof(buf), "%Y-%m-%d", ti);
    return String(buf);
}

void getCurrentMoonCycle(time_t now, time_t &cycleStart, time_t &cycleEnd) {
    const time_t ref = getReferenceNewMoon();
    const double periodSec = SYNODIC_PERIOD_DAYS * 86400.0;
    double elapsed = difftime(now, ref);
    long k = (long)floor(elapsed / periodSec);
    cycleStart = ref + (time_t)(k * periodSec);
    cycleStart = toLocalMidnight(cycleStart);
    cycleEnd = cycleStart + (time_t)ceil(SYNODIC_PERIOD_DAYS) * 86400; // show whole days
}

String generateHTML(String title, String content) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    html += "h1{color:#333;text-align:center;margin-bottom:20px}";
    html += ".nav{display:flex;flex-wrap:wrap;gap:10px;justify-content:center;margin-bottom:20px}";
    html += ".nav a{padding:8px 15px;background:#007bff;color:white;text-decoration:none;border-radius:5px}";
    html += ".nav a:hover{background:#0056b3}";
    html += ".status{padding:10px;margin:10px 0;background:#d4edda;color:#155724;border-radius:5px;text-align:center}";
    html += ".training-mode{background:#e7f3ff;color:#004085;text-align:left;padding:15px}";
    html += ".form-group{margin-bottom:15px}";
    html += "label{display:block;margin-bottom:5px;font-weight:bold;color:#555}";
    html += "input,select{width:100%;padding:8px;border:2px solid #ddd;border-radius:5px;font-size:14px}";
    html += "button{background:#28a745;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;width:100%}";
    html += "button:hover{background:#218838}";
    html += ".logs{background:#f8f9fa;padding:10px;margin:10px 0;border-radius:5px;font-family:monospace;font-size:12px}";
    html += ".delete-btn{background:#dc3545;color:white;padding:8px 15px;border:none;border-radius:5px;cursor:pointer;font-size:12px;margin-top:10px;width:120px;text-align:center}";
    html += ".delete-btn:hover{background:#c82333}";
    html += ".delete-all-btn{background:#dc3545;color:white;padding:8px 15px;border:none;border-radius:5px;cursor:pointer;font-size:14px;margin-left:10px;width:150px}";
    html += ".delete-all-btn:hover{background:#c82333}";
    html += ".button-group{text-align:center;margin:20px 0}";
    html += ".download-btn{background:#28a745;color:white;padding:8px 15px;text-decoration:none;border-radius:5px;width:150px;display:inline-block;text-align:center}";
    html += "</style>";
    html += "<script>";
    html += "function confirmDeleteAll(){return confirm('Are you sure you want to delete ALL session logs? This cannot be undone!');}";
    html += "function confirmDelete(index){return confirm('Are you sure you want to delete this session log?');}";
    html += "let trainingMode = localStorage.getItem('trainingMode') === 'true';";
    html += "let statusInterval;";
    html += "function toggleTrainingMode(){";
    html += "  trainingMode = !trainingMode;";
    html += "  localStorage.setItem('trainingMode', trainingMode);";
    html += "  const btn = document.getElementById('trainingBtn');";
    html += "  if(trainingMode){";
    html += "    btn.innerText = 'Disable Wim Hof Training';";
    html += "    btn.style.background = '#dc3545';";
    html += "    updateStatus();";
    html += "    startStatusPolling();";
    html += "  } else {";
    html += "    btn.innerText = 'Enable Wim Hof Training';";
    html += "    btn.style.background = '#28a745';";
    html += "    updateStatus();";
    html += "    stopStatusPolling();";
    html += "  }";
    html += "}";
    html += "function startStatusPolling(){";
    html += "  if(statusInterval) clearInterval(statusInterval);";
    html += "  statusInterval = setInterval(updateStatus, 1000);";
    html += "}";
    html += "function stopStatusPolling(){";
    html += "  if(statusInterval) clearInterval(statusInterval);";
    html += "}";
    html += "function updateStatus(){";
    html += "  fetch('/status').then(r=>r.json()).then(data=>{";
    html += "    const statusDiv = document.getElementById('statusDiv');";
    html += "    if(trainingMode){";
    html += "      statusDiv.innerHTML = data.trainingDescription;";
    html += "      statusDiv.className = 'status training-mode';";
    html += "    } else {";
    html += "      statusDiv.innerHTML = data.status;";
    html += "      statusDiv.className = 'status';";
    html += "    }";
    html += "    var cr = document.getElementById('currentRounds'); if (cr) cr.innerText = data.currentRound;";
    html += "    var pl = document.getElementById('patternLabel'); if (pl) pl.innerText = data.patternLabel;";
    html += "    var ps = document.getElementById('patternSettings'); if (ps) ps.innerHTML = data.patternSettingsHtml;";
    html += "  }).catch(e=>console.log('Status update failed'));";
    html += "}";
    html += "window.onload = function(){";
    html += "  const btn = document.getElementById('trainingBtn');";
    html += "  if(btn){";
    html += "    if(trainingMode){";
    html += "      btn.innerText = 'Disable Wim Hof Training';";
    html += "      btn.style.background = '#dc3545';";
    html += "      updateStatus();";
    html += "      startStatusPolling();";
    html += "    } else {";
    html += "      btn.innerText = 'Enable Wim Hof Training';";
    html += "      btn.style.background = '#28a745';";
    html += "    }";
    html += "  }";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>🧘 Meditation Timer</h1>";
    html += "<div class='nav'>";
    html += "<a href='/'>Dashboard</a>";
    html += "<a href='/guide'>Guide</a>";
    if (isHotspotMode()) {
        html += "<a href='/wifi-setup' style='background:#dc3545;'>WiFi Setup</a>";
    } else {
        html += "<a href='/wifi-setup'>WiFi</a>";
    }
    html += "<a href='/config'>Settings</a>";
    html += "<a href='/moon'>Moon</a>";
    html += "<a href='/logs'>Logs</a>";
    html += "</div>";
    html += content;
    html += "</div></body></html>";
    return html;
}

void handleClient(WiFiClient& client) {
    String request = client.readStringUntil('\r');
    
    // Don't flush for POST requests - we need the body data!
    bool isPostRequest = request.indexOf("POST") >= 0;
    if (!isPostRequest) {
        client.flush();
    }
    
    Serial.println("Web request: " + request);
    
    String response = "";
    
    if (request.indexOf("GET / ") >= 0 || request.indexOf("GET /?") >= 0) {
        // Dashboard
        Serial.println("Serving dashboard page");
        AppConfig config = loadConfig();
        String content = "<div id='statusDiv' class='status'>" + getStatusString() + "</div>";
        content += "<div class='button-group'>";
        content += "<button id='trainingBtn' onclick='toggleTrainingMode()' style='background:#28a745;color:white;padding:8px 15px;border:none;border-radius:5px;cursor:pointer;'>Enable Wim Hof Training</button>";
        content += "</div>";
        // Current Pattern overview
        auto patternLabelFn = [](int id) -> String {
            switch (id) {
                case 1: return "[1] Wim Hof";
                case 2: return "[2] Box";
                case 3: return "[3] 4-7-8";
                case 4: return "[4] Resonant (6:6)";
            }
            return String("[") + String(id) + "] Unknown";
        };
        // Pattern configuration block will be appended below patterns list

        // Breathing Patterns (selection + inclusion + drag order)
        content += "<h3>Breathing Patterns</h3>";
        content += "<form id='patternForm' method='POST' action='/save'>";
        content += "<div style='background:#f8f9fa;border-radius:6px;padding:8px 10px;margin-bottom:16px'>";
        content += "<p style='margin:6px 0;color:#555;font-size:13px'>Drag to reorder. Select one as current. Tick to include in rotation. Choose whether to include a silent phase at the end.</p>";
        content += "<ul id='patternList' style='list-style:none;padding:0;margin:0'>";
        auto labelForIdDash = [](int id)->String{
            switch(id){case 1:return "[1] Wim Hof";case 2:return "[2] Box";case 3:return "[3] 4\u00B7 7\u00B7 8";case 4:return "[4] Resonant (6:6)";case 5:return "[5] Custom";case 6:return "[6] Dynamic";}return String("[")+String(id)+"]";};
        auto includeForIdDash = [&](int id)->bool{
            if(id==1) return config.includeWimHof; if(id==2) return config.includeBox; if(id==3) return config.include478; if(id==4) return config.includeResonant; if(id==5) return config.includeCustom; return config.includeDynamic; };
        auto silentForIdDash = [&](int id)->bool{
            if(id==1) return config.silentAfterWimHof; if(id==2) return config.silentAfterBox; if(id==3) return config.silentAfter478; if(id==4) return config.silentAfterResonant; if(id==5) return config.silentAfterCustom; return config.silentAfterDynamic; };
        for (int i=0;i<6;i++){
            int id = config.patternOrder[i]; if (id<1||id>6) id=i+1;
            String li = "<li draggable='true' data-id='" + String(id) + "' style='display:flex;align-items:center;gap:8px;border:1px solid #ddd;background:#fff;border-radius:6px;padding:8px;margin:6px 0'>";
            // Radio + label left aligned
            li += "<label style='display:flex;align-items:center;gap:8px;flex:1;margin:0'><input type='radio' name='currentPatternId' value='" + String(id) + "'" + (config.currentPatternId==id?" checked":"") + "> <span>" + labelForIdDash(id) + "</span></label>";
            // Drag handle moved after label
            li += "<span style='cursor:grab'>↕</span>";
            const char* key = id==1?"includeWimHof":id==2?"includeBox":id==3?"include478":id==4?"includeResonant":id==5?"includeCustom":"includeDynamic";
            li += "<label style='display:flex;align-items:center;gap:6px'><input type='checkbox' name='" + String(key) + "' value='1'" + (includeForIdDash(id)?" checked":"") + "> Include</label>";
            const char* silentKey = id==1?"silentAfterWimHof":id==2?"silentAfterBox":id==3?"silentAfter478":id==4?"silentAfterResonant":id==5?"silentAfterCustom":"silentAfterDynamic";
            li += "<label style='display:flex;align-items:center;gap:6px'><input type='checkbox' name='" + String(silentKey) + "' value='1'" + (silentForIdDash(id)?" checked":"") + "> Silent at end</label>";
            li += "</li>";
            content += li;
        }
        content += "</ul>";
        content += "<input type='hidden' id='patternOrder' name='patternOrder' value='";
        for(int i=0;i<6;i++){content += String(config.patternOrder[i]); if(i<5) content += ",";}
        content += "'>";
        content += "<input type='hidden' name='returnTo' value='/'/>";
        content += "<button id='patternSaveBtn' type='submit' style='margin-top:10px'>Save Patterns</button>";
        content += "</div>";
        content += "</form>";
        // DnD script
        content += "<script>(function(){const list=document.getElementById('patternList');const btn=document.getElementById('patternSaveBtn');const form=document.getElementById('patternForm');if(!list||!btn||!form)return;let dragEl=null;list.addEventListener('dragstart',e=>{dragEl=e.target.closest('li');if(e.dataTransfer){e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain','');}});list.addEventListener('dragover',e=>{e.preventDefault();const li=e.target.closest('li');if(!li||li===dragEl) return;const rect=li.getBoundingClientRect();const next=(e.clientY-rect.top)/(rect.bottom-rect.top)>0.5;list.insertBefore(dragEl, next? li.nextSibling: li);});list.addEventListener('drop',e=>{e.preventDefault();updateOrder();});list.addEventListener('dragend',e=>{updateOrder();});function updateOrder(){const ids=[...list.querySelectorAll('li')].map(li=>li.dataset.id);const input=document.getElementById('patternOrder');if(input) input.value=ids.join(',');setDirty();}function setDirty(){btn.textContent='Save Patterns';}form.querySelectorAll('input').forEach(el=>el.addEventListener('change',setDirty));if(location.search.indexOf('saved=1')>=0){btn.textContent='Saved Successfully!';}})();</script>";

        // Pattern configuration shown below the list
        content += "<div id='patternSettings' style='background:#f8f9fa;padding:10px;border-radius:6px;margin:12px 0'>";
        if (config.currentPatternId == 1) {
            content += "<p><strong>Selected Rounds:</strong> <span id='currentRounds'>" + String(config.currentRound) + "</span></p>";
            content += "<p><strong>Max Rounds:</strong> " + String(config.maxRounds) + "</p>";
            content += "<p><strong>Deep Breathing:</strong> " + String(config.deepBreathingSeconds) + "s</p>";
            content += "<p><strong>Recovery:</strong> " + String(config.recoverySeconds) + "s</p>";
        } else if (config.currentPatternId == 2) {
            content += "<p><strong>Box Seconds:</strong> " + String(config.boxSeconds) + "s</p>";
        } else if (config.currentPatternId == 3) {
            content += "<p>No configurable settings yet for 4-7-8.</p>";
        } else if (config.currentPatternId == 4) {
            content += "<p>No configurable settings yet for Resonant.</p>";
        } else if (config.currentPatternId == 5) {
            content += "<p><strong>Custom Pattern:</strong> Inhale=" + String(config.customInhaleSeconds) + "s, HoldIn=" + String(config.customHoldInSeconds) + "s, Exhale=" + String(config.customExhaleSeconds) + "s, HoldOut=" + String(config.customHoldOutSeconds) + "s</p>";
        } else if (config.currentPatternId == 6) {
            content += "<p><strong>Dynamic:</strong> Tap to teach inhale/exhale cadence; device guides at learned rhythm.</p>";
        }
        // Silent Phase common settings
        content += "<hr style='border:none;border-top:1px solid #e0e0e0;margin:10px 0'>";
        content += "<h4 style='margin:8px 0'>Silent Phase</h4>";
        content += "<p><strong>Silent Max:</strong> " + String(config.silentPhaseMaxMinutes) + " min</p>";
        content += "<p><strong>Silent Reminders:</strong> " + String(config.silentReminderEnabled ? "On" : "Off") + "</p>";
        content += "<p><strong>Reminder Interval:</strong> " + String(config.silentReminderIntervalMinutes) + " min</p>";
        content += "</div>";

        // Device Settings
        content += "<h3>Device Settings</h3>";
        content += "<p><strong>Idle Timeout:</strong> " + String(config.idleTimeoutMinutes) + " min</p>";
        content += "<p><strong>Start Confirmation Haptic:</strong> " + String(config.startConfirmationHaptics ? "On" : "Off") + "</p>";
        content += "<p><strong>Keep partial if ≥:</strong> " + String(config.abortSaveThresholdSeconds) + "s</p>";
        content += "<p><strong>Guided Breathing Duration (non‑Wim Hof):</strong> " + String(config.guidedBreathingMinutes) + " min</p>";
        // Silent Phase
        content += "<h3>Silent Phase</h3>";
        content += "<p><strong>Silent Max:</strong> " + String(config.silentPhaseMaxMinutes) + " min</p>";
        content += "<p><strong>Silent Reminders:</strong> " + String(config.silentReminderEnabled ? "On" : "Off") + "</p>";
        content += "<p><strong>Reminder Interval:</strong> " + String(config.silentReminderIntervalMinutes) + " min</p>";
        response = generateHTML("Dashboard", content);
        
    } else if (request.indexOf("GET /moon") >= 0) {
        // Moon cycle progress page
        time_t now = getEpochTime();
        time_t cycleStart, cycleEnd;
        getCurrentMoonCycle(now, cycleStart, cycleEnd);

        // Collect dates from logs within this cycle
        String logs = getSessionLogsJson();
        JsonDocument doc;
        deserializeJson(doc, logs);
        JsonArray logsArray = doc.as<JsonArray>();

        const int dayCount = (int)ceil(SYNODIC_PERIOD_DAYS); // 30 days
        bool dayHasMeditation[31];
        for (int i = 0; i < 31; ++i) dayHasMeditation[i] = false;

        // Build date strings for each day in cycle for quick compare
        String cycleDates[31];
        for (int i = 0; i < dayCount; ++i) {
            time_t d = cycleStart + i * 86400;
            cycleDates[i] = formatDateYMD(d);
        }

        for (JsonVariant v : logsArray) {
            const char* dateStr = v["date"].as<const char*>();
            if (!dateStr) continue;
            for (int i = 0; i < dayCount; ++i) {
                if (cycleDates[i].equals(dateStr)) { dayHasMeditation[i] = true; break; }
            }
        }

        int daysCompleted = 0;
        for (int i = 0; i < dayCount; ++i) if (dayHasMeditation[i]) daysCompleted++;

        String content = "<h2>🌙 Moon Cycle Progress</h2>";
        content += "<p>Goal: meditate every day in the current lunar cycle.</p>";
        content += "<div style='background:#eef7ff;padding:10px;border-radius:6px;margin:10px 0;'>";
        content += "Cycle: <strong>" + formatDateYMD(cycleStart) + "</strong> to <strong>" + formatDateYMD(cycleStart + (dayCount-1)*86400) + "</strong><br>";
        content += "Progress: <strong>" + String(daysCompleted) + "/" + String(dayCount) + " days</strong>";
        content += "</div>";

        // Circular moon-cycle visualization (circle of dots)
        const int svgSize = 260;
        const int cx = svgSize/2;
        const int cy = svgSize/2;
        const int orbitR = 100;
        const int dotR = 7;
        int todayIdx = (int)floor((double)(toLocalMidnight(now) - cycleStart) / 86400.0);
        if (todayIdx < 0) todayIdx = 0; if (todayIdx >= dayCount) todayIdx = dayCount - 1;

        // Compute phase progress and illumination
        const double periodSec = SYNODIC_PERIOD_DAYS * 86400.0;
        double rawElapsed = difftime(now, cycleStart);
        double phaseProgress = fmod(rawElapsed, periodSec) / periodSec; // 0..1
        if (phaseProgress < 0) phaseProgress += 1.0;
        double illum = 0.5 * (1.0 - cos(2.0 * M_PI * phaseProgress)); // 0=new, 1=full
        int illumPct = (int)round(illum * 100.0);
        const char* moonIcon = "🌕";
        if (phaseProgress < 1.0/8.0) moonIcon = "🌑";
        else if (phaseProgress < 2.0/8.0) moonIcon = "🌒";
        else if (phaseProgress < 3.0/8.0) moonIcon = "🌓";
        else if (phaseProgress < 4.0/8.0) moonIcon = "🌔";
        else if (phaseProgress < 5.0/8.0) moonIcon = "🌕";
        else if (phaseProgress < 6.0/8.0) moonIcon = "🌖";
        else if (phaseProgress < 7.0/8.0) moonIcon = "🌗";
        else moonIcon = "🌘";

        content += "<svg width='260' height='260' viewBox='0 0 260 260' xmlns='http://www.w3.org/2000/svg' style='display:block;margin:0 auto;'>";
        // Orbit removed per design request (no grey connecting line)
        // Dots
        for (int i = 0; i < dayCount; ++i) {
            double angle = -M_PI/2 + (2.0*M_PI * i) / (double)dayCount; // start at top
            int x = (int)round(cx + orbitR * cos(angle));
            int y = (int)round(cy + orbitR * sin(angle));
            String fill = dayHasMeditation[i] ? "#ffd54f" : "#e2e3e5";
            String stroke = (i == todayIdx) ? "#007bff" : "none";
            String sw = (i == todayIdx) ? "2" : "0";
            content += "<g>";
            content += "<circle cx='" + String(x) + "' cy='" + String(y) + "' r='" + String(dotR) + "' fill='" + fill + "' stroke='" + stroke + "' stroke-width='" + sw + "'>";
            content += "<title>" + cycleDates[i] + (dayHasMeditation[i] ? " — completed" : " — not yet") + "</title>";
            content += "</circle>";
            content += "</g>";
        }
        // Center moon emoji (large, fills center)
        content += "<text x='" + String(cx) + "' y='" + String(cy - 18) + "' text-anchor='middle' dominant-baseline='middle' font-family='Segoe UI Emoji, Apple Color Emoji, Noto Color Emoji, Arial' font-size='144'>";
        content += moonIcon;
        content += "</text>";
        content += "</svg>";

        // Legend
        content += "<div style='display:flex;gap:12px;margin-top:10px;justify-content:center;align-items:center'>";
        content += "<span style='display:inline-flex;align-items:center;gap:6px'><span style='display:inline-block;width:14px;height:14px;background:#ffd54f;border:1px solid #e0b000;border-radius:50%'></span>Completed</span>";
        content += "<span style='display:inline-flex;align-items:center;gap:6px'><span style='display:inline-block;width:14px;height:14px;background:#e2e3e5;border:1px solid #ced4da;border-radius:50%'></span>Not yet</span>";
        content += "<span style='display:inline-flex;align-items:center;gap:6px'><span style='display:inline-block;width:14px;height:14px;border:2px solid #007bff;border-radius:50%'></span>Today</span>";
        content += "</div>";

        response = generateHTML("Moon Cycle", content);

    } else if (request.indexOf("GET /guide") >= 0) {
        // Breathing Patterns Guide
        AppConfig cfg = loadConfig();
        String content = "<h2>🧭 Breathing Patterns Guide</h2>";
        // Top index with one-line descriptions and anchors
        content += "<div style='background:#eef7ff;padding:12px;border-radius:6px;margin:10px 0;'>";
        content += "<h3 style='margin-top:0'>Patterns</h3>";
        content += "<ul style='margin:0;padding-left:18px'>";
        content += "<li><a href='#wimhof'>[1] Wim Hof</a> – Cycles of deep breathing, exhale hold, and recovery to build CO₂ tolerance and energy.</li>";
        content += "<li><a href='#box'>[2] Box</a> – Equal inhale, hold, exhale, hold (" + String(cfg.boxSeconds) + "s each) to calm and focus.</li>";
        content += "<li><a href='#478'>[3] 4·7·8</a> – Inhale 4s, hold 7s, exhale 8s; promotes relaxation and downshifts the nervous system.</li>";
        content += "<li><a href='#resonant'>[4] Resonant (6:6)</a> – Inhale 6s, exhale 6s; supports HRV and balanced breathing rhythm.</li>";
        content += "<li><a href='#custom'>[5] Custom</a> – Your own timing per phase; device buzzes at each phase boundary and loops.</li>";
        content += "<li><a href='#dynamic'>[6] Dynamic</a> – Tap to teach inhale/exhale cadence; device averages and guides at that rhythm.</li>";
        content += "</ul></div>";
        
        content += "<h2 id='wimhof'>[1] Wim Hof</h2>";
        
        content += "<h3>📖 What is the Wim Hof Method?</h3>";
        content += "<p>The Wim Hof Method is a powerful breathing technique developed by Wim \"The Iceman\" Hof. It combines controlled hyperventilation, breath retention, and meditation to unlock extraordinary physical and mental benefits.</p>";
        
        content += "<h3>🧬 The Science Behind It</h3>";
        content += "<p><strong>Hyperventilation Phase:</strong> Deep, rapid breathing increases oxygen levels while decreasing CO2, creating an alkaline state in your blood.</p>";
        content += "<p><strong>Breath Hold:</strong> Triggers the mammalian dive reflex, activating your sympathetic nervous system and releasing adrenaline and noradrenaline naturally.</p>";
        content += "<p><strong>Recovery:</strong> Balances your nervous system and integrates the physiological changes.</p>";
        content += "<p><strong>Benefits:</strong> Improved immune response, reduced inflammation, increased energy, better stress resilience, and enhanced mental clarity.</p>";
        
        content += "<h3>🤖 How Your Device Helps</h3>";
        content += "<p>Your meditation timer guides you through each phase with precise timing and haptic feedback:</p>";
        content += "<ul>";
        content += "<li><strong>Round Selection:</strong> Short button presses cycle through 1-" + String(loadConfig().maxRounds) + " rounds</li>";
        content += "<li><strong>Phase Transitions:</strong> Automatic timing with manual override capability</li>";
        content += "<li><strong>Vibration Cues:</strong> Clear feedback for each phase transition</li>";
        content += "<li><strong>Progress Tracking:</strong> Buzzes indicate current round number</li>";
        content += "<li><strong>Session Logging:</strong> Automatic recording of your practice sessions</li>";
        content += "</ul>";
        
        content += "<h3>📋 Complete Session Flow</h3>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #007bff;'>";
        content += "<h4>🏠 IDLE - Preparation</h4>";
        content += "<p><strong>Action:</strong> Select your desired number of rounds (1-" + String(loadConfig().maxRounds) + ") with short button presses</p>";
        content += "<p><strong>Device:</strong> Vibrates equal to selected rounds after 1-second delay</p>";
        content += "<p><strong>Start:</strong> Long press (2+ seconds) to begin session</p>";
        content += "</div>";
        
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #28a745;'>";
        content += "<h4>🫁 DEEP BREATHING - Oxygenation</h4>";
        content += "<p><strong>Technique:</strong> Breathe deeply and rhythmically - in through nose, out through mouth</p>";
        content += "<p><strong>Duration:</strong> Default " + String(loadConfig().deepBreathingSeconds) + " seconds (configurable)</p>";
        content += "<p><strong>Feel:</strong> Tingling, lightheadedness, or slight dizziness is normal</p>";
        content += "<p><strong>Transition:</strong> Long vibration after timeout, or short press when ready</p>";
        content += "<p><strong>Preparation:</strong> When advancing - breathe out completely and hold</p>";
        content += "</div>";
        
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #dc3545;'>";
        content += "<h4>🛑 BREATH HOLD - The Core</h4>";
        content += "<p><strong>Position:</strong> Hold breath after complete exhale - lungs empty</p>";
        content += "<p><strong>Mindset:</strong> Stay relaxed, don't force it, trust your body</p>";
        content += "<p><strong>Duration:</strong> As long as comfortable - everyone is different</p>";
        content += "<p><strong>Progression:</strong> When you feel the urge to breathe, try holding a few seconds more</p>";
        content += "<p><strong>Transition:</strong> Take deep breath in, hold 10-15 seconds, then short press</p>";
        content += "</div>";
        
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #ffc107;'>";
        content += "<h4>💨 RECOVERY - Integration</h4>";
        content += "<p><strong>Breath:</strong> Hold deep recovery breath for 10-15 seconds</p>";
        content += "<p><strong>Purpose:</strong> Integrates physiological changes from breath hold</p>";
        content += "<p><strong>Duration:</strong> Default " + String(loadConfig().recoverySeconds) + " seconds (configurable)</p>";
        content += "<p><strong>Next Round:</strong> Device buzzes equal to next round number</p>";
        content += "<p><strong>Final Round:</strong> Device gives one long buzz for silent phase</p>";
        content += "</div>";
        
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #6f42c1;'>";
        content += "<h4>🧘 SILENT MEDITATION - Integration</h4>";
        content += "<p><strong>Experience:</strong> Heightened awareness and calm after breathing rounds</p>";
        content += "<p><strong>Practice:</strong> Observe inner sensations, thoughts, and feelings</p>";
        content += "<p><strong>Duration:</strong> As long as feels right (max " + String(loadConfig().silentPhaseMaxMinutes) + " minutes)</p>";
        content += "<p><strong>End:</strong> Short press when ready to complete session</p>";
        content += "</div>";
        
        content += "<h3>⚠️ Safety Guidelines</h3>";
        content += "<div style='background:#fff3cd;padding:15px;margin:15px 0;border-radius:5px;border:1px solid #ffeaa7;'>";
        content += "<p><strong>⚡ Never practice:</strong></p>";
        content += "<ul>";
        content += "<li>While driving, swimming, or in water</li>";
        content += "<li>Standing up (always sit or lie down)</li>";
        content += "<li>If pregnant or with serious medical conditions</li>";
        content += "</ul>";
        content += "<p><strong>🔍 Normal sensations:</strong> Tingling, lightheadedness, feeling of euphoria</p>";
        content += "<p><strong>🛑 Stop if you experience:</strong> Severe dizziness, chest pain, or discomfort</p>";
        content += "<p><strong>👨‍⚕️ Consult a doctor</strong> if you have heart conditions, breathing disorders, or other health concerns</p>";
        content += "</div>";
        
        content += "<h3>💡 Tips for Beginners</h3>";
        content += "<ul>";
        content += "<li><strong>Start small:</strong> Begin with 1-2 rounds to learn the technique</li>";
        content += "<li><strong>Use Training Mode:</strong> Enable on the Dashboard for real-time guidance</li>";
        content += "<li><strong>Find your rhythm:</strong> Don't rush - quality over quantity</li>";
        content += "<li><strong>Track progress:</strong> Review your session logs to see improvement</li>";
        content += "<li><strong>Be patient:</strong> Breath hold times improve gradually with practice</li>";
        content += "<li><strong>Comfortable position:</strong> Sit upright or lie down comfortably</li>";
        content += "<li><strong>Quiet environment:</strong> Minimize distractions for best results</li>";
        content += "</ul>";
        
        content += "<h3>🎯 Getting Started</h3>";
        content += "<p><strong>1. Read this guide completely</strong></p>";
        content += "<p><strong>2. Go to Dashboard and enable Training Mode</strong> for your first sessions</p>";
        content += "<p><strong>3. Start with 1-2 rounds</strong> to learn the rhythm</p>";
        content += "<p><strong>4. Practice regularly</strong> - consistency builds strength</p>";
        content += "<p><strong>5. Adjust settings</strong> as you become more experienced</p>";
        
        content += "<div style='background:#d4edda;padding:15px;margin:20px 0;border-radius:5px;text-align:center;'>";
        content += "<p><strong>🌟 Remember: This is a practice, not a performance. Listen to your body and enjoy the journey! 🌟</strong></p>";
        content += "</div>";

        // Box Breathing section
        content += "<h2 id='box'>[2] Box Breathing</h2>";
        content += "<p>Box breathing uses equal-length phases to create a steady rhythm that calms the nervous system and sharpens attention.</p>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #28a745;'>";
        content += "<p><strong>Pattern:</strong> Inhale " + String(cfg.boxSeconds) + "s → Hold " + String(cfg.boxSeconds) + "s → Exhale " + String(cfg.boxSeconds) + "s → Hold " + String(cfg.boxSeconds) + "s</p>";
        content += "<p><strong>Config:</strong> Adjust seconds (2–8) in Settings. Use short presses in IDLE to change value.</p>";
        content += "<p><strong>Tips:</strong> Keep shoulders relaxed, breathe quietly through the nose if comfortable.</p>";
        content += "</div>";

        // 4-7-8 section
        content += "<h2 id='478'>[3] 4·7·8 Breathing</h2>";
        content += "<p>A relaxation-focused cadence popularized for easing into sleep and reducing stress.</p>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #6f42c1;'>";
        content += "<p><strong>Pattern:</strong> Inhale 4s → Hold 7s → Exhale 8s (repeat gentle cycles).</p>";
        content += "<p><strong>Focus:</strong> Soften the exhale; let it be long and unforced. Stop if lightheaded.</p>";
        content += "</div>";

        // Resonant section
        content += "<h2 id='resonant'>[4] Resonant Breathing (6:6)</h2>";
        content += "<p>Breathing at ~6 breaths/min (6s inhale, 6s exhale) can improve heart rate variability and calm.</p>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #17a2b8;'>";
        content += "<p><strong>Pattern:</strong> Inhale 6s → Exhale 6s. Keep it smooth; no breath holds.</p>";
        content += "<p><strong>Focus:</strong> Breathe diaphragmatically; let the belly lead the breath.</p>";
        content += "</div>";

        // Custom section
        content += "<h2 id='custom'>[5] Custom (Timed Prompts)</h2>";
        content += "<p>Define your own durations for Inhale → HoldIn → Exhale → HoldOut. The device buzzes at each phase boundary and loops the sequence.</p>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #343a40;'>";
        content += "<p><strong>Phases:</strong> Inhale → HoldIn → Exhale → HoldOut (seconds, 0–16; 0 = skip)</p>";
        content += "<p><strong>Haptics:</strong> short buzz at phase boundaries</p>";
        content += "<p><strong>Loop:</strong> repeats until you stop</p>";
        content += "<p><strong>Bounds:</strong> all values clamped to 0–16s</p>";
        content += "<p><strong>Presets:</strong> Box (4,4,4,4), 6–6 (6,0,6,0), 4·7·8 (4,7,8,0)</p>";
        content += "<p><strong>Behavior:</strong> Builds a list of active phases (skips zeros), buzzes at each start, runs for the specified seconds, then advances; on last phase end, loops.</p>";
        content += "<p><strong>Edge cases:</strong> If all values are zero, session won't start (\\\"Set at least one phase\\\"). Values >16 are clamped.</p>";
        content += "</div>";

        // Dynamic section
        content += "<h2 id='dynamic'>[6] Dynamic (Tap‑to‑Teach Cadence)</h2>";
        content += "<p>Teach the device your inhale/exhale timing by tapping the button at phase boundaries; it averages the last three samples for each and guides you at that cadence.</p>";
        content += "<div style='background:#f8f9fa;padding:15px;margin:15px 0;border-radius:5px;border-left:4px solid #20c997;'>";
        content += "<p><strong>Teach:</strong> Short press at the start of Inhale, then at the start of Exhale. Repeat. Long-press to stop.</p>";
        content += "<p><strong>Sampling:</strong> Rolling 3 samples for Inhale and Exhale; averages rounded to 1s, clamped 1–16s</p>";
        content += "<p><strong>Confirm:</strong> After 3+3 samples: double-buzz (cadence locked) → guided run starts</p>";
        content += "<p><strong>Re‑teach:</strong> Keep pressing at boundaries to update cadence; applies next boundary</p>";
        content += "<p><strong>Timeouts:</strong> No second press within 20s during teaching → reset to idle; debounce 150ms</p>";
        content += "<p><strong>Haptics:</strong> boundary = 100ms; confirm = two×100ms (150ms gap); error = 300ms</p>";
        content += "</div>";
        
        response = generateHTML("Breathing Patterns Guide", content);
        
    } else if (request.indexOf("GET /wifi-setup") >= 0) {
        // WiFi Setup page for hotspot mode
        Serial.println("Serving WiFi setup page");
        String content = "<h2>📶 WiFi Setup</h2>";
        
        // Show WiFi connection status first
        
        if (isHotspotMode()) {
            content += "<div style='background:#fff3cd;padding:15px;margin:15px 0;border-radius:5px;border:1px solid #ffeaa7;'>";
            content += "<p><strong>🔄 Setup Mode Active</strong></p>";
            content += "<p>Your device is in setup mode. Enter your WiFi credentials below to connect to your network.</p>";
            content += "</div>";
        } else {
            content += "<div style='background:#d4edda;padding:15px;margin:15px 0;border-radius:5px;'>";
            content += "<p><strong>✅ Connected to WiFi</strong></p>";
            content += "<p>Your device is connected to: <strong>" + String(WiFi.SSID()) + "</strong></p>";
            content += "<p>You can change WiFi settings below if needed.</p>";
            content += "</div>";
        }
        
        // Show stored WiFi credentials near the top, after status
        {
            WiFiCredentials wifiCreds = loadWiFiCredentials();
            content += "<h3>Stored WiFi Credentials</h3>";
            if (wifiCreds.isConfigured && strlen(wifiCreds.ssid) > 0) {
                content += "<div style='background:#e2e3e5;color:#383d41;padding:10px;margin:10px 0;border-radius:5px;'>";
                content += "<strong>SSID:</strong> " + String(wifiCreds.ssid) + "<br>";
                String maskedPwd = "";
                for (size_t i = 0; i < strlen(wifiCreds.password); ++i) maskedPwd += '*';
                content += "<strong>Password:</strong> " + maskedPwd + "<br>";
                content += "<strong>Status:</strong> " + String(wifiCreds.isConfigured ? "Configured" : "Not Configured") + "";
                content += "</div>";
            } else {
                content += "<div style='background:#f8d7da;color:#721c24;padding:10px;margin:10px 0;border-radius:5px;'>";
                content += "No WiFi credentials stored";
                content += "</div>";
            }
        }
        
        // WiFi connect form
        content += "<form method='POST' action='/save-wifi' onsubmit=\"return validateWifiForm()\">";
        content += "<div class='form-group'><label>Network Name (SSID):</label>";
        content += "<input type='text' name='ssid' placeholder='Enter WiFi network name' required></div>";
        content += "<div class='form-group'><label>Password:</label>";
        content += "<input type='password' name='password' placeholder='Enter WiFi password'></div>";
        content += "<p style='font-size:12px;color:#666;margin-top:-8px;'>SSID: 1–31 characters, Password: 0–63 characters</p>";
        content += "<button type='submit'>Connect to WiFi</button>";
        content += "</form>";
        
        // IP Address Vibration Explanation
        content += "<div style='background:#e7f3ff;padding:15px;margin:15px 0;border-radius:5px;border:1px solid #b3d9ff;'>";
        content += "<h3>📳 IP Address Notification</h3>";
        content += "<p><strong>After successfully connecting to WiFi, your device will vibrate the last part of its IP address so you can access it from your network.</strong></p>";
        content += "<p><strong>Vibration Pattern:</strong></p>";
        content += "<ul style='margin:10px 0;padding-left:20px;'>";
        content += "<li><strong>Long buzz</strong> - Start of IP notification</li>";
        content += "<li><strong>Short buzzes</strong> - Each digit (1 buzz = 1, 2 buzzes = 2, etc.)</li>";
        content += "<li><strong>10 buzzes</strong> - Represents the digit 0</li>";
        content += "<li><strong>Long pause</strong> - Between each digit</li>";
        content += "<li><strong>Long buzz</strong> - End of IP notification</li>";
        content += "</ul>";
        content += "<p><strong>Example:</strong> For IP ending in .165:<br>";
        content += "Long buzz → 1 buzz → pause → 6 buzzes → pause → 5 buzzes → Long buzz</p>";
        content += "<p><strong>📝 Tip:</strong> Have a pen ready to write down the numbers as the device vibrates them!</p>";
        content += "</div>";
        
        content += "<h3>📱 Available Networks</h3>";
        content += "<p><em>Scanning for nearby WiFi networks...</em></p>";
        content += "<div id='networks'></div>";
        
        content += "<script>";
        content += "function validateWifiForm(){";
        content += "  const ssid=document.querySelector('input[name=ssid]').value;";
        content += "  const pwd=document.querySelector('input[name=password]').value;";
        content += "  if(ssid.length<1||ssid.length>31){alert('SSID must be between 1 and 31 characters.');return false;}";
        content += "  if(pwd.length>63){alert('Password must be 63 characters or fewer.');return false;}";
        content += "  return true;";
        content += "}";
        content += "function scanNetworks() {";
        content += "  fetch('/scan-wifi').then(r=>r.json()).then(data=>{";
        content += "    let html = '<ul>';";
        content += "    data.networks.forEach(net => {";
        content += "      html += '<li style=\"margin:5px 0;padding:8px;background:#f8f9fa;border-radius:3px;\">';";
        content += "      html += '<strong>' + net.ssid + '</strong> (' + net.rssi + ' dBm)';";
        content += "      html += ' <button onclick=\"document.querySelector(\\'input[name=ssid]\\').value=\\'' + net.ssid + '\\'\" style=\"margin-left:10px;padding:2px 8px;background:#007bff;color:white;border:none;border-radius:3px;cursor:pointer;\">Select</button>';";
        content += "      html += '</li>';";
        content += "    });";
        content += "    html += '</ul>';";
        content += "    document.getElementById('networks').innerHTML = html;";
        content += "  }).catch(e=>console.log('Scan failed'));";
        content += "}";
        content += "setTimeout(scanNetworks, 1000);";
        content += "</script>";
        
        response = generateHTML("WiFi Setup", content);
        
    } else if (request.indexOf("GET /config") >= 0) {
        // Configuration form
        AppConfig config = loadConfig();
        String content = "<h2>Configuration</h2>";
        // Saved banner if redirected with saved=1
        if (request.indexOf("saved=1") >= 0) {
            content += "<div class='status'>Saved successfully!</div>";
        }
        content += "<form method='POST' action='/save'>";
        content += "<input type='hidden' name='returnTo' value='/config'/>";

        // Breathing Patterns removed (managed on Dashboard)

        // Device Settings
        content += "<h3>Device Settings</h3>";
        content += "<div class='form-group'><label>Idle Timeout (1-60 min):</label>";
        content += "<input type='number' name='idleTimeoutMinutes' min='1' max='60' value='" + String(config.idleTimeoutMinutes) + "'></div>";
        content += "<div class='form-group'><label>Start confirmation haptics (type + value):</label>";
        content += "<select name='startConfirmationHaptics'>";
        content += "<option value='1'" + String(config.startConfirmationHaptics ? " selected" : "") + ">On</option>";
        content += "<option value='0'" + String(!config.startConfirmationHaptics ? " selected" : "") + ">Off</option>";
        content += "</select></div>";
        content += "<div class='form-group'><label>Keep partial session if ≥ (seconds):</label>";
        content += "<input type='number' name='abortSaveThresholdSeconds' min='10' max='3600' value='" + String(config.abortSaveThresholdSeconds) + "'></div>";
        content += "<div class='form-group'><label>Guided Breathing Duration (non‑Wim Hof) (1–120 min):</label>";
        content += "<input type='number' name='guidedBreathingMinutes' min='1' max='120' value='" + String(config.guidedBreathingMinutes) + "'></div>";
        // Silent Phase block
        content += "<h3>Silent Phase</h3>";
        content += "<div class='form-group'><label>Silent Max (5-120 min):</label>";
        content += "<input type='number' name='silentPhaseMaxMinutes' min='5' max='120' value='" + String(config.silentPhaseMaxMinutes) + "'></div>";
        content += "<div class='form-group'><label>Silent Reminders:</label>";
        content += "<select name='silentReminderEnabled'>";
        content += "<option value='1'" + String(config.silentReminderEnabled ? " selected" : "") + ">Enabled</option>";
        content += "<option value='0'" + String(!config.silentReminderEnabled ? " selected" : "") + ">Disabled</option>";
        content += "</select></div>";
        content += "<div class='form-group'><label>Reminder Interval (1-30 min):</label>";
        content += "<input type='number' name='silentReminderIntervalMinutes' min='1' max='30' value='" + String(config.silentReminderIntervalMinutes) + "'></div>";
        // Current pattern dropdown removed; use radios above

        // Wim Hof Settings
        content += "<h3>Wim Hof Settings</h3>";
        content += "<div class='form-group'><label>Max Rounds (1-10):</label>";
        content += "<input type='number' name='maxRounds' min='1' max='10' value='" + String(config.maxRounds) + "'></div>";
        content += "<div class='form-group'><label>Deep Breathing (10-300s):</label>";
        content += "<input type='number' name='deepBreathingSeconds' min='10' max='300' value='" + String(config.deepBreathingSeconds) + "'></div>";
        content += "<div class='form-group'><label>Recovery (5-120s):</label>";
        content += "<input type='number' name='recoverySeconds' min='5' max='120' value='" + String(config.recoverySeconds) + "'></div>";
        content += "<div class='form-group'><label>Silent Max (5-120 min):</label>";
        content += "<input type='number' name='silentPhaseMaxMinutes' min='5' max='120' value='" + String(config.silentPhaseMaxMinutes) + "'></div>";
        content += "<div class='form-group'><label>Silent Reminders:</label>";
        content += "<select name='silentReminderEnabled'>";
        content += "<option value='1'" + String(config.silentReminderEnabled ? " selected" : "") + ">Enabled</option>";
        content += "<option value='0'" + String(!config.silentReminderEnabled ? " selected" : "") + ">Disabled</option>";
        content += "</select></div>";
        content += "<div class='form-group'><label>Reminder Interval (1-30 min):</label>";
        content += "<input type='number' name='silentReminderIntervalMinutes' min='1' max='30' value='" + String(config.silentReminderIntervalMinutes) + "'></div>";

        // Box Breathing Settings
        content += "<h3>Box Breathing Settings</h3>";
        content += "<div class='form-group'><label>Box Seconds (2-8):</label>";
        content += "<input type='number' name='boxSeconds' min='2' max='8' value='" + String(config.boxSeconds) + "'></div>";

        // Custom Timed Prompts
        content += "<h3>Custom Timed Prompts</h3>";
        content += "<div class='form-group'><label>Inhale (0–16 s, 0 = skip):</label>";
        content += "<input type='number' name='customInhaleSeconds' min='0' max='16' value='" + String(config.customInhaleSeconds) + "'></div>";
        content += "<div class='form-group'><label>Hold In (0–16 s, 0 = skip):</label>";
        content += "<input type='number' name='customHoldInSeconds' min='0' max='16' value='" + String(config.customHoldInSeconds) + "'></div>";
        content += "<div class='form-group'><label>Exhale (0–16 s, 0 = skip):</label>";
        content += "<input type='number' name='customExhaleSeconds' min='0' max='16' value='" + String(config.customExhaleSeconds) + "'></div>";
        content += "<div class='form-group'><label>Hold Out (0–16 s, 0 = skip):</label>";
        content += "<input type='number' name='customHoldOutSeconds' min='0' max='16' value='" + String(config.customHoldOutSeconds) + "'></div>";

        // Pattern Offering removed; handled by checkboxes above

        content += "<button type='submit'>Save Settings</button></form>";
        response = generateHTML("Configuration", content);
        
    } else if (request.indexOf("POST /save") >= 0 && request.indexOf("POST /save-wifi") == -1) {
        // Save configuration
        AppConfig config = loadConfig();
        
        // Read headers until we find the blank line, capture Content-Length
        String body = "";
        int contentLength = -1;
        bool sawHeader = false;
        unsigned long startHeadersMs = millis();
        while (millis() - startHeadersMs < 2000) { // 2s header read window
            if (!client.available()) { delay(3); continue; }
            String line = client.readStringUntil('\n');
            if (line.startsWith("Content-Length:")) {
                contentLength = line.substring(String("Content-Length:").length()).toInt();
            }
            // Trim a trailing \r if present
            if (line.endsWith("\r")) line.remove(line.length()-1);
            if (line.length() == 0) {
                if (sawHeader) break; // true blank line after headers
                else continue;        // consume stray newline after request line
            }
            sawHeader = true;
        }
        // Read exact body length if known, otherwise fallback to draining
        if (contentLength >= 0) {
            body.reserve(contentLength);
            unsigned long startBodyMs = millis();
            while ((int)body.length() < contentLength && millis() - startBodyMs < 5000) {
                if (client.available()) {
                    body += (char)client.read();
                } else {
                    delay(3);
                }
            }
        } else {
            while (client.available()) { body += (char)client.read(); }
        }
        
        // Parse form data (simple parsing)
        
        if (body.indexOf("maxRounds=") >= 0) {
            int start = body.indexOf("maxRounds=") + 10;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.maxRounds = value.toInt();
        }
        
        if (body.indexOf("deepBreathingSeconds=") >= 0) {
            int start = body.indexOf("deepBreathingSeconds=") + 21;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.deepBreathingSeconds = value.toInt();
        }
        
        if (body.indexOf("recoverySeconds=") >= 0) {
            int start = body.indexOf("recoverySeconds=") + 16;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.recoverySeconds = value.toInt();
        }
        
        if (body.indexOf("idleTimeoutMinutes=") >= 0) {
            int start = body.indexOf("idleTimeoutMinutes=") + 19;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.idleTimeoutMinutes = body.substring(start, end).toInt();
        }
        
        if (body.indexOf("silentPhaseMaxMinutes=") >= 0) {
            int start = body.indexOf("silentPhaseMaxMinutes=") + 22;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.silentPhaseMaxMinutes = body.substring(start, end).toInt();
        }
        
        if (body.indexOf("silentReminderEnabled=") >= 0) {
            int start = body.indexOf("silentReminderEnabled=") + 22;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.silentReminderEnabled = (body.substring(start, end).toInt() == 1);
        }
        
        if (body.indexOf("silentReminderIntervalMinutes=") >= 0) {
            int start = body.indexOf("silentReminderIntervalMinutes=") + 30;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.silentReminderIntervalMinutes = body.substring(start, end).toInt();
        }

        // currentPatternId from radio; robust parse (handles position in body)
        {
            // Parse currentPatternId reliably even if multiple fields exist
            int lastIdx = -1, searchFrom = 0; int id = -1;
            while (true) {
                int idx = body.indexOf("currentPatternId=", searchFrom);
                if (idx < 0) break; lastIdx = idx; searchFrom = idx + 1;
            }
            if (lastIdx >= 0) {
                int start = lastIdx + 17; // length of "currentPatternId="
                int end = body.indexOf('&', start);
                if (end == -1) end = body.length();
                String val = body.substring(start, end);
                int parsed = 0; for (int k=0;k<(int)val.length();++k){ char c=val.charAt(k); if (c>='0'&&c<='9'){ parsed = parsed*10 + (c - '0'); } }
                if (parsed >= 1 && parsed <= 6) id = parsed;
            }
        if (id != -1) { config.currentPatternId = id; }
        }
        if (body.indexOf("boxSeconds=") >= 0) {
            int start = body.indexOf("boxSeconds=") + 11;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 2) val = 2; if (val > 8) val = 8;
            config.boxSeconds = val;
        }
        if (body.indexOf("startConfirmationHaptics=") >= 0) {
            String key = "startConfirmationHaptics=";
            int start = body.indexOf(key) + key.length();
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.startConfirmationHaptics = (body.substring(start, end).toInt() == 1);
        }
        if (body.indexOf("abortSaveThresholdSeconds=") >= 0) {
            int start = body.indexOf("abortSaveThresholdSeconds=") + 26;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 10) val = 10; if (val > 3600) val = 3600;
            config.abortSaveThresholdSeconds = val;
        }
        if (body.indexOf("guidedBreathingMinutes=") >= 0) {
            String key = "guidedBreathingMinutes=";
            int start = body.indexOf(key) + key.length();
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 1) val = 1; if (val > 120) val = 120;
            config.guidedBreathingMinutes = val;
        }
        if (body.indexOf("customInhaleSeconds=") >= 0) {
            int start = body.indexOf("customInhaleSeconds=") + 21;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 0) val = 0; if (val > 16) val = 16;
            config.customInhaleSeconds = val;
        }
        if (body.indexOf("customHoldInSeconds=") >= 0) {
            int start = body.indexOf("customHoldInSeconds=") + 21;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 0) val = 0; if (val > 16) val = 16;
            config.customHoldInSeconds = val;
        }
        if (body.indexOf("customExhaleSeconds=") >= 0) {
            int start = body.indexOf("customExhaleSeconds=") + 21;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 0) val = 0; if (val > 16) val = 16;
            config.customExhaleSeconds = val;
        }
        if (body.indexOf("customHoldOutSeconds=") >= 0) {
            int start = body.indexOf("customHoldOutSeconds=") + 22;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            int val = body.substring(start, end).toInt();
            if (val < 0) val = 0; if (val > 16) val = 16;
            config.customHoldOutSeconds = val;
        }
        // Parse pattern order: patternOrder=comma-separated ids, dedupe and fill missing
        int poIdx = body.indexOf("patternOrder=");
        if (poIdx >= 0) {
            int start = poIdx + 13;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String csv = body.substring(start, end);
            bool seen[7]; for (int i=0;i<7;i++) seen[i]=false;
            int out[6]; int outIdx=0;
            int pos=0;
            while (pos <= (int)csv.length()) {
                int comma = csv.indexOf(',', pos);
                String tok = (comma==-1)? csv.substring(pos) : csv.substring(pos, comma);
                if (tok.length()>0) {
                    int val = 0; for (int k=0;k<(int)tok.length();++k){ char c=tok.charAt(k); if (c>='0'&&c<='9'){ val = val*10 + (c - '0'); } }
                    if (val >= 1 && val <= 6 && !seen[val]) {
                        out[outIdx++] = val; seen[val]=true;
                        if (outIdx==6) break;
                    }
                }
                if (comma==-1) break; pos = comma+1;
            }
            for (int id=1; id<=6 && outIdx<6; ++id) {
                if (!seen[id]) { out[outIdx++] = id; }
            }
            for (int i=0;i<6;i++) config.patternOrder[i] = out[i];
            // patternOrder parsed
        }
        // Only update pattern include/silent flags when the pattern list form is submitted
        bool hasPatternList = body.indexOf("patternOrder=") >= 0;
        if (hasPatternList) {
            // Checkboxes: presence means include=1; absence means 0
            config.includeWimHof = body.indexOf("includeWimHof=") >= 0;
            config.includeBox = body.indexOf("includeBox=") >= 0;
            config.include478 = body.indexOf("include478=") >= 0;
            config.includeResonant = body.indexOf("includeResonant=") >= 0;
            config.includeCustom = body.indexOf("includeCustom=") >= 0;
            config.includeDynamic = body.indexOf("includeDynamic=") >= 0;
            // Per-pattern silent flags
            config.silentAfterWimHof = body.indexOf("silentAfterWimHof=") >= 0;
            config.silentAfterBox = body.indexOf("silentAfterBox=") >= 0;
            config.silentAfter478 = body.indexOf("silentAfter478=") >= 0;
            config.silentAfterResonant = body.indexOf("silentAfterResonant=") >= 0;
            config.silentAfterCustom = body.indexOf("silentAfterCustom=") >= 0;
            config.silentAfterDynamic = body.indexOf("silentAfterDynamic=") >= 0;
        }
        // includes parsed
        
        // Save configuration
        saveConfig(config);
        // Ensure session uses latest persisted values immediately
        reloadSessionConfig();
        // Saved
        
        // Verification logging removed
        
        // Redirect to the requested page with saved flag
        int rtIdx = body.indexOf("returnTo=");
        if (rtIdx >= 0) {
            int start = rtIdx + 9;
            int end = body.indexOf('&', start);
            String dest = (end == -1) ? body.substring(start) : body.substring(start, end);
            if (dest.length() == 0) dest = "/";
            // dest may be URL-encoded; minimal decode for %2F
            dest.replace("%2F", "/");
            String loc = dest;
            if (loc.indexOf('?') >= 0) loc += "&saved=1"; else loc += "?saved=1";
            client.println("HTTP/1.1 302 Found");
            client.println("Location: " + loc);
            client.println("Connection: close");
            client.println();
            return;
        }
        
    } else if (request.indexOf("GET /logs") >= 0) {
        // Session logs
        String content = "<h2>Session Logs</h2>";
        String logs = getSessionLogsJson();
        
        if (logs == "[]") {
            content += "<p>No session logs found.</p>";
        } else {
            content += "<div class='button-group'>";
            content += "<a href='/download' class='download-btn'>Download JSON</a>";
            content += "<button class='delete-all-btn' onclick='if(confirmDeleteAll()) window.location.href=\"/delete-all\"'>Delete All Sessions</button>";
            content += "</div>";
            
            JsonDocument doc;
            deserializeJson(doc, logs);
            JsonArray logsArray = doc.as<JsonArray>();
            
            for (size_t sessionIndex = 0; sessionIndex < logsArray.size(); sessionIndex++) {
                JsonVariant log = logsArray[sessionIndex];
                content += "<div class='logs'>";
                content += "<strong>Date:</strong> " + String(log["date"].as<const char*>()) + " ";
                content += "<strong>Time:</strong> " + String(log["start_time"].as<const char*>()) + "<br>";
                content += "<strong>Pattern:</strong> " + String(log["pattern_name"].is<const char*>() ? log["pattern_name"].as<const char*>() : "Unknown") + "<br>";
                content += "<strong>Total Duration:</strong> " + formatDuration(log["total"].as<int>()) + " ";
                content += "<strong>Silent Phase:</strong> " + formatDuration(log["silent"].as<int>()) + "<br>";

                int patternId = log["pattern_id"].is<int>() ? log["pattern_id"].as<int>() : 0;
                bool printedRounds = false;
                if (log["rounds"].is<JsonArray>()) {
                    JsonArray rounds = log["rounds"];
                    if (rounds.size() > 0) {
                        content += "<strong>Rounds:</strong> " + String(rounds.size()) + "<br>";
                        for (size_t i = 0; i < rounds.size(); i++) {
                            JsonObject round = rounds[i];
                            content += "R" + String(i + 1) + ": ";
                            content += "Deep=" + formatDuration(round["deep"].as<int>()) + " ";
                            content += "Hold=" + formatDuration(round["hold"].as<int>()) + " ";
                            content += "Recover=" + formatDuration(round["recover"].as<int>()) + "<br>";
                        }
                        printedRounds = true;
                    }
                }

                // For non-Wim Hof patterns (or if no rounds recorded), show a single logical round (R1)
                if (!printedRounds) {
                    content += "<strong>Rounds:</strong> 1<br>";
                    String r1;
                    JsonObject settings = log["settings"].is<JsonObject>() ? log["settings"].as<JsonObject>() : JsonObject();
                    switch (patternId) {
                        case 2: { // Box
                            int s = settings["boxSeconds"].is<int>() ? settings["boxSeconds"].as<int>() : 0;
                            if (s <= 0) s = 4; // sensible fallback
                            r1 = "Box Seconds = " + String(s) + "s";
                            break;
                        }
                        case 3: { // 4-7-8
                            r1 = "In 4s, Hold 7s, Out 8s";
                            break;
                        }
                        case 4: { // Resonant 6:6
                            r1 = "In 6s, Out 6s";
                            break;
                        }
                        case 5: { // Custom
                            int inh = settings["customInhaleSeconds"].is<int>() ? settings["customInhaleSeconds"].as<int>() : 0;
                            int hi  = settings["customHoldInSeconds"].is<int>() ? settings["customHoldInSeconds"].as<int>() : 0;
                            int exh = settings["customExhaleSeconds"].is<int>() ? settings["customExhaleSeconds"].as<int>() : 0;
                            int ho  = settings["customHoldOutSeconds"].is<int>() ? settings["customHoldOutSeconds"].as<int>() : 0;
                            bool first = true;
                            if (inh > 0) { r1 += (first?"":"; "); r1 += "In "; r1 += String(inh); r1 += "s"; first=false; }
                            if (hi  > 0) { r1 += (first?"":"; "); r1 += "HoldIn "; r1 += String(hi);  r1 += "s"; first=false; }
                            if (exh > 0) { r1 += (first?"":"; "); r1 += "Out "; r1 += String(exh); r1 += "s"; first=false; }
                            if (ho  > 0) { r1 += (first?"":"; "); r1 += "HoldOut "; r1 += String(ho);  r1 += "s"; first=false; }
                            if (r1.length() == 0) r1 = "No phases configured";
                            break;
                        }
                        case 6: { // Dynamic
                            int ai = settings["avgInhaleSec"].is<int>() ? settings["avgInhaleSec"].as<int>() : 0;
                            int ae = settings["avgExhaleSec"].is<int>() ? settings["avgExhaleSec"].as<int>() : 0;
                            if (ai > 0 || ae > 0) {
                                r1 = "~In "; r1 += String(ai); r1 += "s, ~Out "; r1 += String(ae); r1 += "s";
                            } else {
                                r1 = "Dynamic cadence (teaching/guided)";
                            }
                            break;
                        }
                        case 1: // Wim Hof with no recorded rounds (fallback)
                        default: {
                            r1 = "Summary not available";
                            break;
                        }
                    }
                    content += "R1: " + r1 + "<br>";
                }

                content += "<button class='delete-btn' onclick='if(confirmDelete(" + String(sessionIndex) + ")) window.location.href=\"/delete-session?index=" + String(sessionIndex) + "\"'>Delete Session</button>";
                content += "</div>";
            }
        }
        response = generateHTML("Session Logs", content);
        
    } else if (request.indexOf("GET /download") >= 0) {
        // Download JSON
        String logs = getSessionLogsJson();
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Content-Disposition: attachment; filename=\"meditation_logs.json\"");
        client.println("Connection: close");
        client.println();
        client.println(logs);
        return;
        
    } else if (request.indexOf("GET /delete-session") >= 0) {
        // Delete individual session
        int indexStart = request.indexOf("index=");
        if (indexStart >= 0) {
            indexStart += 6; // Skip "index="
            int indexEnd = request.indexOf(" ", indexStart);
            if (indexEnd == -1) indexEnd = request.indexOf("&", indexStart);
            if (indexEnd == -1) indexEnd = request.length();
            
            int sessionIndex = request.substring(indexStart, indexEnd).toInt();
            deleteSessionLog(sessionIndex);
            
            String content = "<div class='status'>Session deleted successfully!</div>";
            content += "<p><a href='/logs'>Back to Session Logs</a></p>";
            response = generateHTML("Session Deleted", content);
        } else {
            String content = "<h2>Error</h2><p>Invalid session index.</p>";
            content += "<p><a href='/logs'>Back to Session Logs</a></p>";
            response = generateHTML("Error", content);
        }
        
    } else if (request.indexOf("GET /delete-all") >= 0) {
        // Delete all sessions
        deleteAllSessionLogs();
        
        String content = "<div class='status'>All session logs deleted successfully!</div>";
        content += "<p><a href='/logs'>Back to Session Logs</a></p>";
        response = generateHTML("All Sessions Deleted", content);
        
    } else if (request.indexOf("GET /status") >= 0) {
        // JSON status endpoint for training mode
        AppConfig config = loadConfig();
        String trainingDesc = getTrainingDescription();
        trainingDesc.replace("\"", "\\\""); // Escape quotes for JSON
        trainingDesc.replace("\n", "\\n");   // Escape newlines for JSON
        // Build pattern label and settings snippet
        String pLabel;
        switch (config.currentPatternId) {
            case 1: pLabel = "[1] Wim Hof"; break;
            case 2: pLabel = "[2] Box"; break;
            case 3: pLabel = "[3] 4-7-8"; break;
            case 4: pLabel = "[4] Resonant (6:6)"; break;
            default: pLabel = "[?] Unknown"; break;
        }
        String pHtml;
        if (config.currentPatternId == 1) {
            pHtml = "<p><strong>Selected Rounds:</strong> <span id='currentRounds'>" + String(config.currentRound) + "</span></p>";
            pHtml += "<p><strong>Max Rounds:</strong> " + String(config.maxRounds) + "</p>";
            pHtml += "<p><strong>Deep Breathing:</strong> " + String(config.deepBreathingSeconds) + "s</p>";
            pHtml += "<p><strong>Recovery:</strong> " + String(config.recoverySeconds) + "s</p>";
            pHtml += "<p><strong>Silent Phase Max:</strong> " + String(config.silentPhaseMaxMinutes) + " min</p>";
            pHtml += "<p><strong>Silent Reminders:</strong> " + String(config.silentReminderEnabled ? "On" : "Off") + "</p>";
        } else if (config.currentPatternId == 2) {
            pHtml = "<p><strong>Box Seconds:</strong> " + String(config.boxSeconds) + "s</p>";
        } else if (config.currentPatternId == 3) {
            pHtml = "<p>No configurable settings yet for 4-7-8.</p>";
        } else if (config.currentPatternId == 4) {
            pHtml = "<p>No configurable settings yet for Resonant.</p>";
        } else if (config.currentPatternId == 5) {
            pHtml = "<p><strong>Custom Pattern:</strong> Inhale=" + String(config.customInhaleSeconds) + "s, HoldIn=" + String(config.customHoldInSeconds) + "s, Exhale=" + String(config.customExhaleSeconds) + "s, HoldOut=" + String(config.customHoldOutSeconds) + "s</p>";
        } else if (config.currentPatternId == 6) {
            pHtml = "<p><strong>Dynamic:</strong> Tap to teach inhale/exhale cadence; device guides at learned rhythm.</p>";
        }
        pHtml.replace("\"", "\\\"");
        pHtml.replace("\n", "\\n");
        
        String json = "{";
        json += "\"status\":\"" + getStatusString() + "\",";
        json += "\"currentRound\":" + String(config.currentRound) + ",";
        json += "\"state\":\"" + String((int)getCurrentState()) + "\",";
        json += "\"trainingDescription\":\"" + trainingDesc + "\",";
        json += "\"patternId\":" + String(config.currentPatternId) + ",";
        json += "\"patternLabel\":\"" + pLabel + "\",";
        json += "\"patternSettingsHtml\":\"" + pHtml + "\"";
        json += "}";
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println(json);
        return;
        
    } else if (request.indexOf("GET /scan-wifi") >= 0) {
        // WiFi network scan
        Serial.println("Starting WiFi scan...");
        int n = WiFi.scanNetworks();
        String json = "{\"networks\":[";
        
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]}";
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println(json);
        return;
        
    } else if (request.indexOf("POST /save-wifi") >= 0) {
        // Save WiFi credentials
        String body = "";
        
        // Skip headers until we find the blank line
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.length() <= 1) {
                break;
            }
        }
        
        // Read the POST body
        while (client.available()) {
            body += (char)client.read();
        }
        
        Serial.println("WiFi setup form data: " + body);
        
        // Helper function to decode URL encoded strings (handles %xx and +)
        auto urlDecode = [](const String& str) -> String {
            String out;
            out.reserve(str.length());
            auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return -1;
            };
            for (size_t i = 0; i < str.length(); ++i) {
                char c = str.charAt(i);
                if (c == '+') {
                    out += ' ';
                } else if (c == '%' && i + 2 < str.length()) {
                    int hi = fromHex(str.charAt(i + 1));
                    int lo = fromHex(str.charAt(i + 2));
                    if (hi >= 0 && lo >= 0) {
                        out += char((hi << 4) | lo);
                        i += 2;
                    } else {
                        // Invalid percent-encoding, keep as-is
                        out += c;
                    }
                } else {
                    out += c;
                }
            }
            return out;
        };
        
        WiFiCredentials newCreds;
        strcpy(newCreds.ssid, "");
        strcpy(newCreds.password, "");
        newCreds.isConfigured = false;
        
        // Parse SSID
        if (body.indexOf("ssid=") >= 0) {
            int start = body.indexOf("ssid=") + 5;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String ssidValue = urlDecode(body.substring(start, end));
            // Safe copy with truncation
            strncpy(newCreds.ssid, ssidValue.c_str(), sizeof(newCreds.ssid) - 1);
            newCreds.ssid[sizeof(newCreds.ssid) - 1] = '\0';
        }
        
        // Parse Password
        if (body.indexOf("password=") >= 0) {
            int start = body.indexOf("password=") + 9;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String passwordValue = urlDecode(body.substring(start, end));
            // Safe copy with truncation
            strncpy(newCreds.password, passwordValue.c_str(), sizeof(newCreds.password) - 1);
            newCreds.password[sizeof(newCreds.password) - 1] = '\0';
        }
        
        if (strlen(newCreds.ssid) > 0) {
            newCreds.isConfigured = true;
            
            // Debug output before saving
            Serial.println("=== WEBSERVER SAVE DEBUG ===");
            Serial.print("About to save - SSID: '");
            Serial.print(newCreds.ssid);
            Serial.println("'");
            // Do not print raw passwords to serial
            Serial.println("About to save - Password: [hidden]");
            Serial.print("About to save - isConfigured: ");
            Serial.println(newCreds.isConfigured ? "true" : "false");
            
            saveWiFiCredentials(newCreds);
            
            // Verify save by reading back
            Serial.println("=== VERIFYING SAVE ===");
            WiFiCredentials verifyData = loadWiFiCredentials();
            Serial.print("Verification - SSID: '");
            Serial.print(verifyData.ssid);
            Serial.println("'");
            // Do not print raw passwords to serial
            Serial.println("Verification - Password: [hidden]");
            Serial.print("Verification - isConfigured: ");
            Serial.println(verifyData.isConfigured ? "true" : "false");
            Serial.println("=== END VERIFICATION ===");
            
            String content = "<div class='status'>WiFi credentials saved! Device will restart and attempt to connect.</div>";
            content += "<p>If connection fails, the device will start hotspot mode again after 10 seconds.</p>";
            content += "<p><a href='/'>Back to Dashboard</a></p>";
            response = generateHTML("WiFi Saved", content);
            
            // Send response first, then restart
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.println(response);
            client.stop();
            
            // Set flag to vibrate IP after successful connection
            setVibrateIPFlag(true);
            
            // Restart device to apply new WiFi settings
            delay(1000);
            ESP.restart();
            return;
        } else {
            String content = "<div style='background:#f8d7da;color:#721c24;padding:15px;margin:15px 0;border-radius:5px;'>Error: Please enter a valid network name (SSID)</div>";
            content += "<p><a href='/wifi-setup'>Back to WiFi Setup</a></p>";
            response = generateHTML("WiFi Setup Error", content);
        }
        
    } else {
        // 404 Not Found - but redirect to wifi-setup if in hotspot mode
        if (isHotspotMode()) {
            Serial.println("Hotspot mode - redirecting to /wifi-setup");
            response = "HTTP/1.1 302 Found\r\nLocation: /wifi-setup\r\nConnection: close\r\n\r\n";
            client.println(response);
            return;
        } else {
            response = generateHTML("Not Found", "<h2>404 - Page Not Found</h2><p><a href='/'>Back to Dashboard</a></p>");
        }
    }
    
    // Send HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println(response);
}

bool setupWebServer() {
    if (!isWifiConnected() && !isHotspotMode()) {
        Serial.println("Neither WiFi connected nor hotspot mode. Web server not started.");
        return false;
    }

    // For hotspot mode, try to start the server with extra care
    if (isHotspotMode()) {
        Serial.println("Hotspot mode - Attempting to start web server...");
        
        // Add extra delay to ensure WiFi AP is fully ready
        delay(500);
        
        // Try to start the server
        server.begin();
        webServerRunning = true;
        
        Serial.println("===========================================");
        Serial.println("HOTSPOT WEB SERVER STARTED!");
        Serial.println("===========================================");
        Serial.println("Web interface available at: http://192.168.4.1");
        Serial.println("Available pages:");
        Serial.println("  /wifi-setup - WiFi configuration");
        Serial.println("  / - Dashboard (limited in hotspot mode)");
        Serial.println("===========================================");
        return true;
    }

    server.begin();
    webServerRunning = true;
    
    Serial.println("===========================================");
    Serial.println("WEB SERVER STARTED!");
    Serial.println("===========================================");
    Serial.println("WiFi mode - Open in browser: http://" + WiFi.localIP().toString());
    Serial.println("Available pages:");
    Serial.println("  / - Dashboard with current status");
    Serial.println("  /guide - Wim Hof method guide");
    Serial.println("  /moon - Moon cycle progress");
    Serial.println("  /config - Configuration settings");
    Serial.println("  /logs - Session history");
    Serial.println("===========================================");
    return true;
}

void stopWebServer() {
    if (webServerRunning) {
        server.stop();
        webServerRunning = false;
        Serial.println("Web server stopped");
    }
}

void handleWebServer() {
    if (webServerRunning) {
        // Handle clients for both hotspot and WiFi modes
        WiFiClient client = server.available();
        if (client) {
            Serial.println("New web client connected");
            handleClient(client);
            client.stop();
            Serial.println("Web client disconnected");
        }
    } else {
        // Debug: Check why web server isn't running
        static unsigned long lastDebugTime = 0;
        if (millis() - lastDebugTime > 5000) { // Every 5 seconds
            Serial.print("Web server status - Running: ");
            Serial.print(webServerRunning);
            Serial.print(", WiFi connected: ");
            Serial.print(isWifiConnected());
            Serial.print(", Hotspot mode: ");
            Serial.println(isHotspotMode());
            lastDebugTime = millis();
        }
    }
} 