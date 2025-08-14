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
- ⏳ Automatic Wi-Fi connection (10-second timeout)
- ⏳ NTP time synchronization with fallback
- ⏳ Visual LED feedback during connection
- ⏳ Vibration confirmation of device readiness
- ⏳ Automatic transition to idle state

#### **Idle State Operation**
- ✅ **Short Press:** Cycles through round count (1→2→3→4→5→1...)
- ✅ **Smart Round Selection:** 1-second delay after button press allows multiple rapid presses without pulse interference
- ✅ **Long Press (2+ seconds):** Starts meditation session
- ✅ **Feedback:** Vibration pulses equal to selected round count (delayed for smooth interaction)
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
- ✅ **Persistent Storage:** EEPROM-based configuration (tested and verified)
- ✅ **Power Cycle Survival:** Settings survive power cycles and deep sleep
- ✅ **Default Initialization:** Automatic default value setup on first boot
- ✅ **Web Interface Integration:** Real-time saving and loading via browser

#### **Session Logging**
- ✅ JSON format storage in SPIFFS
- ✅ Timestamp-based entries (real or estimated time)
- ✅ Complete session data including:
  - ✅ Date and start time
  - ✅ Pattern metadata (pattern_id, pattern_name)
  - ✅ Pattern settings snapshot (e.g., boxSeconds, guidedMinutes, per‑pattern silentAfter)
  - ✅ Individual round durations where applicable (Wim Hof)
  - ✅ Silent phase duration
  - ✅ Total session time
  - ✅ UI formatting: non–Wim Hof sessions display as “Rounds: 1” with a compact R1 summary (e.g., `R1: Box=3s Duration=2m00s`)

#### **Example Log Entry**
```json
{
  "date": "2025-05-15",
  "start_time": "10:00:00",
  "pattern_id": 2,
  "pattern_name": "Box",
  "settings": {
    "boxSeconds": 3,
    "guidedMinutes": 10,
    "silentAfter": true,
    "silentPhaseMaxMinutes": 30,
    "silentReminderEnabled": false,
    "silentReminderIntervalMinutes": 10
  },
  "silent": 30,
  "total": 150
}
```

UI representation for the above (on `/logs`):

R1: Box=3s Duration=2m00s

### Power Management
- ⏳ **Deep Sleep Mode:** Automatic after 3 minutes of inactivity
- ⏳ **Wake Sources:** Button press (GPIO interrupt)
- ⏳ **Time Persistence:** RTC memory maintains time estimation across sleep cycles
- ⏳ **Battery Optimized:** Non-blocking code architecture minimizes active current

### Network Features

#### **WiFi Provisioning System**
- ✅ **Automatic Connection:** Attempts to connect to saved WiFi credentials on boot
- ✅ **Hotspot Fallback:** Creates "MeditationTimer-Setup" hotspot if connection fails
- ✅ **Manual Hotspot Mode:** Hold button during boot to force setup mode
- ✅ **Web-Based Setup:** Complete WiFi configuration via browser interface
- ✅ **Network Scanning:** Lists available WiFi networks for easy selection
- ✅ **Credential Storage:** EEPROM-based persistence across reboots

#### **IP Address Communication**
- ✅ **Vibration Notification:** Device vibrates IP address after successful WiFi setup
- ✅ **Last Octet Pattern:** Communicates final IP segment (e.g., .165)
- ✅ **Vibration Sequence:**
  - Long buzz (1s) → Start signal
  - Short buzzes per digit (1 buzz = 1, 2 buzzes = 2, etc.)
  - 10 buzzes = digit 0
  - Long pauses between digits (1.6s)
  - Long buzz (1s) → End signal
- ✅ **Example:** IP ending in .165 = Long buzz → 1 buzz → pause → 6 buzzes → pause → 5 buzzes → Long buzz
- ✅ **Current Rounds:** 3-second pause followed by session round count vibration

