#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>  //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
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

#ifndef MY_TEST_SECRET
#define MY_TEST_SECRET "Oh no, the secret did not make it!"
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

void web_secret() {
    server.send(200, "application/json", "{\"secret\":\"" MY_TEST_SECRET "\"}");
}

void web_fw_version() {
    server.send(200, "application/json", "{\"version\":\"" FIRMWARE_VERSION "\"}");
}

void web_fw_update() {
    if (!server.hasArg("url")) {
        server.send(400, "text/plain", "No URL");
        return;
    }

    Serial.print("Starting firmware update: ");
    Serial.println(server.arg("url"));

    WiFiClientSecure client;
    client.setInsecure();  // FIXME: No checking of server certificate

    // Reading data over SSL may be slow, use an adequate timeout
    client.setTimeout(12);

    t_httpUpdate_return ret = httpUpdate.update(client, server.arg("url"));
    switch (ret) {
        case HTTP_UPDATE_FAILED: {
            String message = "HTTP_UPDATE_FAILED Error (";
            message += httpUpdate.getLastError();
            message += "): ";
            message += httpUpdate.getLastErrorString();
            Serial.println(message);
            server.send(200, "text/plain", message);
        } break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            server.send(200, "text/plain", "No updates?");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            server.send(200, "text/plain", "Firmware update OK");
            break;
    }
}

/*************** Setup **************/
void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("Starting. Firmware version:" FIRMWARE_VERSION);

    WiFiManager wifiManager;
    wifiManager.autoConnect("DemoESP32");
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
    server.on("/secret", HTTP_GET, web_secret);
    server.on("/version", HTTP_GET, web_fw_version);
    server.on("/fwupdate", HTTP_POST, web_fw_update);
    server.onNotFound(handleStaticFile);
    server.begin();
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
}
