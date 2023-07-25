#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment object

const int led_busy = 0;
const int led_pause = 1;
const int button_mode = 2;
const int defaultTimerValue = 300;
const int clk = 16;
const int dt = 17;
const int interrupt0 = 16;
int count = 0;
volatile int lastCLK = 0;
unsigned long previousMillis = 0;
unsigned long interval = 1000;

bool isOnBreak = false; // false = busy true = break
int last_button_value = 0;
int room_number = 298;
int break_time = 0;

void setup() {
  // -- Display setup --
  byte numDigits = 4;
  byte digitsPins[] = {6, 7, 8, 9};
  byte segmentsPins[] = {28, 26, 10, 4, 3, 27, 12, 5};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros
  // Use 'true' if your decimal point doesn't exist or isn't connected. 
  // Then, you only need to specify 7 segmentPins[]
  bool disableDecPoint = false;

  sevseg.begin(hardwareConfig, numDigits, digitsPins, segmentsPins, resistorsOnSegments,
  updateWithDelays, leadingZeros, disableDecPoint);

  sevseg.setBrightness(100);

  // -- Mode switch setup --
  pinMode(led_busy, OUTPUT);
  pinMode(led_pause, OUTPUT);
  pinMode(button_mode, INPUT);

  // -- Rotary encoder setup --
  pinMode(clk, INPUT);
  pinMode(dt, INPUT);

  attachInterrupt(interrupt0, ClockChanged, CHANGE);

  // -- Data rate setup --
  Serial.begin(9600);
}

void loop() {
  // Display logic
  if (isOnBreak) {
    // Breaktime (s) to displayable format (mm.ss) conversion
    int minutes = break_time / 60;
    int secondes = break_time % 60;

    sevseg.setNumber(minutes * 100 + secondes, 2);
  } else {
    sevseg.setNumber(room_number, 0);
  }

  sevseg.refreshDisplay();

  // Button mode switching
  int button_value = digitalRead(button_mode);

  if(last_button_value == 0 && button_value == 1) {
    isOnBreak = !isOnBreak;

    // When switch to break mode, set default timer to 5 min
    if(isOnBreak) {
      break_time = defaultTimerValue;
    }
  }

  last_button_value = button_value;

  // Change mode led
  if (isOnBreak) { // Break
    digitalWrite(led_busy, LOW);
    digitalWrite(led_pause, HIGH);
  } else { // Busy
    digitalWrite(led_busy, HIGH);
    digitalWrite(led_pause, LOW);
  }

  // Timer logic
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    break_time += break_time > 0 ? -1 : 0;

    // When the timer reaches 0, switch to busy mode, time to work !
    if (break_time == 0) {
      isOnBreak = false;
    }
  }
}

void ClockChanged()
{
  //Read the CLK pi=n level
  int clkValue = digitalRead(clk);
  //Read the DT pin level
  int dtValue = digitalRead(dt);

  if (lastCLK != clkValue)
  {
    lastCLK = clkValue;
    if (isOnBreak) {
      break_time += clkValue != dtValue ? 1 : break_time > 0 ? -1 : 0;
    } else {
      room_number += clkValue != dtValue ? 1 : room_number > 0 ? -1 : 0;
    }
  }
}