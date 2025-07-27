#include "webserver.h"
#include "storage.h"
#include "network.h"
#include "session.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

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

String generateHTML(String title, String content) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    html += "h1{color:#333;text-align:center;margin-bottom:20px}";
    html += ".nav{text-align:center;margin-bottom:20px}";
    html += ".nav a{margin:0 10px;padding:8px 15px;background:#007bff;color:white;text-decoration:none;border-radius:5px}";
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
    html += "    btn.innerText = 'Disable Training Mode';";
    html += "    btn.style.background = '#dc3545';";
    html += "    updateStatus();";
    html += "    startStatusPolling();";
    html += "  } else {";
    html += "    btn.innerText = 'Enable Training Mode';";
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
    html += "    document.getElementById('currentRounds').innerText = data.currentRound;";
    html += "  }).catch(e=>console.log('Status update failed'));";
    html += "}";
    html += "window.onload = function(){";
    html += "  const btn = document.getElementById('trainingBtn');";
    html += "  if(btn){";
    html += "    if(trainingMode){";
    html += "      btn.innerText = 'Disable Training Mode';";
    html += "      btn.style.background = '#dc3545';";
    html += "      updateStatus();";
    html += "      startStatusPolling();";
    html += "    } else {";
    html += "      btn.innerText = 'Enable Training Mode';";
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
    html += "<a href='/config'>Settings</a>";
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
    
    String response = "";
    
    if (request.indexOf("GET / ") >= 0) {
        // Dashboard
        AppConfig config = loadConfig();
        String content = "<div id='statusDiv' class='status'>" + getStatusString() + "</div>";
        content += "<div class='button-group'>";
        content += "<button id='trainingBtn' onclick='toggleTrainingMode()' style='background:#28a745;color:white;padding:8px 15px;border:none;border-radius:5px;cursor:pointer;'>Enable Training Mode</button>";
        content += "</div>";
        content += "<h2>Current Settings</h2>";
        content += "<p><strong>Selected Rounds:</strong> <span id='currentRounds'>" + String(config.currentRound) + "</span></p>";
        content += "<p><strong>Max Rounds:</strong> " + String(config.maxRounds) + "</p>";
        content += "<p><strong>Deep Breathing:</strong> " + String(config.deepBreathingSeconds) + "s</p>";
        content += "<p><strong>Recovery:</strong> " + String(config.recoverySeconds) + "s</p>";
        content += "<p><strong>Idle Timeout:</strong> " + String(config.idleTimeoutMinutes) + " min</p>";
        content += "<p><strong>Silent Phase Max:</strong> " + String(config.silentPhaseMaxMinutes) + " min</p>";
        content += "<p><strong>Silent Reminders:</strong> " + String(config.silentReminderEnabled ? "On" : "Off") + "</p>";
        response = generateHTML("Dashboard", content);
        
    } else if (request.indexOf("GET /guide") >= 0) {
        // Wim Hof Method Guide
        String content = "<h2>🧘 Wim Hof Breathing Method Guide</h2>";
        
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
        
        response = generateHTML("Wim Hof Method Guide", content);
        
    } else if (request.indexOf("GET /config") >= 0) {
        // Configuration form
        AppConfig config = loadConfig();
        String content = "<h2>Configuration</h2>";
        content += "<form method='POST' action='/save'>";
        content += "<div class='form-group'><label>Max Rounds (1-10):</label>";
        content += "<input type='number' name='maxRounds' min='1' max='10' value='" + String(config.maxRounds) + "'></div>";
        content += "<div class='form-group'><label>Deep Breathing (10-300s):</label>";
        content += "<input type='number' name='deepBreathingSeconds' min='10' max='300' value='" + String(config.deepBreathingSeconds) + "'></div>";
        content += "<div class='form-group'><label>Recovery (5-120s):</label>";
        content += "<input type='number' name='recoverySeconds' min='5' max='120' value='" + String(config.recoverySeconds) + "'></div>";
        content += "<div class='form-group'><label>Idle Timeout (1-60 min):</label>";
        content += "<input type='number' name='idleTimeoutMinutes' min='1' max='60' value='" + String(config.idleTimeoutMinutes) + "'></div>";
        content += "<div class='form-group'><label>Silent Max (5-120 min):</label>";
        content += "<input type='number' name='silentPhaseMaxMinutes' min='5' max='120' value='" + String(config.silentPhaseMaxMinutes) + "'></div>";
        content += "<div class='form-group'><label>Silent Reminders:</label>";
        content += "<select name='silentReminderEnabled'>";
        content += "<option value='1'" + String(config.silentReminderEnabled ? " selected" : "") + ">Enabled</option>";
        content += "<option value='0'" + String(!config.silentReminderEnabled ? " selected" : "") + ">Disabled</option>";
        content += "</select></div>";
        content += "<div class='form-group'><label>Reminder Interval (1-30 min):</label>";
        content += "<input type='number' name='silentReminderIntervalMinutes' min='1' max='30' value='" + String(config.silentReminderIntervalMinutes) + "'></div>";
        content += "<button type='submit'>Save Settings</button></form>";
        response = generateHTML("Configuration", content);
        
    } else if (request.indexOf("POST /save") >= 0) {
        // Save configuration
        AppConfig config = loadConfig();
        Serial.println("DEBUG: POST /save request received");
        Serial.println("DEBUG: Full request: " + request);
        
        // Read headers until we find the blank line, then read body
        String body = "";
        
        // Skip headers until we find the blank line
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line.length() <= 1) { // Empty line indicates end of headers
                break;
            }
        }
        
        // Now read the POST body
        while (client.available()) {
            body += (char)client.read();
        }
        
        Serial.println("DEBUG: Body received: " + body);
        Serial.println("DEBUG: Body length: " + String(body.length()));
        
        // Parse form data (simple parsing)
        Serial.println("DEBUG: Starting to parse form fields...");
        Serial.println("DEBUG: Config before parsing - maxRounds: " + String(config.maxRounds) + ", deepBreathingSeconds: " + String(config.deepBreathingSeconds) + ", recoverySeconds: " + String(config.recoverySeconds));
        
        if (body.indexOf("maxRounds=") >= 0) {
            int start = body.indexOf("maxRounds=") + 10;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.maxRounds = value.toInt();
            Serial.println("DEBUG: Parsed maxRounds - raw: '" + value + "' parsed: " + String(config.maxRounds));
        }
        
        if (body.indexOf("deepBreathingSeconds=") >= 0) {
            int start = body.indexOf("deepBreathingSeconds=") + 21;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.deepBreathingSeconds = value.toInt();
            Serial.println("DEBUG: Parsed deepBreathingSeconds - raw: '" + value + "' parsed: " + String(config.deepBreathingSeconds));
        }
        
        if (body.indexOf("recoverySeconds=") >= 0) {
            int start = body.indexOf("recoverySeconds=") + 16;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            String value = body.substring(start, end);
            config.recoverySeconds = value.toInt();
            Serial.println("DEBUG: Parsed recoverySeconds - raw: '" + value + "' parsed: " + String(config.recoverySeconds));
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
        
        Serial.println("DEBUG: About to save config with values:");
        Serial.println("  maxRounds: " + String(config.maxRounds));
        Serial.println("  deepBreathingSeconds: " + String(config.deepBreathingSeconds));
        Serial.println("  recoverySeconds: " + String(config.recoverySeconds));
        Serial.println("  idleTimeoutMinutes: " + String(config.idleTimeoutMinutes));
        Serial.println("  silentPhaseMaxMinutes: " + String(config.silentPhaseMaxMinutes));
        Serial.println("  silentReminderEnabled: " + String(config.silentReminderEnabled));
        Serial.println("  silentReminderIntervalMinutes: " + String(config.silentReminderIntervalMinutes));
        
        saveConfig(config);
        Serial.println("Configuration saved via web interface");
        
        // Verify the save by loading it back
        AppConfig verifyConfig = loadConfig();
        Serial.println("DEBUG: Verification - loaded back from storage:");
        Serial.println("  maxRounds: " + String(verifyConfig.maxRounds));
        Serial.println("  deepBreathingSeconds: " + String(verifyConfig.deepBreathingSeconds));
        Serial.println("  recoverySeconds: " + String(verifyConfig.recoverySeconds));
        
        String content = "<div class='status'>Settings saved successfully!</div>";
        content += "<p><a href='/config'>Back to Settings</a></p>";
        response = generateHTML("Saved", content);
        
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
                content += "<strong>Total Duration:</strong> " + formatDuration(log["total"].as<int>()) + " ";
                content += "<strong>Silent Phase:</strong> " + formatDuration(log["silent"].as<int>()) + "<br>";
                
                if (log["rounds"].is<JsonArray>()) {
                    JsonArray rounds = log["rounds"];
                    content += "<strong>Rounds:</strong> " + String(rounds.size()) + "<br>";
                    for (size_t i = 0; i < rounds.size(); i++) {
                        JsonObject round = rounds[i];
                        content += "R" + String(i + 1) + ": ";
                        content += "Deep=" + formatDuration(round["deep"].as<int>()) + " ";
                        content += "Hold=" + formatDuration(round["hold"].as<int>()) + " ";
                        content += "Recover=" + formatDuration(round["recover"].as<int>()) + "<br>";
                    }
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
        
        String json = "{";
        json += "\"status\":\"" + getStatusString() + "\",";
        json += "\"currentRound\":" + String(config.currentRound) + ",";
        json += "\"state\":\"" + String((int)getCurrentState()) + "\",";
        json += "\"trainingDescription\":\"" + trainingDesc + "\"";
        json += "}";
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println(json);
        return;
        
    } else {
        // 404 Not Found
        response = generateHTML("Not Found", "<h2>404 - Page Not Found</h2><p><a href='/'>Back to Dashboard</a></p>");
    }
    
    // Send HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println(response);
}

void setupWebServer() {
    if (!isWifiConnected()) {
        Serial.println("WiFi not connected. Web server not started.");
        return;
    }

    server.begin();
    webServerRunning = true;
    
    Serial.println("===========================================");
    Serial.println("WEB SERVER STARTED!");
    Serial.println("===========================================");
    Serial.println("Open in browser: http://" + WiFi.localIP().toString());
    Serial.println("Available pages:");
    Serial.println("  / - Dashboard with current status");
    Serial.println("  /config - Configuration settings");
    Serial.println("  /logs - Session history");
    Serial.println("===========================================");
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
        WiFiClient client = server.available();
        if (client) {
            Serial.println("New web client connected");
            handleClient(client);
            client.stop();
            Serial.println("Web client disconnected");
        }
    }
} 