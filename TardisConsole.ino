/*
   -- sketch: TardisConsoleMega2560 --

   by Dan Efran, 2021-03

   Not surprisingly, this is a TARDIS Console sketch to run on an Arduino Mega 2560.
   ("TARDIS Console" in the sense of "Rocketship Busy-board": we make some lights and switches
   "do something" abstractly futuristic.)

*/

#define version_string "version 20210406.016"

#include <SoftwareSerial.h>
#include "Adafruit_Soundboard.h"

// ** pin assignments

// sound effects board communication
#define SoundFX_TX 12
#define SoundFX_RX 13
#define SoundFX_Reset 14
#define SoundFX_Active 15

// lights
#define light_demat_bottom 38
#define light_demat_middle 40
#define light_demat_top 42

// switches
#define switch_demat_lever 50
#define switch_door_lever 52
#define switch_major_mode 48

// switch sources (some switches connect to adjacent pins rather than to a bus)
#define switch_source_demat_lever 51
#define switch_source_door_lever 53
#define switch_source_major_mode 49

// analog inputs
#define knob_speed A0

// ** sounds

#define SFX_DOORS 0
#define SFX_DEMAT 1
#define SFX_REMAT 2
#define SFX_BLASTOFF 3
#define SFX_LANDING 4
#define SFX_CLOISTER 5
#define SFX_OLDHUM 6
#define SFX_OLDCLOIS 7
#define SFX_HUM 8
#define SFX_6BEEPS 9
#define SFX_LOWPOWER 10
#define SFX_KACHUNK 11
#define SFX_KEYCLIK1 12
#define SFX_KEYCLIK2 13

#define sound_startup SFX_KEYCLIK1

#define SFX_PRIORITY_LOWEST 0
#define SFX_PRIORITY_OPTIONAL 0
#define SFX_PRIORITY_HIGHEST 1
#define SFX_PRIORITY_REPLACE 1

// time allowed for sound to actually start, before checking to see if it's done
// 100 minimum! slower is fine up to 500 or so. (More might be perceptible lag.)
#define SOUND_CHECK_DELAY 200

// ** other constants

// LEDs are common anode; set cathode LOW-to-glow
#define LED_OFF HIGH
#define LED_ON LOW
// switches are "closed" = LOW because "open" = INPUT_PULLUP
// switches not physically connectable to a GND pin can (equivalently) connect to a SWITCH_SOURCE output pin
#define SWITCH_SOURCE LOW

// ** global objects & data

SoftwareSerial soundFX_serial = SoftwareSerial(SoundFX_TX, SoundFX_RX);
Adafruit_Soundboard soundFX_board = Adafruit_Soundboard(&soundFX_serial, NULL, SoundFX_Reset);

// major mode switch toggles between modes 0 and 1.
// modes 2+ are disabled; edit here to set the desired active modes as 0 and 1
#define MAJOR_MODE_ROCKET   0
#define MAJOR_MODE_DEMO     2
#define MAJOR_MODE_TARDIS   1
#define MAJOR_MODE_STARTUP  -1

#define MINOR_MODE_STARTUP  -1
#define MINOR_MODE_IDLE     0
#define MINOR_MODE_TAKEOFF  1
#define MINOR_MODE_FLIGHT   2
#define MINOR_MODE_LANDING  3

typedef struct {
  int value;
  boolean changed;
} Control;

typedef struct {
  int pin;
  int value;
  uint32_t when;
} LfxEvent;

uint32_t ULONG_MAX = 0UL - 1UL;
uint32_t LFX_NEVER = ULONG_MAX;

#define LFX_EVENTS_MAX 100
LfxEvent lfxEvents[LFX_EVENTS_MAX];

#define LFX_PRIORITY_MERGE 0
#define LFX_PRIORITY_REPLACE 1

typedef struct {
  int major_mode;
  int minor_mode;
  boolean sound_end_mode_change;
  Control door_lever;
  Control demat_lever;
} Tardis;

