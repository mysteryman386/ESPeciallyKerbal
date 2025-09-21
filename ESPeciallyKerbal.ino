/*
    EspeciallyKerbal v3
    Merged KerbalSprigProgram and EspeciallyKerbal into the ESP32
    Button and display logic removed.

    Version: 29/08/2025
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include "KerbalSimpit.h"
#include "PayloadStructs.h"

// =============================
// WiFi + Auth
// =============================
const char *ssid = "PLACEHOLDER";
const char *password = "PLACEHOLDER$";

// Unique ESP32 ID as auth key
String getAuthKey() {
  uint64_t chipid = ESP.getEfuseMac(); // 48-bit unique ID
  char buf[20];
  sprintf(buf, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(buf);
}
String authKey;

// =============================
// Networking
// =============================
AsyncWebServer server(80);

// =============================
// Kerbal Simpit
// =============================
KerbalSimpit mySimpit(Serial);
bool autoAbort = false;
bool speedConditionSatisfied = false;
float surfaceVelocity = 0;
float verticalAltitude = 0;


// =============================
// Abort thresholds
// =============================
constexpr int armAbortAbove = 2000;
constexpr int triggerAbortBelow = 1750;
constexpr int triggerAbortSpeed = 75;

// =============================
// Setup
// =============================
void setup() {
  Serial.begin(115200);


  // Generate authKey from ESP32 ID
  authKey = getAuthKey();
  Serial.print("Auth Key: ");
  Serial.println(authKey);

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Start mDNS
  if (MDNS.begin("kerbal-ui")) {
    Serial.println("mDNS responder started at http://kerbal-ui.local");
    MDNS.addService("http", "tcp", 80);  // Web server
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
  

  // Init SD card
  if (!SD.begin()) {
    Serial.println("SD Card Mount Failed!");
  } else {
    Serial.println("SD Card initialized.");
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SD, "/index.html", "text/html");
    });
    server.serveStatic("/style.css", SD, "/style.css");
    server.serveStatic("/script.js", SD, "/script.js");
  }

  // Init KerbalSimpit
  while (!mySimpit.init()) {
    delay(100);
    delay(100);
  }
  mySimpit.printToKSP("Kerbal Unified Bridge Connected", PRINT_TO_SCREEN);
  mySimpit.inboundHandler(messageHandler);
  mySimpit.registerChannel(ALTITUDE_MESSAGE);
  mySimpit.registerChannel(SCENE_CHANGE_MESSAGE);
  mySimpit.registerChannel(VELOCITY_MESSAGE);
  mySimpit.registerChannel(APSIDES_MESSAGE);

  // =============================
  // Web API
  // =============================
  server.on("/api/abort", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("auth") || request->getParam("auth")->value() != authKey) {
      request->send(403, "text/plain", "Unauthorized");
      return;
    }
    abortAction();
    request->send(200, "text/plain", "Abort command sent");
  });

  server.on("/api/sas", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(SAS_ACTION, "SAS toggled!");
    request->send(200, "text/plain", "SAS toggled");
  });

  server.on("/api/lights", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(LIGHT_ACTION, "Lights toggled!");
    request->send(200, "text/plain", "Lights toggled");
  });

  server.on("/api/surfaceVelocity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(surfaceVelocity));
  });

  server.on("/api/verticalAltitude", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(verticalAltitude));
  });

  server.on("/api/stage", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(STAGE_ACTION, "Staged!");
    request->send(200, "text/plain", "Stage fired");
  });

  server.on("/api/rcs", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(RCS_ACTION, "RCS toggled!");
    request->send(200, "text/plain", "RCS toggled");
  });

  server.on("/api/gears", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(GEAR_ACTION, "Gears toggled!");
    request->send(200, "text/plain", "Gears toggled");
  });

  server.on("/api/brakes", HTTP_GET, [](AsyncWebServerRequest *request) {
    actionBind(BRAKES_ACTION, "Brakes toggled!");
    request->send(200, "text/plain", "Brakes toggled");
  });

  server.begin();
}

// =============================
// Loop
// =============================
void loop() {
  mySimpit.update();
}

// =============================
// Actions
// =============================
void abortAction() {
  mySimpit.deactivateAction(SAS_ACTION);
  mySimpit.activateAction(ABORT_ACTION);
  autoAbort = false;
}

void actionBind(uint8_t ActionGroup, String message) {
  mySimpit.toggleAction(ActionGroup);
  mySimpit.printToKSP(message, PRINT_TO_SCREEN);
}

// =============================
// Simpit Message Handler
// =============================
void messageHandler(byte messageType, byte msg[], byte msgSize) {
  switch (messageType) {
    case APSIDES_MESSAGE:
      break;

    case SCENE_CHANGE_MESSAGE:
      if (msgSize == sizeof(byte)) {
        autoAbort = false; // reset autoabort on scene change
      }
      break;

    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage myVelocity = parseMessage<velocityMessage>(msg);
        surfaceVelocity = myVelocity.surface;
        speedConditionSatisfied = (myVelocity.surface <= -triggerAbortSpeed);
      }
      break;

    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage myAltitude = parseMessage<altitudeMessage>(msg);
        verticalAltitude = myAltitude.surface;
        if (myAltitude.surface >= armAbortAbove) {
          autoAbort = true;
        } else if (autoAbort && myAltitude.surface <= triggerAbortBelow && speedConditionSatisfied) {
          abortAction();
        }
      }
      break;
  }
}
