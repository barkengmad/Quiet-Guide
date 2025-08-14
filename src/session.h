#ifndef SESSION_H
#define SESSION_H

enum class SessionState {
    IDLE,
    DEEP_BREATHING,
    BREATH_HOLD,
    RECOVERY,
    SILENT,
    CUSTOM_RUNNING,
    BOX_RUNNING,
    FOURSEVENEIGHT_RUNNING,
    RESONANT_RUNNING,
    DYNAMIC_TEACHING,
    DYNAMIC_GUIDED,
    BOOTING
};

// Patterns
enum class BreathingPattern : int {
    WIM_HOF = 1,
    BOX = 2,
    FOUR_SEVEN_EIGHT = 3,
    RESONANT = 4,
    CUSTOM = 5,
    DYNAMIC = 6
};

void setupSession();
void loopSession();
void finishBooting();
SessionState getCurrentState();
int getCurrentSessionRound();
int getTotalRounds();
bool shouldPreventDeepSleep();
// Reload in-memory session config from storage after web saves
void reloadSessionConfig();

#endif // SESSION_H 