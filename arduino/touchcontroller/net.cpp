#include "net.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"
#include "settings.h"
#include "wallclock.h"

#if __has_include("Auth.h")
#include "Auth.h"
#else
#warning "Please create Auth.h with WIFI_SSID & WIFI_PASS defined"
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#endif

/*
 * The controller side of the protocol, a port of UDPSender, UDPServer,
 * DisplayRegistry and SessionState of the android app.
 *
 * Udp gives no guarantees, so every command carries a sequence number, is
 * repeated until each display has acknowledged it and is dropped by the
 * display when it has already been carried out. Every packet also carries how
 * long the command has been under way ("sd"), and the displays start at
 * (key press + lead time) - a command that only gets through on the third
 * attempt still starts the countdown at the same instant everywhere.
 */

namespace {

// copies per attempt - cheap insurance against a single lost packet
const uint8_t COPIES = 2;
const uint16_t RETRY_INTERVAL = 200;
const uint8_t MAX_ATTEMPTS = 6;
const uint8_t MAX_PENDING = 12;
const uint16_t BODY_MAX = 400;

// a display counts as gone after roughly three missed heartbeats
const uint32_t OFFLINE_TIMEOUT = 10000;
// a hello is sent three times in a row, only the first one is acted upon
const uint32_t ANNOUNCE_DEBOUNCE = 2000;
const uint32_t SWEEP_INTERVAL = 1000;
// as long as no display has answered, keep asking
const uint32_t DISCOVER_INTERVAL = 10000;

// How long a display that needs a resync waits for the controller to learn
// the current state. The displays send a heartbeat every three seconds, and a
// running pass reports every second.
const uint32_t STATE_REFRESH_TIMEOUT = 5000;
// a running pass reports every second, so this is a long silence
const uint32_t STATE_SILENCE = 4000;

// How far the clock of a display may be off before it is set. A heartbeat is
// a broadcast and can sit in the access point for a few hundred ms.
const int64_t CLOCK_TOLERANCE = 2000;
// a display is sent the time at most this often
const uint32_t CLOCK_SYNC_INTERVAL = 10000;
// anything before 2023 is not a real time
const uint64_t MIN_VALID_EPOCH_MS = 1700000000000ULL;

// The sequence number is kept in flash and moved on by this much at every
// start, so it never falls back behind a number a display has already seen.
const uint64_t SEQ_RESTART_GAP = 100000;

const uint32_t NET_BACKOFF_MIN = 1000;
const uint32_t NET_BACKOFF_MAX = 30000;
const uint32_t NET_CONNECT_TIMEOUT = 15000;

const char* PREFS_NAMESPACE = "touchctl";

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

enum NetState { NetOffline,
                NetConnecting,
                NetOnline };

enum Reason { ReasonNone,
              ReasonNew,       // heard from for the first time
              ReasonRejoined,  // was out of reach and is back
              ReasonRebooted   // restarted and lost everything it knew
};

struct Display {
  bool used;
  // the id of the display, or its address when the id is unusable
  char key[16];
  // the id the display reported, empty when it did not report a usable one
  char deviceId[8];
  uint8_t displayNo;
  IPAddress ip;
  bool online;
  // what the display itself said it is doing
  bool running;
  unsigned long lastSeen;
  unsigned long lastAnnounced;
  unsigned long lastClockSync;
};

struct Pending {
  bool used;
  uint64_t seq;
  char command[16];
  // the message without its closing brace - the age is added per attempt
  char body[BODY_MAX];
  bool unicast;
  IPAddress host;
  unsigned long startTime;
  // one bit per display that has not confirmed yet
  uint16_t missing;
  uint8_t attempts;
  unsigned long nextAttemptAt;
};

/**
 * What the controller believes the displays are currently doing. A display
 * that rejoins in the middle of a pass gets this state pushed to it.
 */
struct Session {
  char viewName[12] = "tournament";
  // end of the lunch break as shown by the lunch view, "HH:MM"
  char lunchTime[6] = "";
  uint32_t lunchEnd = 0;
  bool running = false;
  bool hold = false;
  // the safety stop - a display that joins has to show the warning as well
  bool emergency = false;
  bool prepare = true;
  uint16_t remainingSeconds = 0;
  uint8_t currentGroup = 1;
  uint16_t currentPass = 1;
  uint8_t groupSet = 0;
  // True while the controller cannot know what the displays are doing: after
  // its own start and after the wifi was gone. A stale state must never be
  // sent as a resync - it is rebuilt from the displays first.
  bool stale = true;
  unsigned long lastStateAt = 0;
};

NetState netState = NetOffline;
unsigned long netNextAttempt = 0;
unsigned long netAttemptStarted = 0;
uint32_t netBackoff = NET_BACKOFF_MIN;
bool wifiActive = false;
bool udpStarted = false;

WiFiUDP udp;
JsonDocument doc;
char receiveBuffer[1024];

Display displays[MAX_DISPLAYS];
Pending pending[MAX_PENDING];
Session session;
NetEventHandler eventHandler = NULL;

uint64_t lastSeq = 0;
uint64_t savedSeq = 0;

unsigned long sweepNextTime = 0;
unsigned long discoverNextTime = 0;

// displays that asked to be brought up to date while the controller did not
// know what was going on
uint16_t pendingSync = 0;
unsigned long pendingSyncDeadline = 0;

// ---------------------------------------------------------------------------
// sequence numbers
// ---------------------------------------------------------------------------

void saveSeq() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putULong64("seq", lastSeq);
  prefs.end();
  savedSeq = lastSeq;
}