Tardis TARDIS = {
  .major_mode = MAJOR_MODE_STARTUP,
  .minor_mode = MINOR_MODE_IDLE,
  .sound_end_mode_change = false,
  .door_lever = { .value = -1, .changed = false },
  .demat_lever = { .value = -1, .changed = false }
};

typedef struct {
  int takeoff;
  int landing;
  int doors;
  int startup;
} Soundset;

// one per major mode: 0, 1
Soundset soundset[2] = {
  { .takeoff = SFX_BLASTOFF,
    .landing = SFX_LANDING,
    .doors = SFX_KACHUNK,
    .startup = SFX_KEYCLIK2
  },
  { .takeoff = SFX_DEMAT,
    .landing = SFX_REMAT,
    .doors = SFX_DOORS,
    .startup = SFX_KEYCLIK1
  }
};

// ** main functions

void setup() {

  // ** establish (optional) serial connection with computer (for debugging etc.)

  Serial.begin(115200);
  print_hello_serial();

  // ** establish connection with sound effects board and reset it

  soundFX_serial.begin(9600);

  if (!soundFX_board.reset()) {
    Serial.println("SFX board not found");
  } else {
    Serial.println("SFX board found");
    soundFX_list_files();
  }

  // ** configure Lights (LED-driving output pins)

  pinMode(light_demat_bottom, OUTPUT);
  digitalWrite(light_demat_bottom, LED_OFF);

  pinMode(light_demat_middle, OUTPUT);
  digitalWrite(light_demat_middle, LED_OFF);

  pinMode(light_demat_top, OUTPUT);
  digitalWrite(light_demat_top, LED_OFF);

  // ** configure switches (buttons, levers, etc.)

  pinMode(switch_source_demat_lever, OUTPUT);
  digitalWrite(switch_source_demat_lever, SWITCH_SOURCE);
  pinMode(switch_demat_lever, INPUT_PULLUP);

  pinMode(switch_source_door_lever, OUTPUT);
  digitalWrite(switch_source_door_lever, SWITCH_SOURCE);
  pinMode(switch_door_lever, INPUT_PULLUP);

  pinMode(switch_source_major_mode, OUTPUT);
  digitalWrite(switch_source_major_mode, SWITCH_SOURCE);
  pinMode(switch_major_mode, INPUT_PULLUP);

  // ** prepare to animate lights
  
  for (int ii = 0; ii < LFX_EVENTS_MAX; ii++) {
    lfxEvents[ii].pin = -1;
    lfxEvents[ii].when = LFX_NEVER;
  }
}


void loop() {

  test_major_mode();

  switch (TARDIS.major_mode) {
    case MAJOR_MODE_TARDIS:
      loop_tardis();
      break;
    case MAJOR_MODE_ROCKET:
      loop_rocket();
      break;
    case MAJOR_MODE_DEMO:
      loop_demo();
      break;
  }

}

// ** support functions

#define LFX_DEMAT 1
#define LFX_REMAT 2

boolean lightFX_addEvent(int pin, int value, uint32_t when, boolean priority) {

  if (priority == LFX_PRIORITY_REPLACE) {
    for (int ii = 0; ii < LFX_EVENTS_MAX; ii++) {
      if (lfxEvents[ii].pin == pin) {
        lfxEvents[ii].pin = -1;
        lfxEvents[ii].when = LFX_NEVER;
      }
    }
  }
  for (int ii = 0; ii < LFX_EVENTS_MAX; ii++) {
    if (lfxEvents[ii].pin == -1) {
      lfxEvents[ii].pin = pin;
      lfxEvents[ii].value = value;
      lfxEvents[ii].when = when;
      return true;
    }
  }
  return false;
}

void lightFX_update() {

  uint32_t now = millis();

  for (int ii = 0; ii < LFX_EVENTS_MAX; ii++) {
    if (lfxEvents[ii].pin != -1) {
      if (now >= lfxEvents[ii].when) {
        digitalWrite(lfxEvents[ii].pin, lfxEvents[ii].value);
        lfxEvents[ii].pin = -1;
        lfxEvents[ii].when = LFX_NEVER;
      }
    }
  }
}