#### **Network Status & Connectivity**
- ✅ **NTP Time Sync:** Real-time clock synchronization with 5-second timeout
- ✅ **LED Status Indicators:**
  - Slow flash: Connecting to Wi-Fi
  - Fast flash: Syncing time / Hotspot mode
  - Solid on: Connected and synced
  - Off: Connection failed
- ✅ **Connection Timeouts:** 10-second WiFi timeout, automatic hotspot fallback
- ✅ **Offline Operation:** Full meditation functionality without network
- ✅ **Web Interface Access:** Available at device IP when connected to WiFi

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
2. **Upload Code:** Use PlatformIO to compile and upload
3. **WiFi Setup:** Device automatically creates "MeditationTimer-Setup" hotspot on first boot
4. **Configure Network:** 
   - Connect to the hotspot from any device
   - Navigate to `http://192.168.4.1/wifi-setup`
   - Select your WiFi network and enter password
   - Device will restart and connect to your network
5. **IP Address:** Device vibrates the last part of its IP address after connection
6. **Web Access:** Use the vibrated IP to access web interface (e.g., `http://10.10.10.165`)
7. **Training Mode:** Enable Wim Hof Training in the web dashboard for learning the device

### **Getting Started with Training Mode**

For first-time users, Training Mode provides an excellent way to understand the meditation flow:

1. **Connect to Web Interface:** Open the device's IP address in your browser
2. **Enable Wim Hof Training:** Click the "Enable Wim Hof Training" button on the dashboard
3. **Watch Real-time Updates:** See detailed explanations for each phase as you progress
4. **Learn the Process:** Understand what each vibration pattern means and what to expect
5. **Disable When Ready:** Turn off Training Mode for distraction-free meditation sessions

Training Mode provides educational content explaining the science and technique behind each phase, making it perfect for learning the Wim Hof breathing method. Note: training guidance currently covers the Wim Hof flow.

### **Understanding IP Address Vibration**

After WiFi setup, the device communicates its IP address through vibration patterns so you can access the web interface without needing a display:

#### **Vibration Pattern Guide:**
- **Long buzz (1 second):** Start of IP notification
- **Short buzzes (300ms each):** Each digit (count the buzzes)
  - 1 buzz = digit 1
  - 2 buzzes = digit 2
  - ...
  - 10 buzzes = digit 0 (to distinguish from no buzz)
- **Long pause (1.6 seconds):** Between each digit
- **Long buzz (1 second):** End of IP notification
- **3-second pause:** Separation before round count
- **Session rounds:** Current device round setting

#### **Example:** 
For IP address `10.10.10.165`, you'll hear:
```
Long buzz → 1 buzz → pause → 6 buzzes → pause → 5 buzzes → Long buzz
    ↓ (3 second pause) ↓
Current round count (e.g., 3 buzzes for 3 rounds)
```

**Tip:** Have a pen ready to write down the digits as the device vibrates them!

## Roadmap

### ✅ **Web Interface** (Implemented & Tested)
The web configuration interface is fully functional. When connected to WiFi, the device displays its IP address for browser access.

#### **Dashboard Features:**
- ✅ **Real-time Status:** Current device state and selected round count
- ✅ **Training Mode:** Educational descriptions for each meditation phase
- ✅ **Settings Overview:** All current configuration values displayed
- ✅ **Breathing Patterns Manager:** Drag to reorder, radio select current, include/exclude per type, and per‑pattern silent toggle
- ✅ **WiFi Setup Page:** Complete network configuration with:
  - Network scanning and selection
  - Password entry with proper URL decoding
  - IP vibration pattern explanation
  - Connection status feedback

#### **Training Mode Features:**
- ✅ **Toggle Button:** Enable/disable training mode with one click
- ✅ **Real-time Updates:** Status updates every second when enabled
- ✅ **Educational Content:** Detailed explanations for each phase:
  - 🏠 **IDLE:** Round selection and session start instructions
  - 🫁 **DEEP BREATHING:** Hyperventilation technique and benefits
  - 🛑 **BREATH HOLD:** Mammalian dive reflex and CO2 tolerance training
  - 💨 **RECOVERY:** Integration breath technique and purpose
  - 🧘 **SILENT MEDITATION:** Inner awareness and heightened state benefits
