#include <Arduino.h>
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

void setup() {
    Serial.begin(115200);
    Serial.println("Booting up...");

    setupVibration();
    setupStorage();
    setupSession(); // This now handles button setup internally
    setupNetwork();
    setupRtcTime();
    // setupWebServer(); // Will be called when WiFi is ready

    boot_sequence_start_time = millis();
}

void loop() {
    // These functions are all non-blocking and should be called on every loop
    loopVibration();
    loopNetwork();
    loopSession(); // This now handles button logic internally
    handleLedIndicator();
    handleWebServer(); // Handle web server requests

    // Setup web server once WiFi is connected
    if (isWifiConnected() && !webserver_setup_attempted) {
        Serial.println("WiFi ready - Starting web server...");
        setupWebServer();
        webserver_setup_attempted = true;
    }

    // Handle the initial boot delay without blocking
    if (getCurrentState() == SessionState::BOOTING) {
        if (millis() - boot_sequence_start_time > BOOT_DURATION) {
            finishBooting();
        }
    }
}