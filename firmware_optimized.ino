#include <MIDIUSB.h>

// =====================================================================
//  DJC-DIY controller firmware — optimized
//  6 push buttons + 2 analog (resistor-ladder) buttons + 7 pots
//  + 2 rotary encoders -> MIDI notes/CC over USB (MIDIUSB)
// =====================================================================

// ---------------- TIMING ----------------
const unsigned long POT_READ_INTERVAL = 10;   // ms between pot scans
const unsigned long DEBOUNCE_DELAY    = 15;   // ms debounce for push buttons

// ---------------- DIGITAL PUSH BUTTONS ----------------
const byte BUTTON_COUNT = 6;
const byte buttonPins[BUTTON_COUNT]  = {5, 7, 10, 14, 15, 16};
const byte buttonNotes[BUTTON_COUNT] = {60, 61, 62, 63, 64, 65};

bool buttonState[BUTTON_COUNT]           = {false}; // debounced/confirmed state
bool buttonLastReading[BUTTON_COUNT]     = {false}; // raw reading from previous scan
unsigned long buttonLastChangeMs[BUTTON_COUNT] = {0};

// ------------- ANALOG PUSH BUTTONS ON A6 (resistor ladder) -------------
const byte ANALOG_BTN_COUNT = 2;
const byte  analogBtnNotes[ANALOG_BTN_COUNT] = {66, 67};        // NOTE_1, NOTE_2
const int   analogBtnMin[ANALOG_BTN_COUNT]   = {450, 630};      // ~512 / ~680 centers
const int   analogBtnMax[ANALOG_BTN_COUNT]   = {580, 750};
bool analogBtnState[ANALOG_BTN_COUNT] = {false};

// ---------------- POTENTIOMETERS ----------------
const byte POT_COUNT = 7;
const byte potPins[POT_COUNT] = {A0, A1, A2, A3, A7, A8, A9};
const byte potCCs[POT_COUNT]  = {10, 11, 12, 13, 14, 15, 16};
int  potValue[POT_COUNT] = {-1};   // -1 forces the first reading to always be sent
const int  POT_THRESHOLD = 2;

// A0-A3 are plain pots -> simple linear 0-1023 map.
const int potMin[4] = {0, 0, 0, 0};
const int potMax[4] = {1023, 1023, 1023, 1023};

// ---- CENTERED POT CALIBRATION (A7 = Tempo1, A8 = Crossfader, A9 = Tempo2) ----
// These pots have a spring/detent center. Component tolerance means the real
// electrical center is almost never exactly 512, and travel is rarely
// symmetric — that's what causes the "off-center" MIDI value.
// Run calibrate_center_pots.ino, read the 3 raw values per pot (full CCW,
// physical CENTER, full CW) from the Serial Monitor, and replace the
// numbers below. Defaults shown are just placeholders (assume ideal 512 center).
const int centeredRawMin[3]    = {0,   273,  0};     // <-- measured: full CCW
const int centeredRawCenter[3] = {869, 888,  893};   // <-- measured: physical center
const int centeredRawMax[3]    = {1023, 995, 1023};  // <-- measured: full CW

unsigned long lastPotReadMs = 0;

// Two-segment map so the measured physical center ALWAYS reports MIDI 64,
// regardless of how asymmetric the real travel is on either side.
int mapCenteredPot(int raw, int rawMin, int rawCenter, int rawMax) {
  raw = constrain(raw, rawMin, rawMax);
  if (raw <= rawCenter) {
    return map(raw, rawMin, rawCenter, 0, 64);
  }
  return map(raw, rawCenter, rawMax, 64, 127);
}

// ---------------- ROTARY ENCODERS ----------------
const byte ENCODER_COUNT = 2;
const byte encoderAPins[ENCODER_COUNT] = {0, 3};
const byte encoderBPins[ENCODER_COUNT] = {1, 2};
const byte encoderCCs[ENCODER_COUNT]   = {20, 21};
int encoderLastState[ENCODER_COUNT] = {0, 0};

