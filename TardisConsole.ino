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


#define SFX_TX 12
#define SFX_RX 13
#define SFX_RST 14
#define SFX_ACT 15

SoftwareSerial ss = SoftwareSerial(SFX_TX, SFX_RX);
Adafruit_Soundboard sfx = Adafruit_Soundboard(&ss, NULL, SFX_RST);

void setup() {
  
  pinMode(38, OUTPUT);
  pinMode(40, OUTPUT);
  pinMode(42, OUTPUT);
  digitalWrite(38, HIGH); // bottom demat light
  digitalWrite(40, HIGH); // middle demat light
  digitalWrite(42, HIGH); // top demat light
  
  pinMode(51, OUTPUT);
  pinMode(53, OUTPUT);
  digitalWrite(51, LOW);
  digitalWrite(53, LOW);
  
  pinMode(52, INPUT_PULLUP);
  pinMode(50, INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.println("");
  Serial.println("=== TARDIS Console ===");
    
  ss.begin(9600);
    
  if (!sfx.reset()) {
    Serial.println("SFX board not found");
  } else {
    Serial.println("SFX board found");
    uint8_t files = sfx.listFiles();
    Serial.println("SFX File Listing");
    Serial.println("========================");
    Serial.println();
    Serial.print("Found "); Serial.print(files); Serial.println(" Files");
    for (uint8_t f=0; f<files; f++) {
      Serial.print(f); 
      Serial.print("\tname: "); Serial.print(sfx.fileName(f));
      Serial.print("\tsize: "); Serial.println(sfx.fileSize(f));
    }
    Serial.println("========================");
  }

  int n = 15;
  if (! sfx.playTrack((uint8_t)n) ) {
        Serial.println("Failed to play track?");
  }
  
}

int old_speed = 0;
int old_door = 3;
int old_demat = 3;
int printed_something = 0;

void loop() {
  
  digitalWrite(40, digitalRead(50));
  digitalWrite(42, digitalRead(52));
  
  int val = analogRead(A0);
  if (abs(val - old_speed) > 2) {
    Serial.print("Speed: ");
    Serial.print(val);
    printed_something = 1;
    old_speed = val;
  }
  
  int door = digitalRead(52);
  if (door != old_door) {
    Serial.print("  Door: ");
    Serial.print(door);
    printed_something = 1;
  }
  old_door = door;
  
  int demat = digitalRead(50);
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

