/*
  THERMAL FRONT - Arduino controller
  ----------------------------------
  Inputs : FSR on A0 (ship height), potentiometer on A1 (temperature)
  Outputs: piezo on pin 8 (all game sounds), LEDs on 9 (frost/blue),
           10 (water/green), 11 (fire/red)

  Wiring
    FSR     : one leg to 5V, other leg to A0 AND to GND through a 10k resistor
    Pot     : outer pins to 5V and GND, middle pin (wiper) to A1
              (if frost/fire feel backwards, swap the 5V and GND outer pins)
    Piezo   : one leg to pin 8, other leg to GND
    LEDs    : pin -> 220 ohm resistor -> LED long leg, LED short leg -> GND

  Serial protocol (115200 baud)
    Arduino -> PC : "A,<fsr 0-1023>,<pot 0-1023>\n"  about 50 times a second
    PC -> Arduino : "S<id>\n"  play sound id      (S0 = stop)
                    "L<mode>\n" LED mode: 0 = follow pot, 1 = power-up flicker,
                                          2 = victory chase, 3 = game-over pulse

  No pitches.h needed: the notes are defined below.
  Close the Arduino Serial Monitor before running the game (only one program
  can use the USB port at a time).
*/

#include <avr/pgmspace.h>

const uint8_t PIN_FSR   = A0;
const uint8_t PIN_POT   = A1;
const uint8_t PIN_SPK   = 8;
const uint8_t LED_FIRE  = 11;
const uint8_t LED_WATER = 10;
const uint8_t LED_FROST = 9;

// ---- notes (Hz) ----
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_G6  1568

// ---- sounds: {frequency, duration_ms} pairs, frequency 0 = rest, duration 0 = end ----

// 1 GAME START: AA...ABC...AA...ABC...AA
const uint16_t SND_START[] PROGMEM = {
  NOTE_A4,110, NOTE_A4,110, 0,110,
  NOTE_A4,110, NOTE_B4,110, NOTE_C5,160, 0,110,
  NOTE_A4,110, NOTE_A4,110, 0,110,
  NOTE_A4,110, NOTE_B4,110, NOTE_C5,160, 0,110,
  NOTE_A4,140, NOTE_A4,320, 0,0 };
// 2 FROST pew: high, glassy chirp
const uint16_t SND_PEW_FROST[] PROGMEM = { 2600,16, 3300,16, 2800,16, 3500,24, 0,0 };
// 3 WATER pew: bubbly bloop
const uint16_t SND_PEW_WATER[] PROGMEM = { 480,24, 760,24, 600,32, 0,0 };
// 4 FIRE pew: falling laser
const uint16_t SND_PEW_FIRE[]  PROGMEM = { 1500,14, 1200,14, 950,14, 750,14, 600,20, 0,0 };
// 5 ENEMY laser pew: low and short
const uint16_t SND_PEW_ENEMY[] PROGMEM = { 520,14, 380,14, 280,18, 210,24, 0,0 };
// 6 ENEMY HIT: tick
const uint16_t SND_HIT[]       PROGMEM = { 330,16, 190,22, 0,0 };
// 7 ENEMY HEAL: soft rising two-note
const uint16_t SND_HEAL[]      PROGMEM = { 660,40, 880,70, 0,0 };
// 8 ENEMY DESTROYED: falling crunch
const uint16_t SND_DIE[]       PROGMEM = { 600,30, 450,30, 330,36, 240,40, 160,50, 110,70, 0,0 };
// 9 ENEMY UPGRADE: ominous rise
const uint16_t SND_UPGRADE[]   PROGMEM = { NOTE_G3,60, NOTE_C4,60, NOTE_G4,60, NOTE_C5,110, 0,0 };
// 10 PLAYER HIT: harsh buzz
const uint16_t SND_PLAYER_HIT[] PROGMEM = { 160,55, 110,55, 160,55, 90,90, 0,0 };
// 11 STAR / POWER UP: fast sparkling arpeggio
const uint16_t SND_STAR[]      PROGMEM = { NOTE_C5,45, NOTE_E5,45, NOTE_G5,45, NOTE_C6,45, NOTE_E6,45, NOTE_G6,130, 0,0 };
// 12 POWER DOWN: slow descent
const uint16_t SND_POWERDOWN[] PROGMEM = { NOTE_C6,70, NOTE_G5,70, NOTE_E5,70, NOTE_C5,70, NOTE_G4,70, NOTE_C4,170, 0,0 };
// 13 ALLIANCE TOKEN: trill that is clearly not the star sound
const uint16_t SND_TOKEN[]     PROGMEM = { NOTE_E5,65, NOTE_B5,65, NOTE_E5,65, NOTE_B5,65, NOTE_E6,170, 0,0 };
// 14 LEVEL UP: A C B D F E
const uint16_t SND_LEVELUP[]   PROGMEM = { NOTE_A4,130, NOTE_C5,130, NOTE_B4,130, NOTE_D5,130, NOTE_F5,130, NOTE_E5,340, 0,0 };
// 15 WAVE CLEARED: little chime
const uint16_t SND_WAVE_CLEAR[] PROGMEM = { NOTE_G5,70, NOTE_C6,130, 0,0 };
// 16 GAME OVER: sad descent
const uint16_t SND_GAMEOVER[]  PROGMEM = { NOTE_E4,230, NOTE_D4,230, NOTE_C4,230, NOTE_B3,230, 0,120, NOTE_A3,600, 0,0 };
// 17 WIN FANFARE
const uint16_t SND_WIN[]       PROGMEM = {
  NOTE_C5,110, NOTE_C5,110, NOTE_C5,110, NOTE_C5,320,
  NOTE_GS4,320, NOTE_AS4,320, NOTE_C5,170, NOTE_AS4,100, NOTE_C5,520,
  0,140, NOTE_E5,170, NOTE_G5,170, NOTE_C6,720, 0,0 };