void seqBegin() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true);
  lastSeq = prefs.getULong64("seq", 0) + SEQ_RESTART_GAP;
  prefs.end();
  saveSeq();
}

/**
 * The displays only carry out a command that is newer than the last one. The
 * android app numbers by its clock, so with a clock the controller does the
 * same and the two stay comparable. Without one the number just counts up,
 * from a start value that survives a restart.
 */
uint64_t nextSeq() {
  uint64_t byClock = clockValid() ? clockNowMs() : 0;
  lastSeq = max(lastSeq + 1, byClock);
  if (lastSeq - savedSeq > SEQ_RESTART_GAP / 2) {
    saveSeq();
  }
  return lastSeq;
}

// ---------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------

/**
 * True when the controller cannot trust what it knows: either it was not
 * listening, or it believes a pass is running while the display that reports
 * has gone quiet - silence means the pass has ended and the message was lost.
 */
bool needsRefresh() {
  return session.stale || (session.running && millis() - session.lastStateAt > STATE_SILENCE);
}

/** called for every command the controller sends out */
void sessionOnCommandSent(JsonDocument& d) {
  const char* command = d["cmd"] | "";
  if (strcmp(command, "change_view") == 0) {
    // the displays ignore a view change while a pass is running, so the
    // controller must not record one either
    if (!session.running) {
      strlcpy(session.viewName, d["v"] | session.viewName, sizeof(session.viewName));
      const char* time = d["val"]["t"] | "";
      if (time[0] != 0) {
        strlcpy(session.lunchTime, time, sizeof(session.lunchTime));
        session.lunchEnd = d["val"]["e"] | (uint32_t)0;
      }
      session.hold = false;
      session.prepare = true;
      session.remainingSeconds = 0;
    }
  } else if (strcmp(command, "run") == 0) {
    // the user just defined the state, so it is known again
    session.stale = false;
    session.lastStateAt = millis();
    session.running = true;
    session.hold = false;
  } else if (strcmp(command, "pause") == 0) {
    session.hold = true;
  } else if (strcmp(command, "emergency") == 0) {
    // holds the pass, it does not end it
    session.emergency = d["val"]["e"] | false;
  } else if (strcmp(command, "stop") == 0 || strcmp(command, "reset") == 0) {
    session.stale = false;
    session.running = false;
    session.hold = false;
    session.prepare = true;
    session.remainingSeconds = 0;
    session.currentGroup = 1;
    session.currentPass = 1;
    session.groupSet = 0;
  }
}

/**
 * Called for every state packet of the reporting display - and for its
 * heartbeats, which carry the same fields. Whatever the display reports is
 * the truth, so the controller is up to date again afterwards.
 */