void lightFX_play(int lfx, boolean priority) {

  uint32_t now = millis();

  switch (lfx) {

    case LFX_DEMAT:
      digitalWrite(light_demat_bottom, LED_OFF);
      digitalWrite(light_demat_middle, LED_OFF);
      digitalWrite(light_demat_top, LED_OFF);
      lightFX_addEvent(light_demat_bottom, LED_ON, now + 600, priority);
      lightFX_addEvent(light_demat_middle, LED_ON, now + 1000, priority);
      lightFX_addEvent(light_demat_top,    LED_ON, now + 1400, priority);

      for (int ii = 0; ii < 6; ii++) {
        int pulse_time = 4000 + 2000 * ii;
        lightFX_addEvent(light_demat_bottom, LED_OFF, now +  0 + pulse_time, LFX_PRIORITY_MERGE);
        lightFX_addEvent(light_demat_bottom, LED_ON, now +  300 + pulse_time, LFX_PRIORITY_MERGE);
        lightFX_addEvent(light_demat_middle, LED_OFF, now + 100 + pulse_time, LFX_PRIORITY_MERGE);
        lightFX_addEvent(light_demat_middle, LED_ON, now +  400 + pulse_time, LFX_PRIORITY_MERGE);
        lightFX_addEvent(light_demat_top,    LED_OFF, now + 200 + pulse_time, LFX_PRIORITY_MERGE);
        lightFX_addEvent(light_demat_top,    LED_ON, now +  600 + pulse_time, LFX_PRIORITY_MERGE);
      }

      break;

    case LFX_REMAT:
      digitalWrite(light_demat_bottom, LED_ON);
      digitalWrite(light_demat_middle, LED_ON);
      digitalWrite(light_demat_top, LED_ON);
      lightFX_addEvent(light_demat_top, LED_OFF, now + 600, priority);
      lightFX_addEvent(light_demat_middle, LED_OFF, now + 1200, priority);
      lightFX_addEvent(light_demat_bottom,    LED_OFF, now + 1800, priority);
      break;

  }
}

void test_major_mode() {

  int major_mode = digitalRead(switch_major_mode);
  if (major_mode != TARDIS.major_mode) {
    major_mode_begin(major_mode);
  }
}

uint32_t next_sound_check = 0;

void major_mode_begin(int major_mode) {

  TARDIS.major_mode = major_mode;

  switch (TARDIS.major_mode) {

    case MAJOR_MODE_TARDIS:
      digitalWrite(light_demat_middle, LED_OFF);
      digitalWrite(light_demat_top, LED_OFF);
      digitalWrite(light_demat_bottom, LED_OFF);
      TARDIS.door_lever.value = -1;
      TARDIS.door_lever.changed = false;
      TARDIS.minor_mode = MINOR_MODE_STARTUP;
      Serial.println("TARDIS Startup...");
      soundFX_play(soundset[TARDIS.major_mode].startup, SFX_PRIORITY_HIGHEST);
      TARDIS.sound_end_mode_change = true;
      next_sound_check = millis() + SOUND_CHECK_DELAY;
      break;

    case MAJOR_MODE_ROCKET:

      digitalWrite(light_demat_middle, LED_OFF);
      digitalWrite(light_demat_top, LED_OFF);
      digitalWrite(light_demat_bottom, LED_OFF);
      TARDIS.door_lever.value = -1;
      TARDIS.door_lever.changed = false;
      TARDIS.minor_mode = MINOR_MODE_STARTUP;
      Serial.println("Rocket Startup...");
      soundFX_play(soundset[TARDIS.major_mode].startup, SFX_PRIORITY_HIGHEST);
      TARDIS.sound_end_mode_change = true;
      next_sound_check = millis() + SOUND_CHECK_DELAY;
      break;

    case MAJOR_MODE_DEMO:

      Serial.println("Self-Test Startup...");
      digitalWrite(light_demat_bottom, LED_ON);
      // other lights are set directly from levers in this major mode
      soundFX_play(SFX_6BEEPS, SFX_PRIORITY_HIGHEST);
      break;
  }
}

