#ifndef NETWORK_H
#define NETWORK_H

void setupNetwork();
void loopNetwork();
bool isWifiConnected();
void handleLedIndicator();
void startHotspotMode();
void checkBootButtonForHotspot();
bool isHotspotMode();

#endif // NETWORK_H 