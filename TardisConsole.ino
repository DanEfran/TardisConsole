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
#define switch_door 52

// switch sources (some switches connect to adjacent pins rather than to a bus)
#define switch_source_demat_lever 51
#define switch_source_door 53

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

// ** main functions

void setup() {

  // ** establish (optional) serial connection with computer (for debugging etc.)
  
  Serial.begin(115200);
  
  Serial.println("");
  Serial.println("======================");
  Serial.println("==  TARDIS Console  ==");
  Serial.println("======================");
  Serial.println("");

  // ** establish connection with sound effects board and reset it
  
  soundFX_serial.begin(9600);
    
  if (!soundFX_board.reset()) {
    Serial.println("SFX board not found");
  } else {
    Serial.println("SFX board found");
    soundFX_list_files();
  }

  // ** play a startup sound
  
  if (! soundFX_board.playTrack((uint8_t)sound_startup) ) {
        Serial.println("Failed to play startup sound?");
  }

  // ** configure Lights (LED-driving output pins)
  

  pinMode(light_demat_bottom, OUTPUT);
  pinMode(light_demat_middle, OUTPUT);
  pinMode(light_demat_top, OUTPUT);

  digitalWrite(light_demat_bottom, LED_OFF); // bottom demat light
  digitalWrite(light_demat_middle, LED_OFF); // middle demat light
  digitalWrite(light_demat_top, LED_OFF); // top demat light

  // ** configure switches (buttons, levers, etc.)
  
  pinMode(switch_source_demat_lever, OUTPUT);
  pinMode(switch_source_door, OUTPUT);

  digitalWrite(switch_source_demat_lever, SWITCH_SOURCE);
  digitalWrite(switch_source_door, SWITCH_SOURCE);
  
  pinMode(switch_door, INPUT_PULLUP);
  pinMode(switch_demat_lever, INPUT_PULLUP);

}


void loop() {

  // respond crudely to a few controls
  demo_loop();
}

// ** support functions

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
    
// state tracking for demo_loop
int old_speed = 0;
int old_door = 3;
int old_demat = 3;
int printed_something = 0;

void demo_loop() {

  // ** proof-of-concept loop, responds crudely to a few controls
  
  digitalWrite(light_demat_middle, digitalRead(switch_demat_lever));
  digitalWrite(light_demat_top, digitalRead(switch_door));
  
  int val = analogRead(knob_speed);
  if (abs(val - old_speed) > 2) {
    Serial.print("Speed: ");
    Serial.print(val);
    printed_something = 1;
    old_speed = val;
  }
  
  int door = digitalRead(switch_door);
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

