#include <WiFi.h>

const char* ssid     = "TP-Link_6FCB";
const char* password = "72972786";
const unsigned long TIMEOUT_MS = 15000; //15s

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connection result: SUCCESS");
    Serial.printf("SSID:    %s\n", ssid);
    Serial.printf("RSSI:    %d dBm\n", WiFi.RSSI());
    Serial.printf("IP:      %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("MAC:     %s\n", WiFi.macAddress().c_str());
  } else {
    Serial.printf("Connection result: FAILED (status %d) after %lu s.\n",
                  WiFi.status(), TIMEOUT_MS / 1000);
    Serial.println("Check SSID/password and that the network is 2.4 GHz.");
  }
}

void loop() {}
