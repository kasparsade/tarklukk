#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include "time.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <PN532_I2C.h>
#include <PN532.h>

#define MAX_LINES 200
#define RELAY_PIN 32

const char* SSID = "tttttt";
const char* password = "tttttttt";

const char* webuser = "tere";
const char* webpass = "teretere";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;

AsyncWebServer server(80);

PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

String currentID;
String storedID;
String storedName;
int matchedLine;

bool matchID() {
  File match = SPIFFS.open("/inimesed.csv", FILE_READ);
  int i = 0;
  matchedLine = 0;
  while (match.available()) {
    storedID = match.readStringUntil(',');
    storedName = match.readStringUntil('\r');
    Serial.println(currentID);
    Serial.println(storedID);
    if (currentID == storedID) {
      match.close();
      matchedLine = i;
      return true;
    }
    match.readStringUntil('\n');
    i++;
  }
  match.close();
  return false;
}

void createLog() {
  File logi = SPIFFS.open("/logi.csv", FILE_READ);
  File temp = SPIFFS.open("/temp.csv", FILE_WRITE);
  String logEntry;
  String readLog;
  struct tm gettime;
  getLocalTime(&gettime);
  uint16_t year = gettime.tm_year + 1900;
  uint8_t month = gettime.tm_mon + 1;
  uint8_t day = gettime.tm_mday;
  String dStr; String monStr; String hStr; String minStr;
  if (gettime.tm_hour < 10) {hStr = "0" + String(gettime.tm_hour);} else {hStr = String(gettime.tm_hour);}
  if (gettime.tm_min < 10) {minStr = "0" + String(gettime.tm_min);} else {minStr = String(gettime.tm_min);}
  if (day < 10) {dStr = "0" + String(day);} else {dStr = String(day);}
  if (month < 10) {monStr = "0" + String(month);} else {monStr = String(month);}
  logEntry = storedName;
  logEntry += ",";
  logEntry += hStr;
  logEntry += ":";
  logEntry += minStr;
  logEntry += ",";
  logEntry += dStr;
  logEntry += "/";
  logEntry += monStr;
  logEntry += "/";
  logEntry += String(year);
  Serial.println(logEntry);
  temp.println(logEntry);
  for (int i = 0; (i < (MAX_LINES - 1)) && logi.available(); i++) {
    readLog = logi.readStringUntil('\r');
    temp.println(readLog);
    logi.readStringUntil('\n');
  }
  logi.close();
  temp.close();
  SPIFFS.remove("/logi.csv");
  SPIFFS.rename("/temp.csv", "/logi.csv");
  currentID = "";
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  SPIFFS.begin();
  Serial.begin(115200);
  if (!SPIFFS.exists("/logi.csv")) {
    File create = SPIFFS.open("/logi.csv", FILE_WRITE);
    create.close();
  }

  if (!SPIFFS.exists("/inimesed.csv")) {
    File create = SPIFFS.open("/inimesed.csv", FILE_WRITE);
    create.close();
  }

  nfc.begin();
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();

  WiFi.begin(SSID, password);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(webuser, webpass)){
      return request->requestAuthentication();
    }
     request->send(SPIFFS, "/logi.html", "text/html");
  });

   server.on("/logi", HTTP_GET, [](AsyncWebServerRequest *request){
     if(!request->authenticate(webuser, webpass)){
       return request->requestAuthentication();
     }
     request->send(SPIFFS, "/logi.html", "text/html");
  });

   server.on("/juurdepaas", HTTP_GET, [](AsyncWebServerRequest *request){
     if(!request->authenticate(webuser, webpass)){
       return request->requestAuthentication();
     }
     request->send(SPIFFS, "/juurdepaas.html", "text/html");
  });

   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/style.css", "text/css");
  });

   server.on("/logi.csv", HTTP_GET, [](AsyncWebServerRequest *request){
     if(!request->authenticate(webuser, webpass)){
       return request->requestAuthentication();
     }
     request->send(SPIFFS, "/logi.csv", "text/csv");
  });

   server.on("/inimesed.csv", HTTP_GET, [](AsyncWebServerRequest *request){
     if(!request->authenticate(webuser, webpass)){
       return request->requestAuthentication();
     }
    request->send(SPIFFS, "/inimesed.csv", "text/csv");
  });

  server.on("/kustuta", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!request->authenticate(webuser, webpass)){
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam("kustuta", true)) {
        message = request->getParam("kustuta", true)->value();
        int deleteindex = message.toInt();
        String readInim;
        File inimesed = SPIFFS.open("/inimesed.csv", FILE_READ);
        File tempinim = SPIFFS.open("/tempinim.csv", FILE_WRITE);
        for (int i = 0; inimesed.available(); i++) {
          readInim = inimesed.readStringUntil('\r');
          if (i != deleteindex) {
            tempinim.println(readInim);
          }
          inimesed.readStringUntil('\n');
        }
        inimesed.close();
        tempinim.close();
        SPIFFS.remove("/inimesed.csv");
        SPIFFS.rename("/tempinim.csv", "/inimesed.csv");
    }
    request->redirect("/juurdepaas");
  });

  server.on("/lisa", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!request->authenticate(webuser, webpass)){
      return request->requestAuthentication();
    }
    String message;
    if (request->hasParam("lisa", true)) {
        message = request->getParam("lisa", true)->value();
        boolean read;
        uint8_t readUID[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t readLen;

        read = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &readUID[0], &readLen);


          if (read) {
            currentID = "";
            for (int i = 0; i < readLen; i++) {
              currentID += String(readUID[i], HEX);
            }
          if (!matchID()) {
              String newID = currentID;
              newID += ",";
              newID += message;
              File inimesed = SPIFFS.open("/inimesed.csv", FILE_APPEND);
              inimesed.println(newID);
              inimesed.close();
            }

          }

    }
    request->redirect("/juurdepaas");
  });

  server.on("/logivalja", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/valjalogitud", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/valjalogitud.html", "text/html");
 });

  server.begin();
  MDNS.begin("esp32");
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  boolean read;
  uint8_t readUID[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t readLen;

  read = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &readUID[0], &readLen);

  if (read) {
    currentID = "";
    for (int i = 0; i < readLen; i++) {
      currentID += String(readUID[i], HEX);
    }
    if (matchID()) {
      createLog();
      digitalWrite(RELAY_PIN, HIGH);
      delay(5000);
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  delay(1000);
}
