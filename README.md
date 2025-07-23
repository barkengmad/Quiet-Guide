# Meditation Timer - ESP32 Based Standalone Device

A minimalist, standalone meditation aid that guides breathing sessions over configurable rounds, tracks breath hold durations, and logs session data. The device provides haptic feedback through vibration cues and allows single-button interaction, with Wi-Fi connectivity for configuration and session log review.

## Testing Status Legend
- ✅ **Implemented & Tested** - Feature works as expected
- ❌ **Implemented & Failed** - Feature exists but has issues
- ⏳ **Implemented & Untested** - Feature exists but not yet tested
- 🔄 **Planned** - Feature in roadmap, not yet implemented
- ➡️ **In Progress** - Currently being developed/debugged

*To type these symbols: Copy from this legend or use GitHub's emoji shortcuts (:white_check_mark:, :x:, :hourglass:, :arrows_counterclockwise:, :arrow_right:)*

## Hardware Requirements

### Core Components
- **ESP32 Development Board** (tested with ESP32-WROOM-32)
- **18650 Li-ion Battery** (for standalone operation with deep sleep support)
- **Momentary Push Button** (4-pin tactile switch recommended)
- **Vibration Motor** (requires PWM-capable pin)

### Pin Configuration
- **Button Input:** GPIO 25 (with internal pull-up resistor)
- **Vibration Motor:** GPIO 26 (PWM output)
- **Status LED:** GPIO 2 (onboard LED)

### Wiring
- Button: One leg to GPIO 25, other leg to GND
- Vibration Motor: Connected to GPIO 26 via appropriate driver circuit
- All components use 3.3V logic levels

## Current Features

### Core Meditation Functionality

#### **Boot Sequence**
- ⏳ Automatic Wi-Fi connection (5-second timeout)
- ⏳ NTP time synchronization with fallback
- ⏳ Visual LED feedback during connection
- ⏳ Vibration confirmation of device readiness
- ⏳ Automatic transition to idle state

#### **Idle State Operation**
- ✅ **Short Press:** Cycles through round count (1→2→3→4→5→1...)
- ✅ **Long Press (2+ seconds):** Starts meditation session
- ✅ **Feedback:** Vibration pulses equal to selected round count
- ✅ **Auto Sleep:** Deep sleep after 3 minutes of inactivity
- ⏳ **Wake-up:** Any button press wakes device from deep sleep

#### **Session Flow**
Each round consists of three phases:

1. **Deep Breathing Phase**
   - ✅ Default: 30 seconds (configurable)
   - ✅ Vibration: Round number pulses at start
   - ✅ **End Options:** Button press OR automatic timeout
   - ⏳ **Timeout Signal:** Long vibration pulse

2. **Breath Hold Phase**
   - ✅ User-controlled duration
   - ✅ Vibration: Short pulse at start
   - ✅ **End:** Button press (duration logged)

3. **Recovery Phase**
   - ✅ Default: 10 seconds (configurable)
   - ✅ Vibration: Short pulse at start
   - ⏳ **End Options:** Button press OR automatic timeout
   - ⏳ **Timeout Signal:** Long vibration pulse

#### **Silent Meditation Phase**
- ⏳ Automatically starts after final round
- ⏳ Optional reminder pulses (configurable interval)
- ⏳ Maximum duration: 30 minutes (configurable)
- ⏳ **End:** Button press OR timeout
- ⏳ Complete session logged to device memory

#### **Session Abort**
- ⏳ **Long Press (2+ seconds)** during any active session
- ⏳ Distinct long vibration confirmation
- ⏳ All session data discarded
- ⏳ Return to idle state

### Data Management

#### **Configuration Storage**
- ⏳ Persistent storage in EEPROM
- ⏳ Survives power cycles and deep sleep
- ⏳ Automatic default value initialization

#### **Session Logging**
- ⏳ JSON format storage in SPIFFS
- ⏳ Timestamp-based entries (real or estimated time)
- ⏳ Complete session data including:
  - ⏳ Date and start time
  - ⏳ Individual round durations (deep breathing, hold, recovery)
  - ⏳ Silent phase duration
  - ⏳ Total session time

#### **Example Log Entry**
```json
{
  "date": "2025-05-15",
  "start_time": "10:00:00",
  "rounds": [
    {"deep": 32, "hold": 44, "recover": 11},
    {"deep": 30, "hold": 48, "recover": 10},
    {"deep": 31, "hold": 51, "recover": 10}
  ],
  "silent": 758,
  "total": 957
}
```

### Power Management
- ⏳ **Deep Sleep Mode:** Automatic after 3 minutes of inactivity
- ⏳ **Wake Sources:** Button press (GPIO interrupt)
- ⏳ **Time Persistence:** RTC memory maintains time estimation across sleep cycles
- ⏳ **Battery Optimized:** Non-blocking code architecture minimizes active current

### Network Features
- ⏳ **Wi-Fi Connection:** Automatic with fallback
- ⏳ **NTP Time Sync:** Real-time clock synchronization
- ⏳ **LED Status Indicators:**
  - ⏳ Slow flash: Connecting to Wi-Fi
  - ⏳ Fast flash: Syncing time
  - ⏳ Solid on: Connected and synced
  - ⏳ Off: Connection failed
