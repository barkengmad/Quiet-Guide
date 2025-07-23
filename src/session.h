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

void setupSession();
void loopSession();
void finishBooting();
SessionState getCurrentState();

#endif // SESSION_H 