Submission to the Telegram VoIP Contest

INTRO
litgvoip is a VoIP library based on WebRTC framework (release version M81).
The library interface is declared at the TgVoip class (TgVoip.h file) provided
by the Contest organizers.

SERVERS
Signaling and TURN servers must be running prior libtgvoip depended applications
execution.
Signaling server runs (disabled now) on x.x.x.x:8080 for a few next weeks. This URL is
hardcoded at tgvoip/tgvoip/TgVoip.cpp, line 74.
Signaling server's source code can be found here - tgvoip/tgwss
TURN server (coturn, version 4.5.0.7) runs on x.x.x.x:3478 (disabled now) for a few next
weeks. This URL as well as user's name and password are hardcoded at
tgvoip/tgvoip/webRTCPeer.cpp, lines 207 - 209.

BUILD INSTRUCTION
libtgvoip build instruction can be found at tgvoip/README.md file.