// =====================================================================
void setup() {
  for (byte i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  for (byte i = 0; i < ENCODER_COUNT; i++) {
    pinMode(encoderAPins[i], INPUT_PULLUP);
    pinMode(encoderBPins[i], INPUT_PULLUP);
    encoderLastState[i] = digitalRead(encoderAPins[i]);
  }
}

void loop() {
  handleButtons();        // digital push buttons, debounced, non-blocking
  handleAnalogButtons();  // resistor-ladder buttons on A6
  handlePots();           // rate-limited to POT_READ_INTERVAL
  handleEncoders();        // scanned every loop -> no missed steps

  MidiUSB.flush();        // ONE USB transfer per loop instead of one per event
}

// ------------------ DIGITAL PUSH BUTTONS (debounced) ------------------
void handleButtons() {
  unsigned long now = millis();

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    bool reading = !digitalRead(buttonPins[i]); // pressed = LOW (INPUT_PULLUP)

    if (reading != buttonLastReading[i]) {
      buttonLastChangeMs[i] = now;
      buttonLastReading[i] = reading;
    }

    if ((now - buttonLastChangeMs[i]) > DEBOUNCE_DELAY && reading != buttonState[i]) {
      buttonState[i] = reading;
      reading ? noteOn(buttonNotes[i]) : noteOff(buttonNotes[i]);
    }
  }
}

// ------------- ANALOG PUSH BUTTONS ON A6 (resistor ladder) -------------
void handleAnalogButtons() {
  int val = analogRead(A6);

  for (byte i = 0; i < ANALOG_BTN_COUNT; i++) {
    bool inZone = (val > analogBtnMin[i] && val < analogBtnMax[i]);

    if (inZone != analogBtnState[i]) {
      analogBtnState[i] = inZone;
      inZone ? noteOn(analogBtnNotes[i]) : noteOff(analogBtnNotes[i]);
    }
  }
}

// ------------------------- POTENTIOMETERS -------------------------
void handlePots() {
  unsigned long now = millis();
  if (now - lastPotReadMs < POT_READ_INTERVAL) return;
  lastPotReadMs = now;

  for (byte i = 0; i < POT_COUNT; i++) {
    int raw = analogRead(potPins[i]);
    int mapped;

    if (i < 4) {
      // A0-A3: plain linear pots
      raw = constrain(raw, potMin[i], potMax[i]);
      mapped = map(raw, potMin[i], potMax[i], 0, 127);
    } else {
      // A7/A8/A9: centered pots, mapped in two segments around the
      // measured physical center so it always lands exactly on MIDI 64.
      byte c = i - 4;
      mapped = mapCenteredPot(raw, centeredRawMin[c], centeredRawCenter[c], centeredRawMax[c]);
    }

    if (abs(mapped - potValue[i]) >= POT_THRESHOLD) {
      potValue[i] = mapped;
      sendCC(potCCs[i], mapped);
    }
  }
}

// --------------------------- ENCODERS ------------------------------
void handleEncoders() {
  for (byte i = 0; i < ENCODER_COUNT; i++) {
    int currentState = digitalRead(encoderAPins[i]);

    if (currentState != encoderLastState[i]) {
      int bState = digitalRead(encoderBPins[i]);
      byte directionValue = (currentState == bState) ? 0x41 : 0x3F;
      sendCC(encoderCCs[i], directionValue);
      encoderLastState[i] = currentState;
    }
  }
}

// ------------------------ MIDI HELPERS ------------------------------
// NOTE: sendMIDI() only queues the packet; MidiUSB.flush() in loop()
// pushes everything out in a single USB transfer per iteration.
void noteOn(byte pitch) {
  midiEventPacket_t msg = {0x09, 0x90, pitch, 127};
  MidiUSB.sendMIDI(msg);
}

void noteOff(byte pitch) {
  midiEventPacket_t msg = {0x08, 0x80, pitch, 0};
  MidiUSB.sendMIDI(msg);
}

void sendCC(byte cc, byte val) {
  midiEventPacket_t msg = {0x0B, 0xB0, cc, val};
  MidiUSB.sendMIDI(msg);
}
