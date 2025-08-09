#ifndef SESSION_H
#define SESSION_H

enum class SessionState {
    IDLE,
    DEEP_BREATHING,
    BREATH_HOLD,
    RECOVERY,
    SILENT,
    BOOTING
};

// Patterns
enum class BreathingPattern : int {
    WIM_HOF = 1,
    BOX = 2,
    FOUR_SEVEN_EIGHT = 3,
    RESONANT = 4
};

void setupSession();
void loopSession();
void finishBooting();
SessionState getCurrentState();
int getCurrentSessionRound();
int getTotalRounds();
bool shouldPreventDeepSleep();

#endif // SESSION_H 