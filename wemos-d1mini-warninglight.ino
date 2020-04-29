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
#include <ArduinoOTA.h>
#include <DoubleResetDetect.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// maximum number of seconds between resets that
// counts as a double reset 
#define DRD_TIMEOUT 2.0

// address to the block in the RTC user memory
#define DRD_ADDRESS 0x00

ESP8266WiFiMulti WiFiMulti;
AsyncWebServer server(80);
DNSServer dns;
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

const uint8_t pin_relay = 5;
const uint8_t setup_wait = 5;

const char* endpoint_url_default = "";
const unsigned long check_interval_default = 15000;
unsigned long check_interval_time = 0;
const unsigned long check_retry_default = 3;

const char* PARAM_ENDPOINT_URL = "endpoint_url";
const char* PARAM_CHECK_INTERVAL = "check_interval";
const char* PARAM_CHECK_RETRY = "check_retry";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Dashboard | Warning Light</title>
  <link rel='icon' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAAAmJLR0QAAKqNIzIAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAHdElNRQfkBBoWGzVsCzJ0AAABCUlEQVQoz1XRPS9DcRQG8J9b0U4WL4uEUQQju6HxQdrZbJT4ABT1sgiJfgSLUFZCDNpBw9IYhKRNsDS01+De6/ac5X+e8+R5zvkf4phXUvPlS82mOX2RtasrTOWPHUP/7apQ6E3ed4p0EVP2IqCClz6d7T/vWLyIex9uU0azgYIgsrrEmyN5nQjJKATyUdH0bNS7G22nyfj5wGT0rJo25921CScJYWrQugFwYUnLoyfHilYxLCPMyOpoaxuxouFUoKKlqe1Vwx1bqbXWsNy36EbgQC9xHMdi6od7DgN1+wkwhoUUoewBcs4jwSu8JvJnsjEzZ0dP6FM5anaV5PovOmtLPTp3yUwM/wLOmnuR0Rs7yQAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAyMC0wNC0yNlQyMjoyNzo1MyswMDowMCDTVZQAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMjAtMDQtMjZUMjI6Mjc6NTMrMDA6MDBRju0oAAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAABJRU5ErkJggg==' type='image/x-png' />
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
  <nav class="flex items-center justify-between flex-wrap bg-black p-6 mb-4">
    <div class="flex items-center flex-shrink-0 text-white mr-6">
      <svg class="fill-current h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 576 512" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path fill="currentColor" d="M288 288c-6.04 0-11.77 1.11-17.3 2.67l-75.76-97.4c-8.12-10.45-23.12-12.38-33.69-4.2-10.44 8.12-12.34 23.2-4.19 33.67l75.75 97.39c-5.45 9.42-8.81 20.21-8.81 31.88 0 11.72 3.38 22.55 8.88 32h110.25c5.5-9.45 8.88-20.28 8.88-32C352 316.65 323.35 288 288 288zm0-256C128.94 32 0 160.94 0 320c0 52.8 14.25 102.26 39.06 144.8 5.61 9.62 16.3 15.2 27.44 15.2h443c11.14 0 21.83-5.58 27.44-15.2C561.75 422.26 576 372.8 576 320c0-159.06-128.94-288-288-288zm212.27 400H75.73C57.56 397.63 48 359.12 48 320 48 187.66 155.66 80 288 80s240 107.66 240 240c0 39.12-9.56 77.63-27.73 112z" class=""></path></svg>
      <span class="font-semibold text-xl tracking-tight">Dashboard</span>
    </div>
  </nav>
  <div class="w-full px-4">
    <div class="flex flex-col">
      <div class="flex flex-row items-center justify-between shadow-xs border rounded font-semibold p-2 focus:outline-none focus:shadow-outline">
        <p class="text-xs uppercase w-5/6 overflow-hidden">%endpoint_url%</p>
        <div class="bg-black h-8 w-8 shadow border-2 border-gray-800 rounded-full %check_status%"></div>
      </div>
    </div>
    <div class="flex flex-wrap justify-center mt-4">
      <h1 class="text-xl font-bold mb-2 text-gray-900">SYSTEM INFORMATION</h1>
      <div class="w-full flex justify-center items-center text-black mr-6">
        <svg class="fill-current text-gray-800  h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 512 512" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path fill="currentColor" d="M416 48v416c0 26.51-21.49 48-48 48H144c-26.51 0-48-21.49-48-48V48c0-26.51 21.49-48 48-48h224c26.51 0 48 21.49 48 48zm96 58v12a6 6 0 0 1-6 6h-18v6a6 6 0 0 1-6 6h-42V88h42a6 6 0 0 1 6 6v6h18a6 6 0 0 1 6 6zm0 96v12a6 6 0 0 1-6 6h-18v6a6 6 0 0 1-6 6h-42v-48h42a6 6 0 0 1 6 6v6h18a6 6 0 0 1 6 6zm0 96v12a6 6 0 0 1-6 6h-18v6a6 6 0 0 1-6 6h-42v-48h42a6 6 0 0 1 6 6v6h18a6 6 0 0 1 6 6zm0 96v12a6 6 0 0 1-6 6h-18v6a6 6 0 0 1-6 6h-42v-48h42a6 6 0 0 1 6 6v6h18a6 6 0 0 1 6 6zM30 376h42v48H30a6 6 0 0 1-6-6v-6H6a6 6 0 0 1-6-6v-12a6 6 0 0 1 6-6h18v-6a6 6 0 0 1 6-6zm0-96h42v48H30a6 6 0 0 1-6-6v-6H6a6 6 0 0 1-6-6v-12a6 6 0 0 1 6-6h18v-6a6 6 0 0 1 6-6zm0-96h42v48H30a6 6 0 0 1-6-6v-6H6a6 6 0 0 1-6-6v-12a6 6 0 0 1 6-6h18v-6a6 6 0 0 1 6-6zm0-96h42v48H30a6 6 0 0 1-6-6v-6H6a6 6 0 0 1-6-6v-12a6 6 0 0 1 6-6h18v-6a6 6 0 0 1 6-6z" class=""></path></svg>
        <span class="font-semibold text-md text-gray-800 tracking-tight">%cpu_frequency%</span>
      </div>
      <div class="w-full flex justify-center items-center text-black mr-6 mt-4">
        <svg class="fill-current text-gray-800  h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 640 512" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path fill="currentColor" d="M640 130.94V96c0-17.67-14.33-32-32-32H32C14.33 64 0 78.33 0 96v34.94c18.6 6.61 32 24.19 32 45.06s-13.4 38.45-32 45.06V320h640v-98.94c-18.6-6.61-32-24.19-32-45.06s13.4-38.45 32-45.06zM224 256h-64V128h64v128zm128 0h-64V128h64v128zm128 0h-64V128h64v128zM0 448h64v-26.67c0-8.84 7.16-16 16-16s16 7.16 16 16V448h128v-26.67c0-8.84 7.16-16 16-16s16 7.16 16 16V448h128v-26.67c0-8.84 7.16-16 16-16s16 7.16 16 16V448h128v-26.67c0-8.84 7.16-16 16-16s16 7.16 16 16V448h64v-96H0v96z" class=""></path></svg>
        <span class="font-semibold text-md text-gray-800 tracking-tight">%system_heap%</span>
      </div>
      <div class="w-full flex justify-center items-center text-black mr-6 mt-4">
        <svg class="fill-current text-gray-800  h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 640 512" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path fill="currentColor" d="M634.91 154.88C457.74-8.99 182.19-8.93 5.09 154.88c-6.66 6.16-6.79 16.59-.35 22.98l34.24 33.97c6.14 6.1 16.02 6.23 22.4.38 145.92-133.68 371.3-133.71 517.25 0 6.38 5.85 16.26 5.71 22.4-.38l34.24-33.97c6.43-6.39 6.3-16.82-.36-22.98zM320 352c-35.35 0-64 28.65-64 64s28.65 64 64 64 64-28.65 64-64-28.65-64-64-64zm202.67-83.59c-115.26-101.93-290.21-101.82-405.34 0-6.9 6.1-7.12 16.69-.57 23.15l34.44 33.99c6 5.92 15.66 6.32 22.05.8 83.95-72.57 209.74-72.41 293.49 0 6.39 5.52 16.05 5.13 22.05-.8l34.44-33.99c6.56-6.46 6.33-17.06-.56-23.15z" class=""></path></svg>
        <span class="font-semibold text-md text-gray-800 tracking-tight">%wifi_ssid%</span>
      </div>
    </div>
    <a class="block text-center text-xs underline mt-6" href="/admin">Go to Admin</a>
  </div>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Admin | Warning Light</title>
  <link rel='icon' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAAAmJLR0QAAKqNIzIAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAHdElNRQfkBBoWGzVsCzJ0AAABCUlEQVQoz1XRPS9DcRQG8J9b0U4WL4uEUQQju6HxQdrZbJT4ABT1sgiJfgSLUFZCDNpBw9IYhKRNsDS01+De6/ac5X+e8+R5zvkf4phXUvPlS82mOX2RtasrTOWPHUP/7apQ6E3ed4p0EVP2IqCClz6d7T/vWLyIex9uU0azgYIgsrrEmyN5nQjJKATyUdH0bNS7G22nyfj5wGT0rJo25921CScJYWrQugFwYUnLoyfHilYxLCPMyOpoaxuxouFUoKKlqe1Vwx1bqbXWsNy36EbgQC9xHMdi6od7DgN1+wkwhoUUoewBcs4jwSu8JvJnsjEzZ0dP6FM5anaV5PovOmtLPTp3yUwM/wLOmnuR0Rs7yQAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAyMC0wNC0yNlQyMjoyNzo1MyswMDowMCDTVZQAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMjAtMDQtMjZUMjI6Mjc6NTMrMDA6MDBRju0oAAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAABJRU5ErkJggg==' type='image/x-png' />
  <link href="https://unpkg.com/tailwindcss@^1.0/dist/tailwind.min.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage(type) {
      if (type === "resetFactory" && confirm("Factory Reset\n\nWipe all data ?\nTHIS CAN NOT BE UNDONE!")) {
        
      } else {
        setTimeout(function(){ document.location.reload(false); }, 500);
      }
    }
  </script></head><body>
  <nav class="flex items-center justify-between flex-wrap bg-black p-6">
    <div class="flex items-center flex-shrink-0 text-white mr-6">
      <svg class="fill-current h-8 w-8 mr-2" width="54" height="54" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M0 0h24v24H0z" fill="none"/><path d="M10.82 12.49c.02-.16.04-.32.04-.49 0-.17-.02-.33-.04-.49l1.08-.82c.1-.07.12-.21.06-.32l-1.03-1.73c-.06-.11-.2-.15-.31-.11l-1.28.5c-.27-.2-.56-.36-.87-.49l-.2-1.33c0-.12-.11-.21-.24-.21H5.98c-.13 0-.24.09-.26.21l-.2 1.32c-.31.12-.6.3-.87.49l-1.28-.5c-.12-.05-.25 0-.31.11l-1.03 1.73c-.06.12-.03.25.07.33l1.08.82c-.02.16-.03.33-.03.49 0 .17.02.33.04.49l-1.09.83c-.1.07-.12.21-.06.32l1.03 1.73c.06.11.2.15.31.11l1.28-.5c.27.2.56.36.87.49l.2 1.32c.01.12.12.21.25.21h2.06c.13 0 .24-.09.25-.21l.2-1.32c.31-.12.6-.3.87-.49l1.28.5c.12.05.25 0 .31-.11l1.03-1.73c.06-.11.04-.24-.06-.32l-1.1-.83zM7 13.75c-.99 0-1.8-.78-1.8-1.75s.81-1.75 1.8-1.75 1.8.78 1.8 1.75S8 13.75 7 13.75zM18 1.01L8 1c-1.1 0-2 .9-2 2v3h2V5h10v14H8v-1H6v3c0 1.1.9 2 2 2h10c1.1 0 2-.9 2-2V3c0-1.1-.9-1.99-2-1.99z"/></svg>
      <span class="font-semibold text-xl tracking-tight">Admin</span>
    </div>
  </nav>
  <div class="w-full px-4">
    <section class="mt-4">
      <h2 class="text-lg font-bold mb-4">Probe</h2>
      <form class="flex flex-col" action="/configure" target="hidden-form">
        <label class="flex-grow text-gray-700 text-sm font-bold" for="endpoint_url">Endpoint URL</label>
        <div class="flex flex-row flex-grow-0">
          <input class="flex-grow shadow appearance-none border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" type="text " name="endpoint_url" placeholder="%endpoint_url%" value="%endpoint_url%">
          <input class="flex-grow-0 bg-green-700 hover:bg-green-700 text-white font-bold py-2 px-4 rounded focus:outline-none focus:shadow-outline ml-1" type="submit" value="Save" onclick="submitMessage()">
        </div>    
      </form>
      <form class="flex flex-col mt-4" action="/configure" target="hidden-form">
        <label class="flex-grow text-gray-700 text-sm font-bold" for="check_interval">Check interval</label>
        <div class="flex flex-row flex-grow-0">
          <input class="flex-grow shadow appearance-none border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" type="text " name="check_interval" placeholder="%check_interval%" value="%check_interval%">
          <input class="flex-grow-0 bg-green-700 hover:bg-green-700 text-white font-bold py-2 px-4 rounded focus:outline-none focus:shadow-outline ml-1" type="submit" value="Save" onclick="submitMessage()">
        </div>    
      </form>
      <form class="flex flex-col mt-4" action="/configure" target="hidden-form">
        <label class="flex-grow text-gray-700 text-sm font-bold" for="check_retry">Check retry</label>
        <div class="flex flex-row flex-grow-0">
          <input class="flex-grow shadow appearance-none border rounded py-2 px-3 text-gray-700 leading-tight focus:outline-none focus:shadow-outline" type="text " name="check_retry" placeholder="%check_retry%" value="%check_retry%">
          <input class="flex-grow-0 bg-green-700 hover:bg-green-700 text-white font-bold py-2 px-4 rounded focus:outline-none focus:shadow-outline ml-1" type="submit" value="Save" onclick="submitMessage()">
        </div>
        <p class="text-sm text-gray-600 pl-1">0 to disable retry</p>
      </form>
    </section>
    <section class="mt-4">
      <h2 class="text-lg font-bold mb-4">WiFi</h2>
      <form class="flex items-center" action="/wifi/disconnect" method="post" target="hidden-form">
        <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Forget network" onclick="submitMessage()">
      </form>
    </section>
    <section class="mt-4">
      <h2 class="text-lg font-bold mb-4">Board</h2>
      <form class="flex items-center" action="/restart" method="post" target="hidden-form">
        <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Restart" onclick="submitMessage()">
      </form>
      <form class="flex items-center mt-4" action="/reset" method="post" target="hidden-form">
        <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Reset" onclick="submitMessage()">
      </form>
      <form class="flex items-center mt-4" action="/reset/factory" method="post" target="hidden-form">
        <input class="bg-red-700 hover:bg-red-700 text-white font-bold py-2 rounded focus:outline-none focus:shadow-outline flex-grow" type="submit" value="Factory reset" onclick="submitMessage('resetFactory')">
      </form>
    </section>
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