boolean already_playing = false;

void loop_tardis() {

  uint32_t current_time = millis();

  // ** when certain sounds end, minor mode changes.

  if (TARDIS.sound_end_mode_change && (current_time > next_sound_check)) {
    next_sound_check = current_time + SOUND_CHECK_DELAY;
    if (!soundFX_playing()) {
      switch (TARDIS.minor_mode) {
        case MINOR_MODE_LANDING:
          Serial.println("...landed.");
          TARDIS.minor_mode = MINOR_MODE_IDLE;
          TARDIS.sound_end_mode_change = false;
          break;
        case MINOR_MODE_TAKEOFF:
          Serial.println("...flying.");
          TARDIS.minor_mode = MINOR_MODE_FLIGHT;
          TARDIS.sound_end_mode_change = false;
          break;
        case MINOR_MODE_STARTUP:
          Serial.println("...ready.");
          TARDIS.minor_mode = MINOR_MODE_IDLE;
          TARDIS.sound_end_mode_change = false;
          break;
      }
    }
  }

  // ** poll for changed controls

  int value;

  value = digitalRead(switch_door_lever);
  if (value != TARDIS.door_lever.value) {
    if (TARDIS.door_lever.value != -1) {
      TARDIS.door_lever.changed = true;
    }
    TARDIS.door_lever.value = value;
  }

  value = digitalRead(switch_demat_lever);
  if (value != TARDIS.demat_lever.value) {
    if (TARDIS.demat_lever.value != -1) {
      TARDIS.demat_lever.changed = true;
    }
    TARDIS.demat_lever.value = value;
  }

  // ** take action based on control changes

  // ("take action" is mostly sounds, lights, and minor_mode transitions)

  if (TARDIS.door_lever.changed) {
    Serial.println("Doors.");
    soundFX_play(soundset[TARDIS.major_mode].doors, SFX_PRIORITY_OPTIONAL);
    TARDIS.door_lever.changed = false;
  }

  if (TARDIS.demat_lever.changed) {
    switch (TARDIS.minor_mode) {

      case MINOR_MODE_IDLE:
        TARDIS.minor_mode = MINOR_MODE_TAKEOFF;
        TARDIS.sound_end_mode_change = true;
        Serial.println("Demat...");
        lightFX_play(LFX_DEMAT, LFX_PRIORITY_REPLACE);
        soundFX_play(soundset[TARDIS.major_mode].takeoff, SFX_PRIORITY_REPLACE);
        next_sound_check = current_time + SOUND_CHECK_DELAY;
        break;

      case MINOR_MODE_TAKEOFF:
        Serial.println("(demat lever changed in takeoff mode)");
        break;

      case MINOR_MODE_FLIGHT:
        Serial.println("Remat...");
        TARDIS.minor_mode = MINOR_MODE_LANDING;
        TARDIS.sound_end_mode_change = true;
        lightFX_play(LFX_REMAT, LFX_PRIORITY_REPLACE);
        soundFX_play(soundset[TARDIS.major_mode].landing, SFX_PRIORITY_REPLACE);
        next_sound_check = current_time + SOUND_CHECK_DELAY;
        break;

      case MINOR_MODE_LANDING:
        Serial.println("(demat lever changed in landing mode)");
        break;
    }
    TARDIS.demat_lever.changed = false;
  }

  // ** animate lights

  lightFX_update();

  // note: if preferred, it's fine to run this loop a bit less frequently, e.g.:
  // delay(10);

}

void monitor_playback_status() {

  boolean playing = soundFX_playing();
  if (playing) {
    if (!already_playing) {
      Serial.println("sfx playing....");
    }
  } else {
    if (already_playing) {
      Serial.println("sfx done playing.");
    }
  }
  already_playing = playing;
}

boolean soundFX_playing() {
  return !digitalRead(SoundFX_Active);
}

