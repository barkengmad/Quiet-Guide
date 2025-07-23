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
    html += ".form-group{margin-bottom:15px}";
    html += "label{display:block;margin-bottom:5px;font-weight:bold;color:#555}";
    html += "input,select{width:100%;padding:8px;border:2px solid #ddd;border-radius:5px;font-size:14px}";
    html += "button{background:#28a745;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;width:100%}";
    html += "button:hover{background:#218838}";
    html += ".logs{background:#f8f9fa;padding:10px;margin:10px 0;border-radius:5px;font-family:monospace;font-size:12px}";
    html += "</style></head><body>";
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
        String content = "<div class='status'>" + getStatusString() + "</div>";
        content += "<h2>Current Settings</h2>";
        content += "<p><strong>Selected Rounds:</strong> " + String(config.currentRound) + "</p>";
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
            content += "<p><a href='/download' style='background:#28a745;color:white;padding:5px 10px;text-decoration:none;border-radius:3px;'>Download JSON</a></p>";
            
            JsonDocument doc;
            deserializeJson(doc, logs);
            JsonArray logsArray = doc.as<JsonArray>();
            
            for (JsonVariant log : logsArray) {
                content += "<div class='logs'>";
                content += "<strong>Date:</strong> " + String(log["date"].as<const char*>()) + " ";
                content += "<strong>Time:</strong> " + String(log["start_time"].as<const char*>()) + "<br>";
                content += "<strong>Total:</strong> " + String(log["total"].as<int>()) + "s ";
                content += "<strong>Silent:</strong> " + String(log["silent"].as<int>()) + "s<br>";
                
                if (log["rounds"].is<JsonArray>()) {
                    JsonArray rounds = log["rounds"];
                    content += "<strong>Rounds:</strong> " + String(rounds.size()) + "<br>";
                    for (size_t i = 0; i < rounds.size(); i++) {
                        JsonObject round = rounds[i];
                        content += "R" + String(i + 1) + ": ";
                        content += "Deep=" + String(round["deep"].as<int>()) + "s ";
                        content += "Hold=" + String(round["hold"].as<int>()) + "s ";
                        content += "Recover=" + String(round["recover"].as<int>()) + "s<br>";
                    }
                }
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