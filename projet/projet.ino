#include "SevSeg.h"
#include <WiFi.h>
#include <ThingSpeak.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
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
unsigned long intervalCall = 10000;
unsigned long previousMillisCall = 0;


bool isOnBreak = false; // false = busy true = break
int last_button_value = 0;
int room_number = 298;
int break_time = 0;

// thingspeak
const char ssid[] = "OnePlus Martin"; // Nom du réseau Wi-Fi
const char password[] = "boumshakalaka"; // Mot de passe Wi-Fi
const unsigned long channelID = 2227501; // Remplacez par l'ID de votre canal Thingspeak
const char* apiKey = "Q5SIVG44AM6RV1QY"; // Remplacez par votre clé d'API Thingspeak
int other_class_infos = 0;
bool other_class_bool = false;

WiFiClient client;
HttpClient httpClient = HttpClient(client, "api.thingspeak.com");

void setup() {
  // -- Data rate setup --
  Serial.begin(115200);

  // -- Display setup --
  byte numDigits = 4;
  byte digitsPins[] = {6, 7, 8, 9};
  byte segmentsPins[] = {28, 26, 10, 4, 3, 27, 12, 5};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
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

  if (currentMillis - previousMillisCall > intervalCall) {
    previousMillisCall = currentMillis;
    // Thingspeak
    if(WiFi.status() == WL_CONNECTED) {
      if(isOnBreak){
        ThingSpeak.setField(1, break_time);
      }else{
        ThingSpeak.setField(1, room_number);
      }
      ThingSpeak.setField(2, isOnBreak);
      int response = ThingSpeak.writeFields(channelID, apiKey);

      if(response == 200) {
        Serial.println("Envoi réussi");
      } else {
        Serial.println("Echec de l'envoi");
      }

      httpClient.get("/channels/2227501/feeds.json?api_key=WZRAUBZ3NJ5SKULM&results=1");
      int statusCode = httpClient.responseStatusCode();
      Serial.println(statusCode);
      String responsehttp = httpClient.responseBody();
      Serial.println(responsehttp);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, responsehttp);

      other_class_infos = doc["feeds"][0]["field3"];
      other_class_bool = doc["feeds"][0]["field4"];
      Serial.println(other_class_infos);

      /**
      String url = "https://api.thingspeak.com/channels/2227501/feeds.json?api_key=WZRAUBZ3NJ5SKULM";
      int httpCode = http.begin(url);
      if(httpCode > 0) {
      
          int response = http.GET();

          Serial.println(response);
        
      }

      Serial.println(httpCode);
      Serial.println(HTTP_CODE_OK);
      **/
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