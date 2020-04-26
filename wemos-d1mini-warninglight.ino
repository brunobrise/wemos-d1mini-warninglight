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
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;
AsyncWebServer server(80);

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";
const char* endpoint_url_default = "";
const uint8_t pin_relay = 5;
const uint8_t setup_wait = 5;
const uint8_t check_delay = 15000;

const char* PARAM_ENDPOINT_URL = "endpoint_url";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Warning Light</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
  </script></head><body>
  <form action="/configure" target="hidden-form">
    <label for="endpoint_url">Endpoint URL</label>
    <input type="text " name="endpoint_url" placeholder="%endpoint_url%" value="%endpoint_url%">
    <input type="submit" value="Save" onclick="submitMessage()">
  </form>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_html, processor);
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "");
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("[FILE] READ %s:\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("[FILE] OPEN... failure");
    return String();
  }
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("[FILE] WRITE %s:\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("[FILE] OPEN... failure");
    return;
  }
  if(file.print(message)){
    Serial.println("[FILE] WRITE... success");
  } else {
    Serial.println("[FILE] WRITE... failure");
  }
}

void configure(AsyncWebServerRequest *request) {
    Serial.println("[HTTP] /configure");
    String inputMessage;
    if (request->hasParam(PARAM_ENDPOINT_URL)) {
      inputMessage = request->getParam(PARAM_ENDPOINT_URL)->value();
      writeFile(SPIFFS, "/endpoint_url.txt", inputMessage.c_str());
    } else {
      inputMessage = "[HTTP] /configure: No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
}

// replace placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "endpoint_url"){
    return readFile(SPIFFS, "/endpoint_url.txt");
  }
  return String();
}

void setup() {
  // initialize pin
  pinMode(pin_relay, OUTPUT);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  // initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("[SPIFFS] Error on mount");
    return;
  }
  // store default values
  writeFile(SPIFFS, "/endpoint_url.txt", endpoint_url_default);

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

  // wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // show IP address
  Serial.println("");
  Serial.println("[SETUP] WiFi connected");
  Serial.print("[SETUP] IP address: ");
  Serial.println(WiFi.localIP());

  // Set routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/configure", HTTP_GET, configure);
  server.onNotFound(notFound);
  server.begin();
}

void loop() {
  // check WiFi connection (connect if required)
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClient client;

    HTTPClient http;
    http.setTimeout(5000);

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, readFile(SPIFFS, "/endpoint_url.txt"))) {
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
