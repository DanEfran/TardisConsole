/*
 * -- sketch: TardisConsoleMega2560 --
 * 
 * by Dan Efran, 2021-03
 * 
 * Not surprisingly, this is a TARDIS Console sketch to run on an Arduino Mega 2560.
 * ("TARDIS Console" in the sense of "Rocketship Busy-board": we make some lights and switches 
 * "do something" abstractly futuristic.)
 * 
 */

#define version_string "version 20210406.008"

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

#define MAJOR_MODE_ROCKET   2
#define MAJOR_MODE_DEMO     0
#define MAJOR_MODE_TARDIS   1
#define MAJOR_MODE_STARTUP   -1

#define MINOR_MODE_IDLE     0
#define MINOR_MODE_TAKEOFF  1
#define MINOR_MODE_FLIGHT   2
#define MINOR_MODE_LANDING  3

typedef struct {
  int value;
  boolean changed;
} Control;

typedef struct {
  int major_mode;
  int minor_mode;
  boolean sound_end_mode_change;
  Control door_lever;
  Control demat_lever;
} Tardis;

Tardis TARDIS = {
  .major_mode = MAJOR_MODE_DEMO,
  .minor_mode = MINOR_MODE_IDLE,
  .sound_end_mode_change = false,
  .door_lever = { .value = -1, .changed = false },
  .demat_lever = { .value = -1, .changed = false }
};



// ** main functions

void setup() {

  TARDIS.major_mode = MAJOR_MODE_STARTUP;
  
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

  // ** play a startup sound

  soundFX_play(sound_startup, SFX_PRIORITY_HIGHEST);

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

void test_major_mode() {
  
  int major_mode = digitalRead(switch_major_mode);
  if (major_mode != TARDIS.major_mode) {
    major_mode_begin(major_mode);
  }
}

void major_mode_begin(int major_mode) {
  
    TARDIS.major_mode = major_mode;
    
    switch (TARDIS.major_mode) {
      
    case MAJOR_MODE_TARDIS:
      digitalWrite(light_demat_middle, LED_OFF);
      digitalWrite(light_demat_top, LED_OFF);
      digitalWrite(light_demat_bottom, LED_OFF);
      TARDIS.door_lever.value = -1;
      TARDIS.door_lever.changed = false;
      TARDIS.minor_mode = MINOR_MODE_IDLE;
      soundFX_play(SFX_6BEEPS, SFX_PRIORITY_HIGHEST);  ///      soundFX_play(SFX_KEYCLIK1, SFX_PRIORITY_HIGHEST);
      break;

    case MAJOR_MODE_ROCKET:
      soundFX_play(SFX_KEYCLIK2, SFX_PRIORITY_HIGHEST);
      break;
    
    case MAJOR_MODE_DEMO:
      digitalWrite(light_demat_bottom, LED_ON);
      // other lights are set directly from levers in this major mode
      soundFX_play(SFX_KACHUNK, SFX_PRIORITY_HIGHEST);
      break;
    }
}

boolean already_playing = false;

void loop_tardis() {

  // check for changed controls

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

  // control changes typically represent commands

  if (TARDIS.door_lever.changed) {
    soundFX_play(SFX_DOORS, SFX_PRIORITY_OPTIONAL);
    TARDIS.door_lever.changed = false;
  }

  if (TARDIS.demat_lever.changed) {
    switch (TARDIS.minor_mode) {
      
      case MINOR_MODE_IDLE:
        TARDIS.minor_mode = MINOR_MODE_TAKEOFF;
        TARDIS.sound_end_mode_change = true;
        soundFX_play(SFX_DEMAT, SFX_PRIORITY_REPLACE);
        break;
        
      case MINOR_MODE_TAKEOFF:
        break;
        
      case MINOR_MODE_FLIGHT:
        TARDIS.minor_mode = MINOR_MODE_LANDING;
        TARDIS.sound_end_mode_change = true;
        soundFX_play(SFX_REMAT, SFX_PRIORITY_REPLACE);
        break;
        
      case MINOR_MODE_LANDING:
        break;
    }
    TARDIS.demat_lever.changed = false;
  }

  // when certain sounds end, minor mode changes.

  if (TARDIS.sound_end_mode_change) {
    if (!soundFX_playing()) {
      if (TARDIS.minor_mode == MINOR_MODE_LANDING) {
        TARDIS.minor_mode = MINOR_MODE_IDLE;
        TARDIS.sound_end_mode_change = false;
      } else if (TARDIS.minor_mode == MINOR_MODE_TAKEOFF) {
        TARDIS.minor_mode = MINOR_MODE_FLIGHT;
        TARDIS.sound_end_mode_change = false;
      }
      // other cases: no effect.
    }
    
  }
  
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
    soundFX_board.volUp();
}

void soundFX_list_files() {
  
    uint8_t files = soundFX_board.listFiles();
    
    Serial.println("SFX File Listing");
    Serial.println("========================");
    Serial.println();
    Serial.print("Found "); Serial.print(files); Serial.println(" Files");
    
    for (uint8_t f=0; f<files; f++) {
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
    Serial.println("rocket...");
    delay(1000);  
}


/* -- NOTES --
 *  
 *  General capabilities:
 *  - debounced switches (in hardware?)
 *  - sounds on switch state changes (different sound each way)
 *  - sound while switch closed (looping)
 *  - sound priorities (one pauses another, 2 levels (background/foreground) is probably enough)
 *  - delay trigger: lights/sounds happen several seconds after the triggering switch event
 *  
 *  Specific operations:
 *  - door lever: sound DoorOpen on switch open; sound DoorClose on switch close (or v.v.)
 *  - mat/demat lever: sound Materialize on switch open; sound Dematerialize on switch close (or v.v.)
 *  - background: sound BackgroundHum while switch is closed, lowest priority (paused by other sounds)
 *  - recall pyramid: three LEDs strobe-flash in a pattern, while a switch is closed but starting delayed
 *  - fault indicator: sound WarningBeeps loops and one LED flashes after a button is pressed, 
 *      starting several seconds delayed, stopping when another button is pressed
 *  - cloister bell: sound ClosterBell repeats while switch (Fast Return) has been closed for ten seconds, 
 *      but starting delayed another ten seconds or more
 *  - keypad buttons: sound ButtonBoopN (for several N at random) when any switch of a group are closed)
 *  - blinkenlights: a group of lights that can flash in at least a "thinking" random mode; more modes ideal
 *  
 */