void sessionOnStateReceived() {
  session.stale = false;
  session.lastStateAt = millis();
  session.currentGroup = doc["cg"] | session.currentGroup;
  session.currentPass = doc["cp"] | session.currentPass;
  session.groupSet = doc["gs"] | session.groupSet;
  uint16_t remainingPrepare = doc["cpt"] | 0;
  uint16_t remainingAction = doc["cat"] | 0;
  session.prepare = remainingPrepare > 0;
  session.remainingSeconds = session.prepare ? remainingPrepare : remainingAction;
  if (!doc["ph"].isNull()) {
    session.running = (doc["ph"] | 0) == 1;
  }
  if (!doc["h"].isNull()) {
    session.hold = doc["h"] | session.hold;
  }
  if (!doc["em"].isNull()) {
    session.emergency = doc["em"] | session.emergency;
  }
}

void sessionOnDisplayStopped() {
  session.stale = false;
  session.lastStateAt = millis();
  session.running = false;
  session.hold = false;
  session.remainingSeconds = 0;
}

// ---------------------------------------------------------------------------
// display registry
// ---------------------------------------------------------------------------

/**
 * Displays are told apart by the id they report. A firmware that cannot read
 * its mac would send the same id on every board, so an unusable id falls back
 * to the network address - two boards must never collapse into one entry.
 */
bool isUsableId(const char* id) {
  if (id == NULL || id[0] == 0) {
    return false;
  }
  for (const char* c = id; *c != 0; c++) {
    if (*c != '0') {
      return true;
    }
  }
  return false;
}

int findDisplay(const char* key) {
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if (displays[i].used && strcmp(displays[i].key, key) == 0) {
      return i;
    }
  }
  return -1;
}

/** a free entry, or the one of the display that has been gone the longest */
int allocateDisplay(const char* key) {
  int index = -1;
  for (uint8_t i = 0; i < MAX_DISPLAYS && index < 0; i++) {
    if (!displays[i].used) {
      index = i;
    }
  }
  if (index < 0) {
    unsigned long now = millis();
    unsigned long longest = 0;
    for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
      if (!displays[i].online && now - displays[i].lastSeen >= longest) {
        longest = now - displays[i].lastSeen;
        index = i;
      }
    }
  }
  if (index < 0) {
    return -1;
  }
  unsigned long now = millis();
  Display& d = displays[index];
  d.used = true;
  strlcpy(d.key, key, sizeof(d.key));
  d.deviceId[0] = 0;
  d.displayNo = 1;
  d.ip = IPAddress(0, 0, 0, 0);
  d.online = true;
  d.running = false;
  d.lastSeen = now;
  d.lastAnnounced = now - ANNOUNCE_DEBOUNCE;
  d.lastClockSync = now - CLOCK_SYNC_INTERVAL;
  pendingSync &= ~(1 << index);
  return index;
}

uint16_t onlineMask() {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if (displays[i].used && displays[i].online) {
      mask |= 1 << i;
    }
  }
  return mask;
}

/**
 * The display that reports the tournament state. With n displays only one
 * may drive the controller ui, otherwise the values overwrite each other.
 * The lowest display number wins, ties are broken by the id.
 */
int reportingDisplay() {
  int best = -1;
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if (!displays[i].used || !displays[i].online) {
      continue;
    }
    if (best < 0 || displays[i].displayNo < displays[best].displayNo
        || (displays[i].displayNo == displays[best].displayNo && strcmp(displays[i].key, displays[best].key) < 0)) {
      best = i;
    }
  }
  return best;
}

bool mayReportState(int index) {
  int reporting = reportingDisplay();
  return reporting < 0 || reporting == index;
}

// ---------------------------------------------------------------------------
// sending
// ---------------------------------------------------------------------------

IPAddress subnetBroadcast() {
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  return IPAddress(ip[0] | (uint8_t)~mask[0], ip[1] | (uint8_t)~mask[1], ip[2] | (uint8_t)~mask[2], ip[3] | (uint8_t)~mask[3]);
}

