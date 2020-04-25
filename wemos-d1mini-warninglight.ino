/*********
  wemos-d1mini-warninglight.ino
  Created on  : 2020-04-25
  Author      : Bruno Brito Semedo

  Wemos D1 Mini sends an HTTP request to the specified status endpoint
  if the status is "pass", the warning light is switched off
  else if the status is "fail", the warning light is switched on
  The warning light stays switch on as long as the status is not "pass"
*********/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";
const char* endpoint_url = "REPLACE_WITH_YOUR_ENDPOINT_URL";
const uint8_t pin_relay = 5;
const uint8_t setup_wait = 5;
const uint8_t check_delay = 15000;

void setup() {
  // initialize pin
  pinMode(pin_relay, OUTPUT);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = setup_wait; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
        
    // indicate visual cue for setup
    digitalWrite(pin_relay, (t % 2) ? LOW : HIGH);
    delay(1000);
  }
  // keep the warning light on
  digitalWrite(pin_relay, HIGH);

  // connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);
}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;

    HTTPClient http;
    http.setTimeout(5000);

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, endpoint_url)) {
      Serial.print("[HTTP] GET...\n");
      int httpCode = http.GET();

      if (httpCode > 0) { // httpCode will be negative on error
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {          
          String payload = http.getString();
          Serial.println(payload);
          
          if ((payload == "ok" || payload == "up" || payload == "pass")
            && digitalRead(pin_relay) == HIGH) { // switch off the warning light, if on
            digitalWrite(pin_relay, LOW);          
          } else if (payload == "fail"
            && digitalRead(pin_relay) == LOW) {  // switch on the warning light, if off
            digitalWrite(pin_relay, HIGH);
          }
        }
      } else { // error with unsupported HTTP code
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        if (digitalRead(pin_relay) == LOW) {
          digitalWrite(pin_relay, HIGH);
        }
      }

      http.end();
    } else { // unable to contact server
      Serial.printf("[HTTP] Unable to connect\n");
      if (digitalRead(pin_relay) == LOW) {
        digitalWrite(pin_relay, HIGH);
      }
    }
  }

  // wait for specified duration before requesting current status
  delay(check_delay);
}
