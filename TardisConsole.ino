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


#include "Adafruit_Soundboard.h"

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
Serial.println("TARDIS Console");

}

void loop() {

digitalWrite(40, digitalRead(50));
digitalWrite(42, digitalRead(52));

Serial.print("Speed: ");
int val = analogRead(A0);
Serial.print(val);

int door = digitalRead(52);
Serial.print("  Door: ");
Serial.print(door);

int demat = digitalRead(50);
Serial.print("  Demat: ");
Serial.print(demat);

Serial.println("");
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