void notFound(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(404, "text/plain", "");
}

void handleRoot(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send_P(200, "text/html", index_html, processor);
}

void handleAdmin(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send_P(200, "text/html", admin_html, processor);
}

void handleConfigure(AsyncWebServerRequest *request) {
  logRoute(request);
  String inputMessage;
  if (request->hasParam(PARAM_ENDPOINT_URL)) {
    inputMessage = request->getParam(PARAM_ENDPOINT_URL)->value();
    writeFile(SPIFFS, "/endpoint_url.txt", inputMessage.c_str());
  } else if (request->hasParam(PARAM_CHECK_INTERVAL)) {
    inputMessage = request->getParam(PARAM_CHECK_INTERVAL)->value();
    writeFile(SPIFFS, "/check_interval.txt", inputMessage.c_str());
  } else if (request->hasParam(PARAM_CHECK_RETRY)) {
    inputMessage = request->getParam(PARAM_CHECK_RETRY)->value();
    writeFile(SPIFFS, "/check_retry.txt", inputMessage.c_str());
  } else {
    inputMessage = "[HTTP] /configure: No message sent";
  }
  request->send(200, "text/text", inputMessage);
}

void handleReset(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", redirect_home);
  delay(2000);
  reset();
}

void handleResetFactory(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", redirect_home);
  resetFactory();
}

