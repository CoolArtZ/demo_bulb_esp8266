#include <Arduino.h>
#include <ESP8266WiFi.h> // required by Firebase & WiFiManager
#include <ESP8266WebServer.h> // required by WiFiManager
#include <DNSServer.h> // required by WiFiManager
#include <WiFiManager.h> // https://github.com/kentaylor/WiFiManager
#include <ESP8266HTTPClient.h> // required by Firebase
#include <FirebaseArduino.h> // Firebase for Arduino library
#include "Creds.h" // store Firebase login credentials

#include <Wire.h> // required by OLED LCD
#include "SSD1306Wire.h" // OLED LCD library
#include <Adafruit_Sensor.h> // required by OLED LCD
#include <DHT.h> // Digital Humidity & Temperature sensor library
#include <DHT_U.h> // Digital Humidity & Temperature sensor library

//---Pinout & Configurations---//
#define DHTPIN D5
#define DHTTYPE DHT11
#define DHT_INTERVAL 2000 //DHT capture interval of 2 seconds
#define DHT_UPDATE_INTERVAL 15 //update database every 15 data capture, approx. (DHT_update_count * (DHT_INTERVAL/1000)) seconds

SSD1306Wire display(0x3c, D2, D1); // D2:SDA, D1:SCL
DHT_Unified dht(DHTPIN, DHTTYPE);

int led_lolin = D4; // Lolin board can't use LED_BUILTIN definition, built-in led located @ D4
int powerAC = D6;
int bulb = D7;
int led_powerAC = D8;
int led_state = 1, power_state = 1;

unsigned long DHT_startAt = 0; //DHT time-stamp
int DHT_count = 0; //count number of times data captured
float temp = 0.0;
float humid = 0.0;

//---End of Pinout & Configurations---//

void setup_init_state() {
  digitalWrite(bulb, Firebase.getInt("/LedStatus"));
  digitalWrite(powerAC, Firebase.getInt("/PowerAC"));
  digitalWrite(led_powerAC, (Firebase.getInt("/PowerAC"))^1);
  led_state = Firebase.getInt("/LedStatus");
  power_state = Firebase.getInt("/PowerAC");
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

  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 16, "Connecting...");
  display.display();

  Serial.begin(9600);
  
  //---Configure WiFi---//
  Serial.println("\n Starting");
  unsigned long startedAt = millis();
  Serial.println("Opening configuration portal");
  
  WiFiManager wifiManager;
  //wifiManager.resetSettings(); //uncomment this to erase the saved connection details

  //sets timeout in seconds until configuration portal gets turned off.
  //If not specified device will remain in configuration mode until switched off via webserver.
  if (WiFi.SSID()!="") wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.

  //it starts an access point and goes into a blocking loop awaiting configuration
  if (!wifiManager.startConfigPortal("bulb@saur","compeng2019"))  //Delete these two parameters if you do not want a WiFi password on your configuration access point
  {
     Serial.println("Not connected to WiFi but continuing anyway.");
  } 
  else 
  {
     //if you get here you have connected to the WiFi
     Serial.println("connected...yeey :)");
  }

  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis()- startedAt);
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED)
  {
      Serial.println("failed to connect, finishing setup anyway");
  } 
  else
  {
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());
  }
  //---End of WiFi Configuration---//

  display.drawString(0, 16, "Initializing...");
  display.display();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
  setup_init_state();

  dht.begin();
  
  digitalWrite(led_lolin, LOW); //turn on, indicates init ready

  DHT_startAt = millis(); //start counting for the next DHT sensor capture

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
          digitalWrite(bulb, LOW);  //bulb ON
        }
        else {
          digitalWrite(bulb, HIGH); //bulb off
        }
        led_state = data;
      }
      else if(path == "/PowerAC") {
        if(!data) {
          digitalWrite(powerAC, LOW); //power ON
          digitalWrite(led_powerAC, HIGH);
        }
        else {
          digitalWrite(powerAC, HIGH); //power OFF
          digitalWrite(led_powerAC, LOW);
        }
        power_state = data;
      }
    }
    Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)

  }

  if(DHT_startAt - millis() >= DHT_INTERVAL)
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

    if(DHT_count >= DHT_UPDATE_INTERVAL) { 
      //Firebase.setFloat("/Temperature", temp);
      //Firebase.setFloat("/Humidity", humid);
      DHT_count = 0; //restart count from 0
    }
    else
      DHT_count++;

    DHT_startAt = millis();
    Firebase.stream("/"); //monitor all activity (LedStatus and PowerAC)
  }

  display.clear();
  drawPeripheralStatus(led_state, power_state, temp, humid);
  display.display();
  delay(10);
}