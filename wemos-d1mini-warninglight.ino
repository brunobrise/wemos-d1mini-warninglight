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
const unsigned int check_period_default = 15000;

const char* PARAM_ENDPOINT_URL = "endpoint_url";
const char* PARAM_CHECK_PERIOD = "check_period";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Dashboard | Warning Light</title>
  <link rel='icon' href='data:image/png;base64,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' type='image/x-png' />
  <link href="https://unpkg.com/tailwindcss@^1.0/dist/tailwind.min.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    .fail {
      background-color: red;
    }
    .pass {
      background-color: green;
    }
  </style>
  <script>
    function submitMessage() {
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
    setTimeout(function(){ document.location.reload(false); }, 5000);
  </script></head><body>
  <nav class="flex items-center justify-between flex-wrap bg-black p-6">
    <div class="flex items-center flex-shrink-0 text-white mr-6">
      <svg class="fill-current h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path d="M15.9 5c-.17 0-.32.09-.41.23l-.07.15-5.18 11.65c-.16.29-.26.61-.26.96 0 1.11.9 2.01 2.01 2.01.96 0 1.77-.68 1.96-1.59l.01-.03L16.4 5.5c0-.28-.22-.5-.5-.5zM1 9l2 2c2.88-2.88 6.79-4.08 10.53-3.62l1.19-2.68C9.89 3.84 4.74 5.27 1 9zm20 2l2-2c-1.64-1.64-3.55-2.82-5.59-3.57l-.53 2.82c1.5.62 2.9 1.53 4.12 2.75zm-4 4l2-2c-.8-.8-1.7-1.42-2.66-1.89l-.55 2.92c.42.27.83.59 1.21.97zM5 13l2 2c1.13-1.13 2.56-1.79 4.03-2l1.28-2.88c-2.63-.08-5.3.87-7.31 2.88z"/></svg>
      <span class="font-semibold text-xl tracking-tight">Dashboard</span>
    </div>
  </nav>
  <div class="w-full px-4">
    <div class="flex flex-col">
      <div class="flex flex-row items-center justify-between shadow-xs border rounded font-semibold p-2 mt-4 focus:outline-none focus:shadow-outline">
        <p class="text-xs uppercase w-5/6 overflow-hidden">%endpoint_url%</p>
        <div class="bg-black h-8 w-8 shadow border-2 border-gray-800 rounded-full %check_status%"></div>
      </div>
    </div>
    <a class="block text-center text-xs underline mt-6" href="/admin">Go to Admin</a>
  </div>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Administration | Warning Light</title>
  <link rel='icon' href='data:image/png;base64,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' type='image/x-png' />
  <link href="https://unpkg.com/tailwindcss@^1.0/dist/tailwind.min.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
  </script></head><body>
  <nav class="flex items-center justify-between flex-wrap bg-black p-6">
    <div class="flex items-center flex-shrink-0 text-white mr-6">
      <svg class="fill-current h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path d="M10.82 12.49c.02-.16.04-.32.04-.49 0-.17-.02-.33-.04-.49l1.08-.82c.1-.07.12-.21.06-.32l-1.03-1.73c-.06-.11-.2-.15-.31-.11l-1.28.5c-.27-.2-.56-.36-.87-.49l-.2-1.33c0-.12-.11-.21-.24-.21H5.98c-.13 0-.24.09-.26.21l-.2 1.32c-.31.12-.6.3-.87.49l-1.28-.5c-.12-.05-.25 0-.31.11l-1.03 1.73c-.06.12-.03.25.07.33l1.08.82c-.02.16-.03.33-.03.49 0 .17.02.33.04.49l-1.09.83c-.1.07-.12.21-.06.32l1.03 1.73c.06.11.2.15.31.11l1.28-.5c.27.2.56.36.87.49l.2 1.32c.01.12.12.21.25.21h2.06c.13 0 .24-.09.25-.21l.2-1.32c.31-.12.6-.3.87-.49l1.28.5c.12.05.25 0 .31-.11l1.03-1.73c.06-.11.04-.24-.06-.32l-1.1-.83zM7 13.75c-.99 0-1.8-.78-1.8-1.75s.81-1.75 1.8-1.75 1.8.78 1.8 1.75S8 13.75 7 13.75zM18 1.01L8 1c-1.1 0-2 .9-2 2v3h2V5h10v14H8v-1H6v3c0 1.1.9 2 2 2h10c1.1 0 2-.9 2-2V3c0-1.1-.9-1.99-2-1.99z"/></svg>
      <span class="font-semibold text-xl tracking-tight">Admin</span>
    </div>
  </nav>
  <div class="w-full px-4">
    <form class="flex flex-col mt-4" action="/configure" target="hidden-form">
      <label class="flex-grow text-gray-700 text-sm font-bold" for="endpoint_url">Endpoint URL</label>
      <div class="flex flex-row flex-grow-0">
        <input class="flex-grow shadow appearance-none border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" type="text " name="endpoint_url" placeholder="%endpoint_url%" value="%endpoint_url%">
        <input class="flex-grow-0 bg-green-700 hover:bg-green-700 text-white font-bold py-2 px-4 rounded focus:outline-none focus:shadow-outline ml-1" type="submit" value="Save" onclick="submitMessage()">
      </div>    
    </form>
    <form class="flex flex-col mt-4" action="/configure" target="hidden-form">
      <label class="flex-grow text-gray-700 text-sm font-bold" for="check_period">Check period</label>
      <div class="flex flex-row flex-grow-0">
        <input class="flex-grow shadow appearance-none border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" type="text " name="check_period" placeholder="%check_period%" value="%check_period%">
        <input class="flex-grow-0 bg-green-700 hover:bg-green-700 text-white font-bold py-2 px-4 rounded focus:outline-none focus:shadow-outline ml-1" type="submit" value="Save" onclick="submitMessage()">
      </div>    
    </form>
    <form class="flex items-center mt-8" action="/reset" target="hidden-form">
      <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Reset" onclick="submitMessage()">
    </form>
    <form class="flex items-center mt-4" action="/restart" target="hidden-form">
      <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Restart" onclick="submitMessage()">
    </form>
    <a class="block text-center text-xs underline mt-6" href="/">Go to Dashboard</a>
  </div>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

const char redirect_home[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Warning Light</title>
  <link rel='icon' href='data:image/png;base64,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' type='image/x-png' />
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="0; URL='/'" />
</head><body>
</body></html>)rawliteral";

void logRoute(AsyncWebServerRequest *request) {
  Serial.print("[HTTP] ");
  Serial.print(request->client()->remoteIP());
  Serial.print(" ");
  Serial.println(request->url());
}

void handleRoot(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send_P(200, "text/html", index_html, processor);
}

void handleAdmin(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send_P(200, "text/html", admin_html, processor);
}

void notFound(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(404, "text/plain", "");
}

void handleReset(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", redirect_home);
  delay(2000);
  reset();
}

void handleRestart(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", redirect_home);
  delay(2000);
  restart();
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("[FILE] READ %s...\r\n", path);
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
  Serial.printf("[FILE] WRITE %s...\r\n", path);
  Serial.println(message);
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
  file.close();
}

void configure(AsyncWebServerRequest *request) {
  logRoute(request);
  String inputMessage;
  if (request->hasParam(PARAM_ENDPOINT_URL)) {
    inputMessage = request->getParam(PARAM_ENDPOINT_URL)->value();
    writeFile(SPIFFS, "/endpoint_url.txt", inputMessage.c_str());
  } else if (request->hasParam(PARAM_CHECK_PERIOD)) {
    inputMessage = request->getParam(PARAM_CHECK_PERIOD)->value();
    writeFile(SPIFFS, "/check_period.txt", inputMessage.c_str());
  } else {
    inputMessage = "[HTTP] /configure: No message sent";
  }
  request->send(200, "text/text", inputMessage);
}

// replace placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "endpoint_url"){
    return readFile(SPIFFS, "/endpoint_url.txt");
  } else if(var == "check_period") {
    return readFile(SPIFFS, "/check_period.txt");
  } else if(var == "check_status") {
    return readFile(SPIFFS, "/check_status.txt");
  }
  return String();
}

void reset() {
  ESP.reset();
  delay(1000);
}

void restart() {
  ESP.restart();
  delay(1000);
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
  if(readFile(SPIFFS, "/endpoint_url.txt") == "") {
    writeFile(SPIFFS, "/endpoint_url.txt", endpoint_url_default);
  }
  if(readFile(SPIFFS, "/check_period.txt") == "") {
    char cstr[16];
    const char* check_period = itoa(check_period_default, cstr, 10);
    writeFile(SPIFFS, "/check_period.txt", check_period);
  }

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
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/configure", HTTP_GET, configure);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/restart", HTTP_GET, handleRestart);
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
            payload = "pass";
          } else if (payload == "fail"
            && digitalRead(pin_relay) == LOW) {  // switch on the warning light, if off
            digitalWrite(pin_relay, HIGH);
            payload = "fail";
          }
          char status[payload.length()+1];
          payload.toCharArray(status, payload.length()+1);
          writeFile(SPIFFS, "/check_status.txt", status);
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
  delay(atol(readFile(SPIFFS, "/check_period.txt").c_str()));
}