void transmit(const char* body, bool unicast, IPAddress host, unsigned long startTime) {
  if (!wifiActive || !udpStarted) {
    return;
  }
  // stamped per attempt: a later attempt reports a larger age, so every
  // display still arrives at the same absolute start instant
  char packet[BODY_MAX + 32];
  int length = snprintf(packet, sizeof(packet), "%s,\"sd\":%lu}", body, millis() - startTime);
  if (length <= 0 || length >= (int)sizeof(packet)) {
    return;
  }

  IPAddress targets[2];
  uint8_t targetCount = 0;
  if (unicast) {
    targets[targetCount++] = host;
  } else {
    targets[targetCount++] = IPAddress(255, 255, 255, 255);
    // some access points drop the all-ones broadcast but forward the directed
    // one, so both are used
    IPAddress directed = subnetBroadcast();
    if (directed != targets[0]) {
      targets[targetCount++] = directed;
    }
  }
  for (uint8_t t = 0; t < targetCount; t++) {
    for (uint8_t i = 0; i < COPIES; i++) {
      udp.beginPacket(targets[t], UDP_PORT);
      udp.write((const uint8_t*)packet, length);
      udp.endPacket();
    }
  }
}

Pending* allocatePending() {
  Pending* oldest = &pending[0];
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    if (!pending[i].used) {
      return &pending[i];
    }
    if ((long)(pending[i].startTime - oldest->startTime) < 0) {
      oldest = &pending[i];
    }
  }
  return oldest;
}

/**
 * @param target index of the one display the command is meant for, -1 for all
 * @param startTime millis() of the key press the command belongs to
 * @param reliable repeated until every display it is meant for has confirmed
 */
void dispatch(JsonDocument& d, int target, unsigned long startTime, bool reliable) {
  d["src"] = "controller";
  if (target >= 0 && displays[target].deviceId[0] != 0) {
    d["dst"] = displays[target].deviceId;
  }
  uint64_t seq = reliable ? nextSeq() : 0;
  if (seq != 0) {
    d["seq"] = seq;
  }

  // the session state must follow the command once, not once per repeat
  sessionOnCommandSent(d);

  char body[BODY_MAX];
  if (measureJson(d) >= sizeof(body)) {
    Serial.println("command too long, not sent");
    return;
  }
  size_t length = serializeJson(d, body, sizeof(body));
  body[length - 1] = 0;  // the closing brace, put back together with the age

  bool unicast = target >= 0 && displays[target].ip != IPAddress(0, 0, 0, 0);
  IPAddress host = unicast ? displays[target].ip : IPAddress(0, 0, 0, 0);
  transmit(body, unicast, host, startTime);
  Serial.printf("sent %s}\n", body);

  // Nothing known yet means nothing to wait for - the command still went
  // out, it just cannot be checked.
  uint16_t missing = target >= 0 ? (1 << target) : onlineMask();
  if (seq == 0 || missing == 0 || !wifiActive) {
    return;
  }
  Pending* entry = allocatePending();
  entry->used = true;
  entry->seq = seq;
  strlcpy(entry->command, d["cmd"] | "", sizeof(entry->command));
  strcpy(entry->body, body);
  entry->unicast = unicast;
  entry->host = host;
  entry->startTime = startTime;
  entry->missing = missing;
  entry->attempts = 1;
  entry->nextAttemptAt = millis() + RETRY_INTERVAL;
}

/** a display confirmed that it carried out a command */
void onAck(uint64_t seq, int index) {
  if (seq == 0) {
    return;
  }
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    Pending& entry = pending[i];
    if (!entry.used || entry.seq != seq) {
      continue;
    }
    entry.missing &= ~(1 << index);
    if (entry.missing == 0) {
      entry.used = false;
      if (entry.attempts > 1) {
        Serial.printf("'%s' confirmed by all displays after %d attempts\n", entry.command, entry.attempts);
      }
    }
    return;
  }
}

void retryPending() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    Pending& entry = pending[i];
    if (!entry.used || (long)(now - entry.nextAttemptAt) < 0) {
      continue;
    }
    // a display that is gone will not answer any more
    entry.missing &= onlineMask();
    if (entry.missing == 0) {
      entry.used = false;
      continue;
    }
    if (entry.attempts >= MAX_ATTEMPTS) {
      entry.used = false;
      for (uint8_t n = 0; n < MAX_DISPLAYS; n++) {
        if (entry.missing & (1 << n)) {
          Serial.printf("display %d (%s) did not confirm '%s'\n", displays[n].displayNo, displays[n].key, entry.command);
        }
      }
      continue;
    }
    entry.attempts++;
    entry.nextAttemptAt = now + RETRY_INTERVAL;
    transmit(entry.body, entry.unicast, entry.host, entry.startTime);
  }
}

