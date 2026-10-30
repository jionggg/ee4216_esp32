// EE4216 Lab 3 - Sensor node (ESP32-S3 A), Tasks 1-4
//
// Task 1: joins the gateway AP (SSID = matric number) as a station and
//         prints its DHCP IP (sensor-node IP for Table 2).
// Task 2: HTTP CLIENT - measures ultrasonic distance and POSTs JSON to the gateway every 5 s:
//           POST http://192.168.4.1/api/sensor   {"node":"sensor01","distance":35.6}
// Task 4: HTTP SERVER - receives LED commands from the gateway:
//           POST /api/led   {"index":3,"r":255,"g":0,"b":0}
//         Validates index (0-7) and r/g/b (0-255), updates only that LED,
//         replies 200 + ack after actuation, 400 otherwise.

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NewPing.h>
#include <Adafruit_NeoPixel.h>

// ===================== EDIT: must match B_gateway =====================
const char* AP_SSID = "A0282723A";     // gateway AP SSID (your matric number)
const char* AP_PASS = "password123";   // gateway AP password
// =====================================================================
const unsigned long CONNECT_TIMEOUT_MS = 15000;

// ---- Gateway endpoint (Task 2) ----
const char* GATEWAY_URL = "http://192.168.4.1/api/sensor";
const char* NODE_ID     = "sensor01";
const unsigned long SEND_INTERVAL_MS = 5000;   // every 5 s
const uint16_t HTTP_TIMEOUT_MS = 3000;         // never hang on a dead gateway

// ---- Hardware (Lab 1/2 wiring) ----
const int TRIG_PIN = 4, ECHO_PIN = 5, MAX_DISTANCE_CM = 400;
const int LED_BAR_PIN = 6, NUM_LEDS = 8;
const uint8_t BAR_BRIGHTNESS = 60;             // 0-255, keeps current draw low
const float SOUND_SPEED_CM_PER_US = 0.0343;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM);
Adafruit_NeoPixel bar(NUM_LEDS, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);

// ---- Local HTTP server (Task 4) ----
WebServer server(80);

// ---- Timers ----
const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long STATUS_MS     = 30000;
unsigned long lastWifiAttempt = 0;
unsigned long lastSend = 0;
unsigned long lastStatus = 0;

// ---------------- Task 1: network info ----------------
void printNetworkInfo() {
  Serial.println("------------ Table 2 (sensor side) ------------");
  Serial.printf("gateway AP IP  : %s  (SSID %s)\n",
                WiFi.gatewayIP().toString().c_str(), AP_SSID);
  Serial.printf("sensor-node IP : %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("RSSI           : %d dBm\n", WiFi.RSSI());
  Serial.printf("MAC            : %s\n", WiFi.macAddress().c_str());
  Serial.println("-----------------------------------------------");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Joined %s, DHCP IP %s\n", AP_SSID,
                    WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] Disconnected from %s (reason %d)\n", AP_SSID,
                    info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
  }
}

// ---------------- Task 2: measure + send ----------------
// Returns distance in cm (1 decimal), or -1 if no valid echo
float readDistanceCm() {
  unsigned long us = sonar.ping();
  if (us == 0) return -1;
  return round(us * SOUND_SPEED_CM_PER_US / 2.0 * 10) / 10.0;
}

void sendReading(float distance) {
  JsonDocument doc;
  doc["node"] = NODE_ID;
  doc["distance"] = distance;
  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(GATEWAY_URL)) {
    Serial.println("[HTTP] begin() failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(payload);
  if (code > 0) {
    Serial.printf("[HTTP] POST %s -> %d %s\n", payload.c_str(), code,
                  http.getString().c_str());
  } else {
    Serial.printf("[HTTP] POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

// ---------------- Task 4: POST /api/led ----------------
void sendError(int code, const char* msg) {
  JsonDocument doc;
  doc["status"] = "error";
  doc["error"] = msg;
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
  Serial.printf("[LED] Rejected (%d): %s\n", code, msg);
}

// field must exist, be an integer, and lie in [lo, hi]
bool readInt(JsonDocument& doc, const char* key, long lo, long hi, long& out) {
  JsonVariant v = doc[key];
  if (!v.is<long>()) return false;
  long x = v.as<long>();
  if (x < lo || x > hi) return false;
  out = x;
  return true;
}

void handleLed() {
  JsonDocument body;
  if (deserializeJson(body, server.arg("plain")) || !body.is<JsonObject>())
    return sendError(400, "malformed JSON body");

  long index, r, g, b;   // validate everything before touching the LEDs
  if (!readInt(body, "index", 0, NUM_LEDS - 1, index))
    return sendError(400, "index must be an integer 0-7");
  if (!readInt(body, "r", 0, 255, r) || !readInt(body, "g", 0, 255, g) ||
      !readInt(body, "b", 0, 255, b))
    return sendError(400, "r, g and b must be integers 0-255");

  // Update only the specified LED; the others keep their colour
  bar.setPixelColor(index, bar.Color(r, g, b));
  bar.show();
  Serial.printf("[LED] LED %ld set to (%ld,%ld,%ld)\n", index, r, g, b);

  // Acknowledge only after actuation
  JsonDocument ack;
  ack["status"] = "ok";
  ack["index"] = index;
  ack["r"] = r;
  ack["g"] = g;
  ack["b"] = b;
  String out;
  serializeJson(ack, out);
  server.send(200, "application/json", out);
}

// ---------------- Setup / loop ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  bar.begin();
  bar.setBrightness(BAR_BRIGHTNESS);
  bar.clear();
  bar.show();

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(AP_SSID, AP_PASS);
  Serial.printf("Connecting to gateway AP %s", AP_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    printNetworkInfo();
  } else {
    Serial.printf("Connection FAILED (status %d) after %lu s. Will keep retrying.\n",
                  WiFi.status(), CONNECT_TIMEOUT_MS / 1000);
  }
  lastWifiAttempt = millis();

  server.on("/api/led", HTTP_POST, handleLed);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"status\":\"error\",\"error\":\"not found\"}");
  });
  server.begin();
  Serial.println("HTTP server started on port 80 (POST /api/led)");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // Reconnect to the gateway AP without blocking
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
      lastWifiAttempt = now;
      Serial.println("[WiFi] Not connected, retrying...");
      WiFi.disconnect();
      WiFi.begin(AP_SSID, AP_PASS);
    }
    return;
  }

  if (now - lastStatus >= STATUS_MS) {
    lastStatus = now;
    printNetworkInfo();
  }

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    float d = readDistanceCm();
    if (d < 0) {
      Serial.println("[SONAR] No valid echo, skipping this reading");
      return;
    }
    sendReading(d);
  }
}