- ✅ **Performance Optimized:** Zero CPU overhead when disabled
- ✅ **Persistent Setting:** Browser remembers preference across sessions

#### **Configuration Management:**
- ✅ **Settings Page:** Adjust all timing parameters via web browser
- ✅ **Form Validation:** Input ranges enforced (tested and verified)
- ✅ **Persistent Storage:** All settings save correctly to EEPROM
- ✅ **Real-time Updates:** Changes immediately reflected in interface
- ✅ **Settings Management:**
  - ✅ Deep breathing phase duration (10-300 seconds)
  - ✅ Recovery phase duration (5-120 seconds)
  - ✅ Silent reminder settings (enable/disable and interval)
  - ✅ Idle timeout configuration (1-60 minutes)
  - ✅ Maximum rounds setting (1-10 rounds)

#### **Session Log Management:**
- ✅ **Session Review:** View complete session history with formatted display
- ✅ **JSON Download:** Export complete session history
- ✅ **Individual Session Deletion:** Delete specific sessions with confirmation
- ✅ **Bulk Deletion:** Delete all sessions with "Are you sure?" confirmation
- ✅ **Session Data Display:**
  - ✅ Date and time stamps
  - ✅ Round-by-round durations (deep breathing, breath hold, recovery)
  - ✅ Silent meditation duration
  - ✅ Total session time
  - ✅ Human-readable time formatting (e.g., "2m 15s")

**Current Status:** Web interface is fully functional and extensively tested. All features work reliably across different browsers.

### ✅ **Moon Cycle Page**
Visual tracker for meditations across the current lunar cycle:

- ✅ Circular visualization of ~30 days with yellow dots for completed days
- ✅ “Today” highlighted with a blue ring
- ✅ Large center moon icon reflecting lunar phase (new → full → new)
- ✅ Progress summary and cycle date range

### 🔄 **Multiple Breathing Modes**
Implemented and selectable via long-press in IDLE, with haptic confirmation:

- ✅ **Wim Hof**: Rounds-based (value buzzes)
- ✅ **Box**: Equal seconds (2–8) for each phase (value buzzes)
- ✅ **4·7·8**: Inhale 4s, hold 7s, exhale 8s (type-only buzz)
- ✅ **Resonant (6:6)**: Inhale 6s, exhale 6s (type-only buzz)
- ✅ **Custom (Timed Prompts)**: User-defined seconds per phase (type-only buzz)
- ✅ **Dynamic (Tap-to-Teach)**: Learn inhale/exhale cadence by tapping (type-only buzz)

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

### **Recent Improvements**
- **Added:** Complete WiFi provisioning system with hotspot fallback
- **Implemented:** IP address vibration notification for screenless setup
- **Enhanced:** WiFi setup page with network scanning and clear instructions
- **Fixed:** URL decoding for special characters in WiFi passwords
- **Added:** Wim Hof Training Mode with real-time educational content
- **Enhanced:** Round selection with 1-second delay for smooth interaction
- **Improved:** Session log management with individual and bulk deletion
- **Verified:** All configuration settings and WiFi credentials persist correctly
- **New:** Breathing Patterns management on Dashboard (drag to reorder, radio select, include/exclude per type)
- **New:** Pattern order persisted and used in haptic rotation; radios persist via web UI
- **Fix:** currentPatternId POST parsing (off-by-one) and URL-decoding robustness
- **Change:** Value buzzes only for Wim Hof and Box; others are type-only, including boot/wake and confirmations
- **New:** Logs show pattern name; non–Wim Hof sessions display “Rounds: 1” with compact `R1: … Duration=..` line
- **New:** Moon Cycle page with circular progress and center moon
- **UI:** Dashboard list layout: drag handle at left, then radio, then `[n] Name` for clarity

---

**Version:** 1.1.1  
**Platform:** ESP32 Arduino Framework  
**License:** Open Source  
**Development Status:** Core functionality and web interface complete. Training mode and session management fully implemented and tested. 