/** asks every display on the network to announce itself */
void sendDiscover() {
  JsonDocument d;
  d["cmd"] = "discover";
  // discovery is what finds the displays in the first place, so there is
  // nobody who could acknowledge it
  dispatch(d, -1, millis(), false);
}

void sendConfiguration(int target) {
  JsonDocument d;
  d["cmd"] = "config";
  JsonObject values = d["val"].to<JsonObject>();
  values["pt"] = settings.prepareTime;
  values["at"] = settings.arrowTime;
  values["ac"] = settings.arrowCount;
  values["rac"] = settings.reshootArrowCount;
  values["sit"] = settings.shootInTime;
  values["wt"] = settings.warnTime;
  values["g"] = settings.groups;
  values["p"] = settings.passes;
  values["fpl"] = settings.flashingPrepareLight;
  dispatch(d, target, millis(), true);
}

void sendVolume(int target) {
  JsonDocument d;
  d["cmd"] = "volume";
  d["val"]["val"] = settings.muted ? 0 : settings.volume;
  dispatch(d, target, millis(), true);
}

/**
 * Brings a single display back in step with the others - view, phase and the
 * remaining seconds of the pass that is currently running.
 */
void sendResync(int target) {
  JsonDocument d;
  d["cmd"] = "resync";
  JsonObject values = d["val"].to<JsonObject>();
  values["v"] = session.viewName;
  values["r"] = session.running && session.remainingSeconds > 0;
  values["h"] = session.hold;
  values["em"] = session.emergency;
  values["pr"] = session.prepare;
  values["t"] = session.remainingSeconds;
  values["cg"] = session.currentGroup;
  values["cp"] = session.currentPass;
  values["gs"] = session.groupSet;
  // local time minus UTC - the displays know no time zone of their own
  values["tz"] = clockTzOffset();
  if (session.lunchTime[0] != 0) {
    values["lt"] = session.lunchTime;
    values["le"] = session.lunchEnd;
  }
  dispatch(d, target, millis(), true);
}

/**
 * Sets the clock of a display to the time of the controller, in ms since 1970
 * (UTC). The age every packet carries is added by the display, so a repeat
 * that gets through late still sets the right time.
 */
void sendTime(int target) {
  JsonDocument d;
  d["cmd"] = "set_time";
  d["val"]["t"] = clockNowMs();
  dispatch(d, target, millis(), true);
}

// ---------------------------------------------------------------------------
// bringing displays up to date
// ---------------------------------------------------------------------------

void syncDisplay(int index) {
  Display& d = displays[index];
  if (d.running) {
    // A display that is running is the authority on its own countdown. It was
    // started by somebody - the app, or this controller before a restart -
    // and a configuration from here could change the times under its feet.
    Serial.printf("display %d (%s) is running, left alone\n", d.displayNo, d.key);
    return;
  }
  sendConfiguration(index);
  sendVolume(index);
  sendResync(index);
  Serial.printf("display %d (%s) synchronised\n", d.displayNo, d.key);
}

/**
 * A display announced itself. It gets the current configuration and, if a
 * pass is running, the position of the other displays, so it does not stay
 * behind on an old view.
 *
 * As long as the controller does not know what the displays are doing, the
 * display is put on hold until one of them has reported in.
 */
void onDisplayAvailable(int index) {
  if (needsRefresh()) {
    if (pendingSync == 0) {
      pendingSyncDeadline = millis() + STATE_REFRESH_TIMEOUT;
    }
    pendingSync |= 1 << index;
    return;
  }
  syncDisplay(index);
}

/** brings the displays that were waiting for a known state up to date */
void flushPendingSync() {
  if (pendingSync == 0) {
    return;
  }
  if (needsRefresh() && (long)(millis() - pendingSyncDeadline) < 0) {
    return;  // a display may still report in
  }
  if (session.stale) {
    Serial.println("no display reports a state - going on with the own one");
    session.stale = false;
  }
  uint16_t waiting = pendingSync;
  pendingSync = 0;
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if ((waiting & (1 << i)) && displays[i].used && displays[i].online) {
      syncDisplay(i);
    }
  }
}