void print_hello_serial() {

  Serial.println("");
  Serial.println("======================");
  Serial.println("==  TARDIS Console  ==");
  Serial.println("======================");
  Serial.println("");
  Serial.println(version_string);
}


void soundFX_play(uint8_t file_number, int priority) {

  if (soundFX_playing()) {
    switch (priority) {
      case SFX_PRIORITY_OPTIONAL:
        // so don't play while something else is playing
        return; // early exit

      case SFX_PRIORITY_REPLACE:
        soundFX_stop();
        break;
    }
  }

  // a bug in the sound FX board makes play() fail sometimes after stop().
  // luckily, a volUp in between will fix it.
  soundFX_board.volUp();
  
  if (! soundFX_board.playTrack((uint8_t)file_number) ) {
    Serial.print("Failed to play sound? #");
    Serial.println(file_number);
  }
}

void soundFX_stop() {
  if (! soundFX_board.stop() ) {
    Serial.println("Failed to stop sound");
  }
  // a bug in the sound FX board makes play() fail sometimes after stop().
  // luckily, a volUp in between will fix it.
  soundFX_board.volUp();
}

void soundFX_list_files() {

  uint8_t files = soundFX_board.listFiles();

  Serial.println("SFX File Listing");
  Serial.println("========================");
  Serial.println();
  Serial.print("Found "); Serial.print(files); Serial.println(" Files");

  for (uint8_t f = 0; f < files; f++) {
    Serial.print(f);
    Serial.print("\tname: "); Serial.print(soundFX_board.fileName(f));
    Serial.print("\tsize: "); Serial.println(soundFX_board.fileSize(f));
  }

  Serial.println();
  Serial.println("========================");
  Serial.println();
}

// state tracking for loop_demo
int old_speed = 0;
int old_door = 3;
int old_demat = 3;
int printed_something = 0;

void loop_demo() {

  // ** proof-of-concept loop, responds crudely to a few controls

  digitalWrite(light_demat_middle, digitalRead(switch_demat_lever));
  digitalWrite(light_demat_top, digitalRead(switch_door_lever));

  int val = analogRead(knob_speed);
  if (abs(val - old_speed) > 2) {
    Serial.print("Speed: ");
    Serial.print(val);
    printed_something = 1;
    old_speed = val;
  }

  int door = digitalRead(switch_door_lever);
  if (door != old_door) {
    Serial.print("  Door: ");
    Serial.print(door);
    printed_something = 1;
  }
  old_door = door;

  int demat = digitalRead(switch_demat_lever);
  if (demat != old_demat) {
    Serial.print("  Demat: ");
    Serial.print(demat);
    printed_something = 1;
  }
  old_demat = demat;

  if (printed_something == 1) {
    Serial.println("");
    printed_something = 0;
  }

}

void loop_rocket() {
  // at present, rocket mode differs from tardis mode only in which sounds are played.
  loop_tardis();
}


/* -- NOTES --

    General capabilities:
    - debounced switches (in hardware?)
    - sounds on switch state changes (different sound each way)
    - sound while switch closed (looping)
    - sound priorities (one pauses another, 2 levels (background/foreground) is probably enough)
    - delay trigger: lights/sounds happen several seconds after the triggering switch event

    Specific operations:
    - door lever: sound DoorOpen on switch open; sound DoorClose on switch close (or v.v.)
    - mat/demat lever: sound Materialize on switch open; sound Dematerialize on switch close (or v.v.)
    - background: sound BackgroundHum while switch is closed, lowest priority (paused by other sounds)
    - recall pyramid: three LEDs strobe-flash in a pattern, while a switch is closed but starting delayed
    - fault indicator: sound WarningBeeps loops and one LED flashes after a button is pressed,
        starting several seconds delayed, stopping when another button is pressed
    - cloister bell: sound ClosterBell repeats while switch (Fast Return) has been closed for ten seconds,
        but starting delayed another ten seconds or more
    - keypad buttons: sound ButtonBoopN (for several N at random) when any switch of a group are closed)
    - blinkenlights: a group of lights that can flash in at least a "thinking" random mode; more modes ideal

*/

