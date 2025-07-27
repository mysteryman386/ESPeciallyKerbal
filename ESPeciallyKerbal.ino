/*
    ESPeciallyKerbal
    ESPeciallyKerbal.ino

    An ESP32 companion program for KerbalSprigProgram that hosts a web UI to control Kerbal Space Program

    This Version Modified 27/06/2025
    By Wilmer Zhang

*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ESPmDNS.h>

//WiFi settings, change as you see fit.
const char *ssid = "SPARK-6LHYX5";
const char *password = "QuickTigerUU38$";
//Authkey, change it to the Sprigs specified code, otherwise you can't authorize risky actions
const String authkey = "E6632C85937C2730";

// Sprig backend host (mDNS name)
String rp2040Host = "http://KerbalSprigProgram.local";


AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  // Connect to Wi-Fi
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
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // Initialize SD card
  if (!SD.begin()) {
    Serial.println("SD Card Mount Failed!");
    return;
  }
  Serial.println("SD Card initialized.");

  // Serve files from SD card
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/index.html", "text/html");
  });
  server.serveStatic("/style.css", SD, "/style.css");
  server.serveStatic("/script.js", SD, "/script.js");

  // Proxy endpoints to RP2040 backend. This has to be done due to CORS, which is a PITA.
  server.on("/api/abort", HTTP_GET, [](AsyncWebServerRequest *request) {
    forwardToRP2040("/abort", true);
    request->send(200, "text/plain", "Abort command sent");
  });

  server.on("/api/sas", HTTP_GET, [](AsyncWebServerRequest *request) {
    forwardToRP2040("/sas", false);
    request->send(200, "text/plain", "SAS toggled");
  });

  server.on("/api/lights", HTTP_GET, [](AsyncWebServerRequest *request) {
    forwardToRP2040("/lights", false);
    request->send(200, "text/plain", "Lights toggled");
  });

  server.on("/api/verticalAltitude", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", forwardToRP2040("/verticalAltitude", false));
  });

    server.on("/api/surfaceVelocity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", forwardToRP2040("/surfaceVelocity", false));
  });

  server.begin();
}

void loop() {
}

// Function to forward a GET request to the RP2040. 1st parameter determines where to redirect the user to, 2nd parameter determines if the user wants to append an API token to authorize risky actions.
String forwardToRP2040(const String &path, bool needAuth) {
  HTTPClient http;
  String url;
  String payload;
  if (needAuth == true) {
    url = rp2040Host + path + "?auth=" + authkey;
  } else {
    url = rp2040Host + path;
  }
  http.begin(url);
  int code = http.GET();
  if (code > 0) {
    Serial.printf("Forwarded %s: code %d\n", path.c_str(), code);
    if (code == HTTP_CODE_OK) {
      payload = http.getString();  // Get the response body
    }
  } else {
    Serial.printf("Failed to forward %s\n", path.c_str());
  }
  http.end();
  return payload;
}
