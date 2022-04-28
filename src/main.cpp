#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>  //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include "FS.h"
#include "SPIFFS.h"

#ifdef ESP32_HOSTNAME
const char* hostname = ESP32_HOSTNAME;  // For multicast DNS and OTA
#else
const char* hostname = "esp32";  // For multicast DNS and OTA
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1"
#endif

// WLAN and local webserver
WiFiClient wifiClient;
WebServer server(80);

/******************** Local webserver *************************/
String getContentType(String filename) {
    if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".png"))
        return "image/png";
    else if (filename.endsWith(".gif"))
        return "image/gif";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".svg"))
        return "image/svg+xml";
    return "text/plain";
}

bool serveFile(String path) {
    if (path.endsWith("/")) path += "index.html";

    if (SPIFFS.exists(path)) {
        String contentType = getContentType(path);
        File f = SPIFFS.open(path, "r");
        // server.sendHeader("Cache-Control", "max-age=86400");
        server.streamFile(f, contentType);
        f.close();
        return true;
    }
    return false;
}

void handleStaticFile() {
    String path = server.uri();

    if (!serveFile(path) &&
        !serveFile(path + "/index.html")) {
        server.send(404, "text/html",
                    "<h1>File not found</h1>\n<a href=\"/index.html\">Main page</a>");
    }
}

/*** Webserver route handlers ***/

void web_hello() {
    server.send(200, "application/json", "{\"message\":\"Hello world!\"}");
}

void web_fw_version() {
    server.send(200, "application/json", "{\"version\":\"" FIRMWARE_VERSION "\"}");
}

/*************** Setup **************/
void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("Starting. Firmware version:" FIRMWARE_VERSION);

    WiFiManager wifiManager;
    wifiManager.autoConnect("Lock");
    Serial.print("Wifi connected: ");
    Serial.println(WiFi.localIP());

    // esp32.local multicast DNS setup
    MDNS.begin(hostname);

    // OTA firmware update
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
    });
    ArduinoOTA.onEnd([]() {
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    });
    ArduinoOTA.onError([](ota_error_t error) {
    });
    ArduinoOTA.begin();

    /* Webserver routes */
    SPIFFS.begin();
    server.on("/hello", HTTP_GET, web_hello);
    server.on("/version", HTTP_GET, web_fw_version);
    server.onNotFound(handleStaticFile);
    server.begin();
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
}
