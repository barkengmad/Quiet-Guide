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
    switch (state) {
        case SessionState::IDLE:
            return "<h3>🏠 IDLE - Ready to Begin</h3>"
                   "<p><strong>What's happening:</strong> The device is ready for meditation. Use short button presses to select your desired number of rounds (1-10).</p>"
                   "<p><strong>Why it matters:</strong> Choosing the right number of rounds helps build your breath-holding capacity gradually. Start with fewer rounds and increase as you improve.</p>"
                   "<p><strong>Next phase:</strong> Long press the button to start your breathing session with the selected rounds.</p>";
                   
        case SessionState::DEEP_BREATHING:
            return "<h3>🫁 DEEP BREATHING - Oxygenate Your Body</h3>"
                   "<p><strong>What's happening:</strong> Take deep, controlled breaths to saturate your blood with oxygen. Breathe in through your nose, out through your mouth.</p>"
                   "<p><strong>Why it matters:</strong> This hyperventilation phase increases oxygen levels and decreases CO2, preparing your body for the breath hold. It triggers physiological changes that improve breath-holding capacity.</p>"
                   "<p><strong>Next phase:</strong> When you feel fully oxygenated (tingling, slight dizziness is normal), short press to move to breath hold.</p>";
                   
        case SessionState::BREATH_HOLD:
            return "<h3>🛑 BREATH HOLD - The Main Event</h3>"
                   "<p><strong>What's happening:</strong> Hold your breath after a final exhale. Stay relaxed, don't force it. Your body will signal when it's time to breathe.</p>"
                   "<p><strong>Why it matters:</strong> This activates your mammalian dive reflex, trains CO2 tolerance, and builds mental resilience. It's where the real benefits of the Wim Hof method occur.</p>"
                   "<p><strong>Next phase:</strong> When you feel the urge to breathe, take a deep breath and short press to start recovery.</p>";
                   
        case SessionState::RECOVERY:
            return "<h3>💨 RECOVERY - Integration Breath</h3>"
                   "<p><strong>What's happening:</strong> Take a deep breath in and hold for 10-15 seconds, then exhale slowly. This is your recovery breath.</p>"
                   "<p><strong>Why it matters:</strong> This phase helps integrate the physiological changes from the breath hold and prepares you for the next round (or silent phase if finished).</p>"
                   "<p><strong>Next phase:</strong> Short press when ready to continue to the next round, or enter silent meditation if this was your final round.</p>";
                   
        case SessionState::SILENT:
            return "<h3>🧘 SILENT MEDITATION - Inner Awareness</h3>"
                   "<p><strong>What's happening:</strong> Enjoy the heightened state of awareness after your breathing rounds. Meditate in silence, observing your inner experience.</p>"
                   "<p><strong>Why it matters:</strong> This phase allows you to experience the full benefits of the practice - increased focus, calmness, and bodily awareness that follows the breathing technique.</p>"
                   "<p><strong>Next phase:</strong> Stay as long as feels right, or short press when ready to end your session and return to idle.</p>";
                   
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
    html += "<a href='/config'>Settings</a>";
    html += "<a href='/logs'>Logs</a>";
    html += "</div>";
    html += content;
    html += "</div></body></html>";
    return html;
}

void handleClient(WiFiClient& client) {
    String request = client.readStringUntil('\r');
    client.flush();
    
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
        String body = "";
        
        // Read POST body
        while (client.available()) {
            body += (char)client.read();
        }
        
        // Parse form data (simple parsing)
        if (body.indexOf("maxRounds=") >= 0) {
            int start = body.indexOf("maxRounds=") + 10;
            int end = body.indexOf("&", start);
            if (end == -1) end = body.length();
            config.maxRounds = body.substring(start, end).toInt();
        }
        
        saveConfig(config);
        Serial.println("Configuration saved via web interface");
        
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