/** marks displays that stopped sending heartbeats as offline */
void sweep() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    Display& d = displays[i];
    if (d.used && d.online && now - d.lastSeen > OFFLINE_TIMEOUT) {
      d.online = false;
      Serial.printf("display %d (%s) out of reach\n", d.displayNo, d.key);
    }
  }
}

// ---------------------------------------------------------------------------
// receiving
// ---------------------------------------------------------------------------

void notify(const char* command, bool resynced) {
  if (eventHandler != NULL) {
    eventHandler(command, doc, resynced);
  }
}

/**
 * A heartbeat carries what the display is doing. After a start or a gap in
 * the wifi the controller has no idea, so it takes the answer from the
 * display that reports instead of pushing its own outdated view back onto
 * the displays.
 */
void adoptStateFromPresence(int index) {
  if (!needsRefresh() || doc["ph"].isNull()) {
    return;
  }
  if (!mayReportState(index)) {
    return;
  }
  bool resynced = session.stale;
  sessionOnStateReceived();
  Serial.printf("state taken over from display %d\n", displays[index].displayNo);
  // the ui only listens for state packets, so the heartbeat is handed on as one
  notify("state", resynced);
}

/**
 * Every hello and heartbeat carries the clock of the display. A controller
 * without a time takes it from there. One that got its time from the net or
 * from the settings page sets every display that is off; one that only
 * borrowed its time from a display sets the displays that have none.
 */
void keepClockInSync(int index) {
  if (doc["t"].isNull()) {
    return;  // a firmware without a clock
  }
  uint64_t theirs = doc["t"] | (uint64_t)0;
  if (!clockValid()) {
    if (theirs >= MIN_VALID_EPOCH_MS) {
      clockAdopt(theirs);
      Serial.printf("clock taken over from display %d\n", displays[index].displayNo);
    }
    return;
  }
  if (clockSource() == ClockDisplay && theirs != 0) {
    return;
  }
  int64_t drift = (int64_t)theirs - (int64_t)clockNowMs();
  if (drift >= -CLOCK_TOLERANCE && drift <= CLOCK_TOLERANCE) {
    return;
  }
  Display& d = displays[index];
  unsigned long now = millis();
  if (now - d.lastClockSync < CLOCK_SYNC_INTERVAL) {
    return;  // the correction is on its way, give it time to show
  }
  d.lastClockSync = now;
  sendTime(index);
  Serial.printf("clock of display %d is off, time sent\n", d.displayNo);
}

void handlePresence(const char* key, const char* reportedId, IPAddress ip, bool hello) {
  unsigned long now = millis();
  int index = findDisplay(key);
  bool known = index >= 0;
  bool wasOnline = known && displays[index].online;
  if (!known) {
    index = allocateDisplay(key);
    if (index < 0) {
      return;
    }
  }

  Display& d = displays[index];
  strlcpy(d.deviceId, isUsableId(reportedId) ? reportedId : "", sizeof(d.deviceId));
  d.displayNo = doc["dn"] | d.displayNo;
  d.ip = ip;
  d.lastSeen = now;
  d.online = true;
  if (!doc["ph"].isNull()) {
    d.running = (doc["ph"] | 0) == 1;
  }

  // A display only has to be brought up to date when it lost the thread: it
  // is new, it was away, or it restarted. The answer to a discover from a
  // display that has been on air all along says nothing is wrong with it.
  Reason reason = ReasonNone;
  if (!known) {
    reason = ReasonNew;
  } else if (!wasOnline) {
    reason = ReasonRejoined;
  } else if (hello && (doc["boot"] | false)) {
    reason = ReasonRebooted;
  }

  if (reason != ReasonNone && now - d.lastAnnounced >= ANNOUNCE_DEBOUNCE) {
    d.lastAnnounced = now;
    Serial.printf("display %d (%s) %s, %s\n", d.displayNo, d.key,
                  reason == ReasonNew ? "connected" : reason == ReasonRejoined ? "back" : "restarted",
                  ip.toString().c_str());
    onDisplayAvailable(index);
  }

  adoptStateFromPresence(index);
  keepClockInSync(index);
}

