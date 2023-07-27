#include "SevSeg.h"
#include <WiFi.h>
#include <ThingSpeak.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
SevSeg sevseg;  //Instantiate a seven segment object

const int my_led_busy = 0;
const int my_led_pause = 1;
const int other_led_busy = 15;
const int other_led_pause = 14;
const int button_mode = 2;
const int defaultTimerValue = 300;
const int clk = 16;
const int dt = 17;
const int class_button = 18;
const int interrupt0 = 16;
int count = 0;
volatile int lastCLK = 0;
unsigned long previousMillis = 0;
unsigned long interval = 1000;
unsigned long intervalCall = 10000;
unsigned long previousMillisCall = 0;


bool isOnBreak = false;  // false = busy true = break
bool isMe = true;        // false = mtd true = dev
int last_button_mode_value = 0;
int last_button_class_value = 0;
int my_room_number = 298;
int my_break_time = 0;
int other_class_infos = 0;
bool other_class_bool = false;

// constants won't change. They're used here to set pin numbers:
int LONG_PRESS_TIME  = 500; // 500 milliseconds

// Variables will change
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

// thingspeak
const char ssid[] = "OnePlus Martin";     // Nom du réseau Wi-Fi
const char password[] = "boumshakalaka";  // Mot de passe Wi-Fi
const unsigned long channelID = 2228138;  // Remplacez par l'ID de votre canal Thingspeak
const char* writeApiKey = "0GWNH1LROW60G7FD";  // Remplacez par votre clé d'API Thingspeak
const char* readApiKey = "YJSR2IQDA6XRMG7F";  // Remplacez par votre clé d'API Thingspeak

WiFiClient client;
HttpClient httpClient = HttpClient(client, "api.thingspeak.com");

void setup() {
  // -- Data rate setup --
  Serial.begin(115200);
  pinMode(button_mode, INPUT_PULLUP);

  // -- Display setup --
  byte numDigits = 4;
  byte digitsPins[] = { 6, 7, 8, 9 };
  byte segmentsPins[] = { 28, 26, 10, 4, 3, 27, 12, 5 };
  bool resistorsOnSegments = false;    // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE;  // See README.md for options
  bool updateWithDelays = false;       // Default 'false' is Recommended
  bool leadingZeros = false;           // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false;

  sevseg.begin(hardwareConfig, numDigits, digitsPins, segmentsPins, resistorsOnSegments,
               updateWithDelays, leadingZeros, disableDecPoint);

  sevseg.setBrightness(100);

  // -- Mode switch setup --
  pinMode(my_led_busy, OUTPUT);
  pinMode(my_led_pause, OUTPUT);
  pinMode(other_led_busy, OUTPUT);
  pinMode(other_led_pause, OUTPUT);
  pinMode(button_mode, INPUT);
  pinMode(class_button, INPUT);

  // -- Rotary encoder setup --
  pinMode(clk, INPUT);
  pinMode(dt, INPUT);

  attachInterrupt(interrupt0, ClockChanged, CHANGE);

  // -- Wifi setup --
  while (!Serial) {}

  // Connexion au réseau Wi-Fi
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Le module WiFi n'est pas présent.");
    while (true);
  }

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.println("Connexion au réseau Wi-Fi en cours...");
    delay(5000);
  }

  Serial.println("Connecté au réseau Wi-Fi");

  // -- Thingspeak setup --
  ThingSpeak.begin(client);
}

void loop() {
  if (isMe) {
    // Display logic
    if (isOnBreak) {
      // Breaktime (s) to displayable format (mm.ss) conversion
      int minutes = my_break_time / 60;
      int secondes = my_break_time % 60;

      sevseg.setNumber(minutes * 100 + secondes, 2);
    } else {
      sevseg.setNumber(my_room_number, 0);
    }
  } else {
    if (other_class_bool) {
      // Breaktime (s) to displayable format (mm.ss) conversion
      int minutes = other_class_infos / 60;
      int secondes = other_class_infos % 60;
      sevseg.setNumber(minutes * 100 + secondes, 2);
    } else {
      sevseg.setNumber(other_class_infos, 0);
    }
  }

  sevseg.refreshDisplay();

  // Button mode switching
  int button_value = digitalRead(button_mode);

  if (last_button_mode_value == 0 && button_value == 1) {
    pressedTime = millis();
  } else if (last_button_mode_value == 1 && button_value == 0) { // button is released
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime; 

    if (pressDuration > LONG_PRESS_TIME ) {
      isMe = !isMe;   
    } else {
      if(isMe) {
        isOnBreak = !isOnBreak;
        // When switch to break mode, set default timer to 5 min
        if(isOnBreak) {
          my_break_time = defaultTimerValue;
        }
      }
    }
  }
  last_button_mode_value = button_value;

  if (isMe) {
    digitalWrite(other_led_busy, LOW);
    digitalWrite(other_led_pause, LOW);
    if (isOnBreak) {  // Break
      digitalWrite(my_led_busy, LOW);
      digitalWrite(my_led_pause, HIGH);
    } else {  // Busy
      digitalWrite(my_led_busy, HIGH);
      digitalWrite(my_led_pause, LOW);
    }
  } else {
    digitalWrite(my_led_busy, LOW);
    digitalWrite(my_led_pause, LOW);
    if (other_class_bool) { // Break
      digitalWrite(other_led_busy, LOW);
      digitalWrite(other_led_pause, HIGH);
    } else { // Busy
      digitalWrite(other_led_busy, HIGH);
      digitalWrite(other_led_pause, LOW);
    }
  }
  // Timer logic
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    my_break_time += my_break_time > 0 ? -1 : 0;

    // When the timer reaches 0, switch to busy mode, time to work !
    if (my_break_time == 0 && isMe) {
      isOnBreak = false;
    }
  }

  if (!isMe && other_class_bool) {
    previousMillis = currentMillis;
    other_class_infos += other_class_infos > 0 ? -1 : 0;
  }

  if (currentMillis - previousMillisCall > intervalCall) {
    previousMillisCall = currentMillis;
    // Thingspeak
    if (WiFi.status() == WL_CONNECTED) {
      if (isOnBreak) {
        ThingSpeak.setField(1, my_break_time);
      } else {
        ThingSpeak.setField(1, my_room_number);
      }
      ThingSpeak.setField(2, isOnBreak);
      int response = ThingSpeak.writeFields(channelID, writeApiKey);

      if (response == 200) {
        Serial.println("Envoi réussi");
      } else {
        Serial.println("Echec de l'envoi");
      }

      httpClient.get("/channels/" + String(channelID) + "/feeds.json?api_key=" + String(readApiKey) + "&results=1");
      int statusCode = httpClient.responseStatusCode();
      // Serial.println(statusCode);
      String responsehttp = httpClient.responseBody();
      // Serial.println(responsehttp);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, responsehttp);

      other_class_infos = doc["feeds"][0]["field1"];
      other_class_bool = doc["feeds"][0]["field2"];
      Serial.println(other_class_infos);
    }
  }
}

void ClockChanged() {
  //Read the CLK pi=n level
  int clkValue = digitalRead(clk);
  //Read the DT pin level
  int dtValue = digitalRead(dt);

  if (lastCLK != clkValue) {
    lastCLK = clkValue;
    if (isOnBreak) {
      my_break_time += clkValue != dtValue ? -1 : my_break_time > 0 ? +1
                                                                    : 0;
    } else {
      my_room_number += clkValue != dtValue ? -1 : my_room_number > 0 ? +1
                                                                      : 0;
    }
  }
}