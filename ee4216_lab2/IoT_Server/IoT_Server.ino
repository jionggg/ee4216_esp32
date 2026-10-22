#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <NewPing.h>
#include "lab_page.h"

// ---- Wi-Fi (tries STA first, falls back to AP) ----
const char* STA_SSID = "TP-Link_6FCB";
const char* STA_PASS = "72972786";
const char* AP_SSID  = "A0282723A";   
const char* AP_PASS  = "12345678"; // "qwertyuiop";

// ---- Hardware (from Lab 1) ----
const int TRIG_PIN = 4, ECHO_PIN = 5, MAX_DISTANCE_CM = 400;  // NewPing timeout ~= max distance
const int LED_BAR_PIN = 6, NUM_LEDS = 8;
const float SOUND_SPEED_CM_PER_US = 0.0343;

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);
Adafruit_NeoPixel bar(NUM_LEDS, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

struct Led { uint8_t r, g, b, brightness; };
Led leds[NUM_LEDS];  // 8 independent logical states, all off at start

// ---- Helpers ----
void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", out);
}

void sendError(int code, const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(code, doc);
}

void ledToJson(JsonObject o, int id) {
  o["id"] = id;
  o["r"] = leds[id].r;
  o["g"] = leds[id].g;
  o["b"] = leds[id].b;
  o["brightness"] = leds[id].brightness;
}

void applyStrip() {  // per-pixel brightness: channel * brightness / 255
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t k = leds[i].brightness;
    bar.setPixelColor(i, bar.Color(leds[i].r * k / 255, leds[i].g * k / 255, leds[i].b * k / 255));
  }
  bar.show();
}

// field must exist, be an integer, and be 0..255
bool readByte(JsonDocument& doc, const char* key, uint8_t& out) {
  JsonVariant v = doc[key];
  if (!v.is<long>()) return false;
  long x = v.as<long>();
  if (x < 0 || x > 255) return false;
  out = (uint8_t)x;
  return true;
}

// ---- Handlers ----
void handleDistance() {
  unsigned long us = sonar.ping();  // one new measurement, finite timeout
  JsonDocument doc;
  if (us == 0) {
    doc["distance_cm"] = nullptr;
    doc["valid"] = false;
  } else {
    doc["distance_cm"] = round(us * SOUND_SPEED_CM_PER_US / 2.0 * 10) / 10.0;
    doc["valid"] = true;
  }
  sendJson(200, doc);
}

void handleGetLeds() {
  JsonDocument doc;
  JsonArray arr = doc["leds"].to<JsonArray>();
  for (int i = 0; i < NUM_LEDS; i++) ledToJson(arr.add<JsonObject>(), i);
  sendJson(200, doc);
}

void handlePutLed() {
  String idStr = server.pathArg(0);
  char* end;
  long id = strtol(idStr.c_str(), &end, 10);
  if (idStr.length() == 0 || *end != '\0' || id < 0 || id >= NUM_LEDS)
    return sendError(404, "unknown LED id");

  JsonDocument body;
  if (deserializeJson(body, server.arg("plain")) || !body.is<JsonObject>())
    return sendError(400, "malformed JSON body");

  Led next;  // validate everything before touching state
  if (!readByte(body, "r", next.r) || !readByte(body, "g", next.g) ||
      !readByte(body, "b", next.b) || !readByte(body, "brightness", next.brightness))
    return sendError(400, "r, g, b and brightness must be integers 0-255");

  leds[id] = next;
  applyStrip();  // update hardware before replying

  JsonDocument doc;
  ledToJson(doc.to<JsonObject>(), id);
  sendJson(200, doc);
}

// ---- Setup / loop ----
void setup() {
  Serial.begin(115200);
  delay(1000);
  bar.begin();
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = {0, 0, 0, 255};
  applyStrip();

  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.printf("Connecting to %s", STA_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(500); Serial.print("."); }
  Serial.println();

  IPAddress ip;
  if (WiFi.status() == WL_CONNECTED) {
    ip = WiFi.localIP();
    Serial.println("STA mode connected");
  } else {
    Serial.println("STA failed, starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 2);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    ip = WiFi.softAPIP();
  }

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", LAB_PAGE); });
  server.on("/api/distance", HTTP_GET, handleDistance);
  server.on("/api/leds", HTTP_GET, handleGetLeds);
  server.on(UriBraces("/api/leds/{}"), HTTP_PUT, handlePutLed);
  server.onNotFound([]() { sendError(404, "not found"); });
  server.begin();

  Serial.printf("Dashboard: http://%s/\n", ip.toString().c_str());
}

void loop() {
  server.handleClient();
}