void handlePacket(IPAddress sender) {
  const char* source = doc["src"] | "";
  if (strcmp(source, "display") != 0) {
    return;  // our own broadcasts and those of the app come back to us
  }
  const char* command = doc["cmd"] | "";
  const char* reportedId = doc["id"] | "";

  IPAddress ip = sender;
  const char* reportedIp = doc["ip"] | "";
  if (reportedIp[0] != 0) {
    IPAddress parsed;
    if (parsed.fromString(reportedIp)) {
      ip = parsed;
    }
  }

  char key[16];
  strlcpy(key, isUsableId(reportedId) ? reportedId : ip.toString().c_str(), sizeof(key));

  bool hello = strcmp(command, "hello") == 0;
  if (hello || strcmp(command, "heartbeat") == 0) {
    handlePresence(key, reportedId, ip, hello);
    return;  // presence traffic never reaches the ui
  }

  int index = findDisplay(key);

  // every display acknowledges every command, not just the reporting one
  if (strcmp(command, "ack") == 0) {
    if (index >= 0) {
      onAck(doc["seq"] | (uint64_t)0, index);
    }
    return;
  }

  // A display that reports state is obviously on the network, even if its
  // hello was missed. Without this the arbitration below would have nothing
  // to go by and would let every display drive the ui.
  if (index < 0) {
    index = allocateDisplay(key);
    if (index < 0) {
      return;
    }
    displays[index].ip = ip;
    strlcpy(displays[index].deviceId, isUsableId(reportedId) ? reportedId : "", sizeof(displays[index].deviceId));
  }
  displays[index].displayNo = doc["dn"] | displays[index].displayNo;
  displays[index].lastSeen = millis();
  displays[index].online = true;

  // with n displays only the reporting one may drive the ui
  if (!mayReportState(index)) {
    return;
  }

  bool resynced = session.stale;
  if (strcmp(command, "state") == 0) {
    sessionOnStateReceived();
  } else if (strcmp(command, "stop") == 0 || strcmp(command, "reset") == 0) {
    sessionOnDisplayStopped();
  } else if (strcmp(command, "emergency") == 0) {
    // set or released by the other controller
    session.emergency = doc["e"] | false;
  }
  notify(command, resynced);
}

void receiveLoop() {
  // a handful per pass, so a burst of heartbeats cannot starve the screen
  for (uint8_t i = 0; i < 8; i++) {
    if (udp.parsePacket() <= 0) {
      return;
    }
    int length = udp.read(receiveBuffer, sizeof(receiveBuffer) - 1);
    if (length <= 0) {
      continue;
    }
    receiveBuffer[length] = 0;
    IPAddress sender = udp.remoteIP();
    if (deserializeJson(doc, receiveBuffer)) {
      continue;
    }
    handlePacket(sender);
  }
}

// ---------------------------------------------------------------------------
// wifi state machine - reconnects for as long as it takes
// ---------------------------------------------------------------------------

