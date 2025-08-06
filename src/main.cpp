#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "vibration.h"
#include "network.h"
#include "storage.h"
#include "webserver.h"
#include "rtc_time.h"
#include "session.h"
#include "secrets.h"

unsigned long boot_sequence_start_time = 0;
const unsigned long BOOT_DURATION = 5000;
bool webserver_setup_attempted = false;
extern bool webServerRunning; // Declare external variable from webserver.cpp

void setup() {
    Serial.begin(115200);
    Serial.println("Booting up...");

    setupVibration();
    setupStorage();
    setupSession(); // This now handles button setup internally
    checkBootButtonForHotspot(); // Check for hotspot mode before network setup
    setupNetwork();
    setupRtcTime();
    // setupWebServer(); // Will be called when WiFi/hotspot is ready

    boot_sequence_start_time = millis();
}

void loop() {
    // These functions are all non-blocking and should be called on every loop
    loopVibration();
    loopNetwork();
    loopSession(); // This now handles button logic internally
    handleLedIndicator();
    handleWebServer(); // Handle web server requests

    // Setup web server once WiFi is connected OR hotspot is running
    if ((isWifiConnected() || isHotspotMode()) && !webserver_setup_attempted) {
        // Add a delay to ensure WiFi stack is fully ready
        static unsigned long web_server_delay = millis();
        if (millis() - web_server_delay > 2000) { // 2 second delay
            if (isHotspotMode()) {
                Serial.println("Hotspot ready - Starting web server...");
            } else {
                Serial.println("WiFi ready - Starting web server...");
            }
            
            // Try to setup web server with error handling
            if (setupWebServer()) {
                webserver_setup_attempted = true;
                Serial.println("Web server started successfully");
            } else {
                Serial.println("Failed to start web server - will retry");
                web_server_delay = millis(); // Reset delay for retry
            }
        }
    }
    
    // Debug: Show current network status every 10 seconds
    static unsigned long last_status_debug = 0;
    if (millis() - last_status_debug > 10000) {
        Serial.print("Network Status - WiFi connected: ");
        Serial.print(isWifiConnected());
        Serial.print(", Hotspot mode: ");
        Serial.print(isHotspotMode());
        Serial.print(", Web server running: ");
        Serial.println(webServerRunning);
        if (isWifiConnected()) {
            Serial.print("WiFi IP: ");
            Serial.println(WiFi.localIP().toString());
        }
        last_status_debug = millis();
    }

    // Handle the initial boot delay without blocking
    if (getCurrentState() == SessionState::BOOTING) {
        if (millis() - boot_sequence_start_time > BOOT_DURATION) {
            finishBooting();
        }
    }
}