- ⏳ **Offline Operation:** Full functionality without network

### Modular Code Architecture
- ✅ **Clean Separation:** Hardware abstraction, configuration, session logic, networking
- ✅ **Non-blocking Design:** Interrupt-free, polling-based for reliability
- ✅ **Extensible:** Ready for additional breathing modes or features
- ✅ **Debug Support:** Serial output for development and troubleshooting

## Configuration

### **Default Settings**
- **Max Rounds:** 5
- **Deep Breathing Duration:** 30 seconds
- **Recovery Duration:** 10 seconds
- **Idle Timeout:** 3 minutes
- **Silent Phase Maximum:** 30 minutes
- **Silent Reminder:** Disabled (10-minute interval when enabled)

### **User Configuration**
Current round count is adjustable via button interface (1-5 rounds).

## Setup Instructions

1. **Hardware Assembly:** Wire components according to pin configuration
2. **WiFi Credentials:** Edit `src/secrets.h` with your network details
3. **Upload Code:** Use PlatformIO to compile and upload
4. **First Boot:** Device will attempt network connection and initialize defaults
5. **Operation:** Use button interface to select rounds and start sessions

## Roadmap

### 🔄 **Web Interface** (In Development)
The web configuration interface is currently being implemented. The device will display the IP address when connected to WiFi. **Planned features:**

- 🔄 **Configuration Page:** Adjust all timing parameters via web browser
- 🔄 **Session Review:** View and download session logs in readable format
- 🔄 **Log Download:** JSON export of complete session history
- 🔄 **Settings Management:**
  - 🔄 Deep breathing phase duration (10-300 seconds)
  - 🔄 Recovery phase duration (5-120 seconds)
  - 🔄 Silent reminder settings (enable/disable and interval)
  - 🔄 Idle timeout configuration (1-60 minutes)
  - 🔄 Maximum rounds setting (1-10 rounds)

**Current Status:** Core meditation functionality is fully working. Web interface will be added in the next update using a more compatible web server library.

### 🔄 **Multiple Breathing Modes**
The architecture supports additional breathing patterns:

- 🔄 **Box Breathing:** Equal timing for inhale, hold, exhale, hold
- 🔄 **4-7-8 Technique:** Structured count-based breathing
- 🔄 **Custom Patterns:** User-defined breathing sequences

### 🔄 **Enhanced Features**
- 🔄 **Calendar View:** Visual session history in web interface
- 🔄 **Progress Tracking:** Session frequency and duration trends
- 🔄 **Export Options:** CSV/JSON data export for external analysis

## Testing Checklist

### Hardware Tests
- ⏳ **Button Response:** Short and long press detection
- ⏳ **Vibration Motor:** All pulse patterns and durations
- ⏳ **LED Indicators:** Boot sequence and network status
- ⏳ **Power Consumption:** Deep sleep current draw
- ⏳ **Wake-up Function:** Button press from deep sleep

### Functional Tests
- ⏳ **Complete 1-Round Session:** Deep breathing → Hold → Recovery → Silent → Idle
- ⏳ **Complete Multi-Round Session:** Test with 2-5 rounds
- ⏳ **Session Abort:** Long press during active session
- ⏳ **Timeout Behavior:** Automatic transitions after configured times
- ⏳ **Round Count Selection:** Cycling through 1-5 rounds with feedback
- ⏳ **Configuration Persistence:** Settings survive power cycle
- ⏳ **Session Logging:** JSON data correctly stored and retrievable

### Network Tests
- ⏳ **Wi-Fi Connection:** Successful connection to configured network
- ⏳ **NTP Synchronization:** Time correctly synced from internet
- ⏳ **Offline Operation:** Full functionality without network
- ⏳ **Network Recovery:** Reconnection after network outage

### Edge Case Tests
- ⏳ **Rapid Button Presses:** Debouncing effectiveness
- ⏳ **Extended Sessions:** Long silent phases and timeout behavior
- ⏳ **Power Loss Recovery:** State restoration after unexpected shutdown
- ⏳ **Memory Limits:** Logging capacity and overflow handling

## Technical Notes

### **Code Structure**
```
src/
├── main.cpp           # Application entry point and main loop
├── config.h          # Pin assignments and default values
├── secrets.h         # WiFi credentials (gitignored)
├── session.cpp/h     # Core state machine and button handling
├── vibration.cpp/h   # PWM motor control
├── storage.cpp/h     # EEPROM config and SPIFFS logging
├── network.cpp/h     # WiFi and NTP functionality
├── rtc_time.cpp/h    # Time management and RTC persistence
└── webserver.cpp/h   # Web interface (simplified/disabled)
```

### **Memory Usage**
- **RAM:** ~14% utilization
- **Flash:** ~62% utilization
- **Storage:** EEPROM for config, SPIFFS for session logs

### **Build Requirements**
- **Platform:** PlatformIO with ESP32 Arduino framework
- **Dependencies:** ArduinoJson library
- **Target:** ESP32-WROOM-32 compatible boards

---

**Version:** 1.0.0  
**Platform:** ESP32 Arduino Framework  
**License:** Open Source  
**Development Status:** Core functionality complete, web interface pending restoration 