void handleRestart(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", redirect_home);
  delay(2000);
  restart();
}

void handleWiFiDisconnect(AsyncWebServerRequest *request) {
  logRoute(request);
  request->send(200, "text/html", "redirect_home");
  disconnectWiFi();
}

String readFile(fs::FS &fs, const char * path){
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.printf("[FILE] OPEN %s... failure\r\n", path);
    return String();
  }
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  file.close();
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

// replace placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "endpoint_url"){
    return readFile(SPIFFS, "/endpoint_url.txt");
  } else if(var == "check_interval") {
    return readFile(SPIFFS, "/check_interval.txt");
  } else if(var == "check_retry") {
    return readFile(SPIFFS, "/check_retry.txt");
  } else if(var == "check_status") {
    return readFile(SPIFFS, "/check_status.txt");
  } else if(var == "cpu_frequency") {
    String cpu_frequency = String(ESP.getCpuFreqMHz()) + " MHz";
    return cpu_frequency;
  } else if(var == "system_heap") {
    String system_heap = String(ESP.getFreeHeap()) + " bytes";
    return system_heap;
  } else if(var == "wifi_ssid") {
    return WiFi.SSID();;
  }
  return String();
}

void reset() {
  ESP.reset();
  delay(1000);
}

void resetFactory() {
    Serial.println("[SYSTEM] Factory reset...");

    Serial.print("[SYSTEM] Clear WiFi credentials...");
    AsyncWiFiManager wifiManager(&server,&dns);
    wifiManager.resetSettings();
    Serial.print(" success");

    Serial.print("[SYSTEM] Format filesystem...");      
    if(SPIFFS.format()) {
      Serial.print(" success");
    } else {
      Serial.print(" failure");
    }

    reset();
}