// 18 FIREWORK POP (only heard after the fanfare, lowest priority)
const uint16_t SND_FW_POP[]    PROGMEM = { 1800,8, 900,10, 400,18, 150,26, 0,0 };
// 19 MENU MOVE
const uint16_t SND_MENU_MOVE[] PROGMEM = { NOTE_A5,25, 0,0 };
// 20 MENU SELECT
const uint16_t SND_MENU_SEL[]  PROGMEM = { NOTE_E5,50, NOTE_B5,100, 0,0 };

const uint8_t N_SOUNDS = 21;
const uint16_t* SOUND_TABLE[N_SOUNDS] = {
  NULL, SND_START, SND_PEW_FROST, SND_PEW_WATER, SND_PEW_FIRE, SND_PEW_ENEMY,
  SND_HIT, SND_HEAL, SND_DIE, SND_UPGRADE, SND_PLAYER_HIT,
  SND_STAR, SND_POWERDOWN, SND_TOKEN, SND_LEVELUP, SND_WAVE_CLEAR,
  SND_GAMEOVER, SND_WIN, SND_FW_POP, SND_MENU_MOVE, SND_MENU_SEL };

// A new sound only interrupts the current one if its priority is >= the current one.
const uint8_t SOUND_PRIO[N_SOUNDS] = {
  0, 8, 2, 2, 2, 1,
  3, 3, 4, 4, 5,
  6, 5, 6, 7, 5,
  8, 9, 1, 3, 5 };

// ---- non-blocking sound sequencer ----
const uint16_t* seqPtr = NULL;
uint8_t  seqPrio = 0;
uint32_t noteEnd = 0;

void playNext() {
  uint16_t f = pgm_read_word(seqPtr);
  uint16_t d = pgm_read_word(seqPtr + 1);
  if (d == 0) {                       // end of sound
    seqPtr = NULL; seqPrio = 0; noTone(PIN_SPK);
    return;
  }
  if (f == 0) noTone(PIN_SPK); else tone(PIN_SPK, f);
  noteEnd = millis() + d;
  seqPtr += 2;
}

void startSound(int id) {
  if (id <= 0) { noTone(PIN_SPK); seqPtr = NULL; seqPrio = 0; return; }
  if (id >= N_SOUNDS) return;
  if (seqPtr != NULL && SOUND_PRIO[id] < seqPrio) return;
  seqPtr = SOUND_TABLE[id];
  seqPrio = SOUND_PRIO[id];
  playNext();
}

void updateSound() {
  if (seqPtr != NULL && (int32_t)(millis() - noteEnd) >= 0) playNext();
}

// ---- LEDs ----
uint8_t ledMode = 0;
int lastPot = 512;
const int POT_THIRD = 341;
const int POT_DZ = 22;                // must match POT_DZ in the game

void setLeds(bool r, bool w, bool f) {
  digitalWrite(LED_FIRE, r);
  digitalWrite(LED_WATER, w);
  digitalWrite(LED_FROST, f);
}

void updateLeds() {
  uint32_t now = millis();
  if (ledMode == 2) { uint8_t s = (now / 110) % 3; setLeds(s == 0, s == 1, s == 2); return; }
  if (ledMode == 3) { setLeds(((now / 500) % 2) == 0, false, false); return; }
  if (ledMode == 1 && ((now / 70) % 2)) { setLeds(true, true, true); return; }

  int p = lastPot;
  if (p < POT_THIRD - POT_DZ)          setLeds(false, false, true);    // frost
  else if (p <= POT_THIRD + POT_DZ) {                                  // switching frost/water
    bool a = ((now / 70) % 2) == 0; setLeds(false, !a, a);
  }
  else if (p < 2 * POT_THIRD - POT_DZ) setLeds(false, true, false);    // water
  else if (p <= 2 * POT_THIRD + POT_DZ) {                              // switching water/fire
    bool a = ((now / 70) % 2) == 0; setLeds(!a, a, false);
  }
  else                                  setLeds(true, false, false);   // fire
}

// ---- serial commands from the game ----
char cmdBuf[8];
uint8_t cmdLen = 0;

void handleCmd() {
  if (cmdBuf[0] == 'S') startSound(atoi(cmdBuf + 1));
  else if (cmdBuf[0] == 'L') ledMode = atoi(cmdBuf + 1);
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) { cmdBuf[cmdLen] = 0; handleCmd(); cmdLen = 0; }
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

uint32_t lastSend = 0;

void setup() {
  pinMode(PIN_SPK, OUTPUT);
  pinMode(LED_FIRE, OUTPUT);
  pinMode(LED_WATER, OUTPUT);
  pinMode(LED_FROST, OUTPUT);
  Serial.begin(115200);
  Serial.println("READY");
}

void loop() {
  readSerial();
  updateSound();

  if (millis() - lastSend >= 20) {
    lastSend = millis();
    analogRead(PIN_FSR);                      // throwaway read lets the ADC settle
    int fsr = analogRead(PIN_FSR);
    analogRead(PIN_POT);
    int pot = analogRead(PIN_POT);
    lastPot = pot;
    Serial.print("A,");
    Serial.print(fsr);
    Serial.print(',');
    Serial.println(pot);
  }
  updateLeds();
}
