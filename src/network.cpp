#include "network.h"
#include "config.h"
#include "storage.h"
#include "vibration.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include "time.h"

enum class NetworkStatus {
    BOOTING,
    CHECKING_CREDS,
    CONNECTING_WIFI,
    SYNCING_NTP,
    CONNECTED,
    HOTSPOT_STARTING,
    HOTSPOT_RUNNING,
    HOTSPOT_TIMEOUT,
    DISCONNECTED
};

NetworkStatus networkStatus = NetworkStatus::BOOTING;
unsigned long network_timeout_start = 0;
WiFiCredentials currentCreds;
bool forceHotspotMode = false;
DNSServer dnsServer;
const char* hotspotSSID = "MeditationTimer-Setup";
unsigned long hotspot_client_check = 0;

void checkBootButtonForHotspot() {
    // Check if button is pressed during boot (after debounce delay)
    delay(100); // Wait for hardware to stabilize
    if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("Boot button pressed - forcing hotspot mode");
        forceHotspotMode = true;
    }
}

void setupNetwork() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Start with LED off
    networkStatus = NetworkStatus::CHECKING_CREDS;
    network_timeout_start = millis();
}

bool isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void startHotspotMode() {
    Serial.println("Starting hotspot mode...");
    
    // Disconnect from any existing WiFi first
    WiFi.disconnect(true);
    delay(100);
    
    // Set WiFi mode to AP only
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Start the access point
    if (WiFi.softAP(hotspotSSID)) {
        Serial.print("Hotspot IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("Connect to 'MeditationTimer-Setup' and go to: http://192.168.4.1/wifi-setup");
        
        // DNS server disabled for now to prevent crashes
        // Users will need to go directly to http://192.168.4.1/wifi-setup
        Serial.println("DNS server disabled - go directly to http://192.168.4.1/wifi-setup");
        
        networkStatus = NetworkStatus::HOTSPOT_RUNNING;
        network_timeout_start = millis();
        hotspot_client_check = millis();
    } else {
        Serial.println("Failed to start hotspot!");
        networkStatus = NetworkStatus::DISCONNECTED;
    }
}

bool isHotspotMode() {
    return (networkStatus == NetworkStatus::HOTSPOT_RUNNING || 
            networkStatus == NetworkStatus::HOTSPOT_STARTING);
}

void loopNetwork() {
    switch (networkStatus) {
        case NetworkStatus::CHECKING_CREDS:
            currentCreds = loadWiFiCredentials();
            
            Serial.print("Checking credentials - Force hotspot: ");
            Serial.print(forceHotspotMode);
            Serial.print(", Configured: ");
            Serial.print(currentCreds.isConfigured);
            Serial.print(", SSID length: ");
            Serial.println(strlen(currentCreds.ssid));
            
            if (forceHotspotMode || !currentCreds.isConfigured || strlen(currentCreds.ssid) == 0) {
                Serial.println("No WiFi credentials or hotspot mode forced - starting hotspot");
                networkStatus = NetworkStatus::HOTSPOT_STARTING;
            } else {
                Serial.print("Attempting to connect to: ");
                Serial.println(currentCreds.ssid);
                WiFi.disconnect(true);
                delay(100);
                WiFi.mode(WIFI_STA);
                WiFi.begin(currentCreds.ssid, currentCreds.password);
                networkStatus = NetworkStatus::CONNECTING_WIFI;
                network_timeout_start = millis();
            }
            break;
            
        case NetworkStatus::CONNECTING_WIFI:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi Connected!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                
                // Check if we should vibrate IP address after setup
                if (getVibrateIPFlag()) {
                    Serial.println("Vibrating IP address after setup...");
                    vibrateIPAddress(WiFi.localIP());
                }
                
                networkStatus = NetworkStatus::SYNCING_NTP;
                network_timeout_start = millis();
                configTime(0, 0, "pool.ntp.org");
            } else if (millis() - network_timeout_start > 10000) { // 10 second timeout
                Serial.println("WiFi connection failed - assuming new location, starting hotspot");
                WiFi.disconnect(true);
                delay(100);
                networkStatus = NetworkStatus::HOTSPOT_STARTING;
            }
            break;
            
        case NetworkStatus::SYNCING_NTP:
            {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo)) {
                    Serial.println("NTP Sync successful.");
                    networkStatus = NetworkStatus::CONNECTED;
                    Serial.println("Network setup complete - Web server should be accessible now!");
                } else if (millis() - network_timeout_start > 5000) {
                    Serial.println("NTP Sync failed - continuing without time sync.");
                    networkStatus = NetworkStatus::CONNECTED;
                }
            }
            break;
            
        case NetworkStatus::HOTSPOT_STARTING:
            startHotspotMode();
            break;
            
        case NetworkStatus::HOTSPOT_RUNNING:
            // Check if any clients are connected (every 5 seconds)
            if (millis() - hotspot_client_check > 5000) {
                int clientCount = WiFi.softAPgetStationNum();
                if (clientCount > 0) {
                    Serial.print("Hotspot clients connected: ");
                    Serial.println(clientCount);
                    // Reset timeout when clients are active
                    network_timeout_start = millis();
                } else {
                    Serial.println("No clients connected to hotspot");
                }
                hotspot_client_check = millis();
            }
            
            // Auto-timeout after 60 seconds if no activity
            if (millis() - network_timeout_start > 60000) {
                Serial.println("Hotspot timeout - shutting down to save power");
                dnsServer.stop();
                WiFi.softAPdisconnect(true);
                delay(100);
                networkStatus = NetworkStatus::HOTSPOT_TIMEOUT;
            }
            break;
            
        case NetworkStatus::HOTSPOT_TIMEOUT:
        case NetworkStatus::DISCONNECTED:
            // Device continues working offline
            break;
            
        default:
            break;
    }
}

void handleLedIndicator() {
    static unsigned long lastToggleTime = 0;
    static bool ledState = false;

    unsigned long interval = 0;
    bool solid_on = false;

    switch (networkStatus) {
        case NetworkStatus::CHECKING_CREDS:
            interval = 125; // Very fast flash
            break;
        case NetworkStatus::CONNECTING_WIFI:
            interval = 250; // Slow flash
            break;
        case NetworkStatus::SYNCING_NTP:
            interval = 62; // Fast flash
            break;
        case NetworkStatus::CONNECTED:
            solid_on = true;
            break;
        case NetworkStatus::HOTSPOT_STARTING:
        case NetworkStatus::HOTSPOT_RUNNING:
            interval = 500; // Very slow flash for hotspot
            break;
        default:
            // LED is off
            break;
    }

    if (solid_on) {
        digitalWrite(LED_PIN, HIGH);
    } else if (interval > 0) {
        if (millis() - lastToggleTime > interval) {
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
            lastToggleTime = millis();
        }
    } else {
        digitalWrite(LED_PIN, LOW);
    }
}

 