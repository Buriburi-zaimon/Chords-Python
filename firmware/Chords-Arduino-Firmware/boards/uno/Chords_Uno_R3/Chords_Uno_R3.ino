// Chords_Uno_R3.ino
// Adapted for Arduino Uno R3 from UpsideDownLabs firmware
// Uses TimerOne library for periodic sampling
// Place in firmware/Chords-Arduino-Firmware/boards/uno/Chords_Uno_R3/

#include <Arduino.h>
#include <TimerOne.h>

// ----- CONFIG -----
#define NUM_CHANNELS 6
#define HEADER_LEN 3
#define PACKET_LEN (NUM_CHANNELS * 2 + HEADER_LEN + 1)
#define SAMP_RATE 500.0            // desired sampling rate (Hz)
#define SYNC_BYTE_1 0xC7
#define SYNC_BYTE_2 0x7C
#define END_BYTE 0x01
#define BAUD_RATE 115200           // UNO: 115200 is safe and reliable

// ----- Globals -----
uint8_t packetBuffer[PACKET_LEN];
volatile bool timerStatus = false;
volatile bool bufferReady = false;
volatile uint8_t packetCounter = 0;

// Forward declarations
void timerISR();
bool timerStart();
bool timerStop();
bool timerBegin(float sampling_rate);

void setup() {
  Serial.begin(BAUD_RATE);
  // DON'T block waiting for Serial on UNO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // packet init
  packetBuffer[0] = SYNC_BYTE_1;
  packetBuffer[1] = SYNC_BYTE_2;
  packetBuffer[2] = packetCounter;                 // counter
  packetBuffer[PACKET_LEN - 1] = END_BYTE;

  // Timer initialization (does not start timer)
  timerBegin(SAMP_RATE);
}

void loop() {
  // If buffer ready, send packet (do Serial.write in main loop, not ISR)
  if (timerStatus && bufferReady) {
    packetBuffer[2] = packetCounter;
    Serial.write(packetBuffer, PACKET_LEN);
    bufferReady = false;
  }

  // handle simple commands (WHORU, START, STOP, STATUS)
  if (Serial.available()) {
    String command = Serial.readStringUntil('\\n');
    command.trim();
    command.toUpperCase();

    if (command == "WHORU") {
      Serial.println("UNO-R3");
    } else if (command == "START") {
      if (timerStart()) {
        Serial.println("OK");
      } else {
        Serial.println("ERR");
      }
    } else if (command == "STOP") {
      timerStop();
      Serial.println("OK");
    } else if (command == "STATUS") {
      Serial.println(timerStatus ? "RUNNING" : "STOPPED");
    } else {
      Serial.println("UNKNOWN COMMAND");
    }
  }
}

/* Timer / sampling functions */

void timerISR() {
  if (!timerStatus) return;

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    uint16_t v = analogRead(ch);               // UNO returns 0..1023
    packetBuffer[HEADER_LEN + (ch * 2)] = highByte(v);
    packetBuffer[HEADER_LEN + (ch * 2) + 1] = lowByte(v);
  }

  packetCounter++;
  packetBuffer[2] = packetCounter;
  bufferReady = true;
}

bool timerBegin(float sampling_rate) {
  unsigned long period_us = (unsigned long)(1000000.0 / sampling_rate);
  if (period_us < 2) period_us = 2;
  Timer1.initialize(period_us);
  return true;
}

bool timerStart() {
  if (timerStatus) return true;
  bufferReady = false;
  packetCounter = 0;
  Timer1.attachInterrupt(timerISR);
  timerStatus = true;
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}

bool timerStop() {
  if (!timerStatus) return true;
  Timer1.detachInterrupt();
  timerStatus = false;
  bufferReady = false;
  digitalWrite(LED_BUILTIN, LOW);
  return true;
}