void restart() { 
  ESP.restart();
  delay(1000);
}

void disconnectWiFi() {
  Serial.print("[WIFI] Disconnect...");
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.resetSettings();
  if(WiFi.status() == WL_DISCONNECTED ) {
    Serial.println(" success");
    reset();
    return;
  }    
  Serial.println(" failure");
}

void checkEndpoint(int retry) {
  const unsigned long check_retry = atol(readFile(SPIFFS, "/check_retry.txt").c_str());
  if (check_retry == 0) {
    Serial.println("[HTTP] Retry disabled.");
  } else if (retry < check_retry) {
    Serial.printf("[HTTP] Retry... %d\n", retry);
    delay(200);
  } else if (retry < 0) { // all retries have been performed
    return;
  }

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
        
        bool pass = (payload == "ok" || payload == "up" || payload == "pass");
        if (pass && digitalRead(pin_relay) == HIGH) { // switch off the warning light, if on
          digitalWrite(pin_relay, LOW);
        } else if (!pass && digitalRead(pin_relay) == LOW) {  // switch on the warning light, if off
          digitalWrite(pin_relay, HIGH);
        }
        // save last check to memory
        writeFile(SPIFFS, "/check_status.txt", (pass ? "pass" : "fail"));
      }
    } else { // error with unsupported HTTP code
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      const int n_retry = retry - 1;
      checkEndpoint(n_retry);
      if (retry == 0 && digitalRead(pin_relay) == LOW) {
        digitalWrite(pin_relay, HIGH);
      }
      writeFile(SPIFFS, "/check_status.txt", "fail");
    }

    http.end();
  } else { // unable to contact server
    Serial.printf("[HTTP] Unable to connect\n");
    const int n_retry = retry - 1;
    checkEndpoint(n_retry);
    if (retry == 0 && digitalRead(pin_relay) == LOW) {
      digitalWrite(pin_relay, HIGH);
    }
    writeFile(SPIFFS, "/check_status.txt", "fail");
  }
}