void startWifiConnect() {
  Serial.printf("connecting to ssid %s\n", WIFI_SSID);
  netState = NetConnecting;
  netAttemptStarted = millis();
  WiFi.disconnect(false, false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void goOnline() {
  netState = NetOnline;
  netBackoff = NET_BACKOFF_MIN;
  wifiActive = true;

  // the socket still holds the old interface address, so it has to be
  // reopened for every fresh lease - without this nothing arrives after a
  // reconnect.
  if (udpStarted) {
    udp.stop();
  }
  udp.begin(UDP_PORT);
  udpStarted = true;

  Serial.printf("wifi connected, ip %s, %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());

  // whatever happened while nothing was received - the displays know
  session.stale = true;
  clockStartNtp();
  sendDiscover();
  discoverNextTime = millis() + DISCOVER_INTERVAL;
}

void goOffline() {
  Serial.println("wifi connection lost");
  netState = NetOffline;
  wifiActive = false;
  netNextAttempt = millis();  // try again at once, then back off
  for (uint8_t i = 0; i < MAX_PENDING; i++) {
    pending[i].used = false;
  }
}

void wifiLoop() {
  unsigned long now = millis();
  switch (netState) {
    case NetOffline:
      if ((long)(now - netNextAttempt) >= 0) {
        startWifiConnect();
      }
      break;

    case NetConnecting:
      if (WiFi.status() == WL_CONNECTED) {
        goOnline();
      } else if (now - netAttemptStarted > NET_CONNECT_TIMEOUT) {
        Serial.printf("wifi connect timed out, next try in %lu ms\n", (unsigned long)netBackoff);
        WiFi.disconnect(false, false);
        netState = NetOffline;
        netNextAttempt = now + netBackoff;
        netBackoff = netBackoff < NET_BACKOFF_MAX ? netBackoff * 2 : NET_BACKOFF_MAX;
      }
      break;

    case NetOnline:
      if (WiFi.status() != WL_CONNECTED) {
        goOffline();
      }
      break;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// public
// ---------------------------------------------------------------------------

void netBegin(NetEventHandler handler) {
  eventHandler = handler;
  seqBegin();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // modem sleep makes the board miss udp broadcasts
  WiFi.setHostname("archerytimer-controller");
  // connecting is driven by netLoop(), so a missing access point does not
  // block the start and is retried forever.
  startWifiConnect();
}

void netLoop() {
  wifiLoop();
  if (!wifiActive) {
    return;
  }

  receiveLoop();
  retryPending();

  unsigned long now = millis();
  if ((long)(now - sweepNextTime) >= 0) {
    sweepNextTime = now + SWEEP_INTERVAL;
    sweep();
    flushPendingSync();
    if (onlineMask() == 0 && (long)(now - discoverNextTime) >= 0) {
      discoverNextTime = now + DISCOVER_INTERVAL;
      sendDiscover();
    }
  }
}

bool netWifiConnected() {
  return wifiActive;
}

uint8_t netDisplaysOnline() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if (displays[i].used && displays[i].online) {
      count++;
    }
  }
  return count;
}

uint8_t netDisplaysKnown() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    if (displays[i].used) {
      count++;
    }
  }
  return count;
}

void netSendRun() {
  JsonDocument d;
  d["cmd"] = "run";
  d["val"]["t"] = 0;
  d["val"]["h"] = false;
  dispatch(d, -1, millis(), true);
}

void netSendPause() {
  JsonDocument d;
  d["cmd"] = "pause";
  d["val"]["t"] = 0;
  d["val"]["h"] = true;
  dispatch(d, -1, millis(), true);
}

void netSendStop() {
  JsonDocument d;
  d["cmd"] = "stop";
  d["val"]["h"] = false;
  dispatch(d, -1, millis(), true);
}

void netSendReset() {
  JsonDocument d;
  d["cmd"] = "reset";
  dispatch(d, -1, millis(), true);
}

void netSendEmergency(bool on) {
  JsonDocument d;
  d["cmd"] = "emergency";
  d["val"]["e"] = on;
  dispatch(d, -1, millis(), true);
}

void netSendConfig() {
  sendConfiguration(-1);
}

void netSendVolume() {
  sendVolume(-1);
}

void netSendTestSignal() {
  JsonDocument d;
  d["cmd"] = "testsignal";
  dispatch(d, -1, millis(), true);
}

void netChangeView(const char* viewName) {
  JsonDocument d;
  d["cmd"] = "change_view";
  d["v"] = viewName;
  if (strcmp(viewName, "lunch") == 0) {
    // The time as text for the display to show, and the same time of today as
    // seconds since 1970. The displays keep UTC and know no time zone, so
    // they cannot tell on their own when "13:30" is reached.
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", settings.lunchHour % 24, settings.lunchMinute % 60);
    d["val"]["t"] = time;
    d["val"]["e"] = clockEpochToday(settings.lunchHour, settings.lunchMinute);
  }
  dispatch(d, -1, millis(), true);
}

void netAdoptView(const char* viewName) {
  strlcpy(session.viewName, viewName, sizeof(session.viewName));
}

void netClockChanged() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_DISPLAYS; i++) {
    displays[i].lastClockSync = now - CLOCK_SYNC_INTERVAL;
  }
}
