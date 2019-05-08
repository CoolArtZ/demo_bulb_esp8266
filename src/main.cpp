#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseArduino.h>
#include "Creds.h"

#include <Wire.h>
#include "SSD1306Wire.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN D5
#define DHTTYPE DHT11

SSD1306Wire  display(0x3c, D2, D1);
DHT_Unified dht(DHTPIN, DHTTYPE);

int led_lolin = D4; // Lolin board can't use LED_BUILTIN definition
int powerAC = D6;
int bulb = D7;
int led_powerAC = D8;
int led_state = 1, power_state = 1;
int count_delay = 0; //delay for getting temp & humid data
int count_delay_update = 0; //delay for updating database
float temp = 0.0;
float humid = 0.0;

void connectToWiFi(){
  delay(10);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Wifi connected to ");
  Serial.println(SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void setup_init_state() {
  digitalWrite(bulb, Firebase.getInt("/LedStatus"));
  digitalWrite(powerAC, Firebase.getInt("/PowerAC"));
  digitalWrite(led_powerAC, (Firebase.getInt("/PowerAC"))^1);
}

void drawPeripheralStatus(int led_state, int power_state, float temp, float humid) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if(!led_state)
    display.drawString(0, 0, "LED: ON");
  else
    display.drawString(0, 0, "LED: OFF");
  if(!power_state)
    display.drawString(0, 16, "Power AC: ON");
  else
    display.drawString(0, 16, "Power AC: OFF");
  display.drawString(0, 32, "Temp: " + String(temp));
  display.drawString(0, 48, "Humid: " + String(humid));
}

void setup() {
  pinMode(bulb, OUTPUT); //active low
  pinMode(led_lolin, OUTPUT); //active low
  pinMode(powerAC, OUTPUT); //active low
  pinMode(led_powerAC, OUTPUT); //active high

  digitalWrite(bulb, HIGH); //turn off at start
  digitalWrite(led_lolin, HIGH); //turn off until init ready
  digitalWrite(powerAC, HIGH); //turn off at start
  digitalWrite(led_powerAC, LOW); //turn off at start
  
  //pinMode(LED_BUILTIN, OUTPUT);

  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 16, "Initializing...");
  display.display();

  Serial.begin(9600);
  
  connectToWiFi();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
  setup_init_state();

  dht.begin();
  digitalWrite(led_lolin, LOW); //turn on, indicates init ready

  Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)
}

void loop() {
  if(Firebase.failed()){
    Serial.print("[ERROR] setting LED Status failed");
    Serial.println(Firebase.error());
    delay(1000); 
    Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)
    return;
  }

  if(Firebase.available()){
    FirebaseObject event = Firebase.readEvent();
    String eventType = event.getString("type");
    eventType.toLowerCase();

    Serial.print("event: ");
    Serial.println(eventType);
    if(eventType == "put") {
      int data = event.getInt("data");
      String path = event.getString("path");
      Serial.print("data: ");
      Serial.println(data);
      Serial.print("path: ");
      Serial.println(path);

      if(path == "/LedStatus") {
        if(!data) {
          digitalWrite(bulb, LOW);
        }
        else {
          digitalWrite(bulb, HIGH);
        }
        led_state = data;
      }
      else if(path == "/PowerAC") {
        if(!data) {
          digitalWrite(powerAC, LOW);
          digitalWrite(led_powerAC, HIGH);
        }
        else {
          digitalWrite(powerAC, HIGH);
          digitalWrite(led_powerAC, LOW);
        }
        power_state = data;
      }
    }
    Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)

  }

  count_delay++;
  if(count_delay >= 200)
  {
    sensors_event_t event;

    dht.temperature().getEvent(&event);
    if(isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    }
    else {
      Serial.print(F("Temperature: "));
      Serial.print(event.temperature);
      Serial.println(F("°C"));
      temp = event.temperature;
    }

    dht.humidity().getEvent(&event);
    if(isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    }
    else {
      Serial.print(F("Humidity: "));
      Serial.print(event.relative_humidity);
      Serial.println(F("%"));
      humid = event.relative_humidity;
    }

    if(count_delay_update >= 5) {  //update to database approx every 10 seconds
      //Firebase.setFloat("/Temperature", temp);
      //Firebase.setFloat("/Humidity", humid);
      count_delay_update = 0;
    }
    else
      count_delay_update++;

    count_delay = 0;
    Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)
  }

  display.clear();
  drawPeripheralStatus(led_state, power_state, temp, humid);
  display.display();
  delay(10);
}