void ota(){
  // Hostname defaults to esp8266-[ChipID]
  // char cstr[16];
  // const char* chip_id = itoa(ESP.getChipId(), cstr, 10);
  // ArduinoOTA.setHostname(chip_id);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"845210");

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
}

void setup() {
  // check double reset
  if (drd.detect()) {
    Serial.println("[SYSTEM] Double reset detected");
    resetFactory();
  }

  // initialize pin
  pinMode(pin_relay, OUTPUT);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  
  Serial.println();
  Serial.println();
  Serial.println();

  // initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("[SPIFFS] Error on mount");
    return;
  }
  // store default values
  if(readFile(SPIFFS, "/endpoint_url.txt") == "") {
    writeFile(SPIFFS, "/endpoint_url.txt", endpoint_url_default);
  }
  if(readFile(SPIFFS, "/check_interval.txt") == "") {
    char cstr[16];
    const char* check_interval = itoa(check_interval_default, cstr, 10);
    writeFile(SPIFFS, "/check_interval.txt", check_interval);
  }
  if(readFile(SPIFFS, "/check_retry.txt") == "") {
    char cstr[16];
    const char* check_retry = itoa(check_retry_default, cstr, 10);
    writeFile(SPIFFS, "/check_retry.txt", check_retry);
  }

  for (uint8_t t = setup_wait; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
        
    // indicate visual cue for setup
    digitalWrite(pin_relay, (t % 2) ? LOW : HIGH);
    delay(1000);
  }
  // keep the warning light on
  digitalWrite(pin_relay, HIGH);

  // connect to WiFi
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect();
  
  // wait for WiFi connection
  Serial.print("[WIFI] CONNECT");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  
  // show IP address
  Serial.print("[WIFI] IP address: ");
  Serial.println(WiFi.localIP());

  // initialize OTA
  ota();

  // Set routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/configure", HTTP_GET, handleConfigure);
  server.on("/reset/factory", HTTP_POST, handleResetFactory);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/wifi/disconnect", HTTP_POST, handleWiFiDisconnect);
  server.onNotFound(notFound);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  
  // check WiFi connection (connect if required)
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    // check timer
    unsigned long current_time = millis();
    const unsigned long check_interval = atol(readFile(SPIFFS, "/check_interval.txt").c_str());
    if (current_time - check_interval_time < check_interval) {
      return;
    }
    check_interval_time = current_time;

    const unsigned long check_retry = atol(readFile(SPIFFS, "/check_retry.txt").c_str());
    checkEndpoint(check_retry);
  }
}
