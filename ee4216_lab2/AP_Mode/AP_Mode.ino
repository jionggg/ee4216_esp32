#include <WiFi.h>

const char* ssid     = "A0282723A";   // 
const char* password = "12345678";  // must be at least 8 characters
const int   channel  = 1;
const int   hidden   = 0;             // 0 = visible SSID
const int   maxConn  = 2;

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(ssid, password, channel, hidden, maxConn)) {
    Serial.println("AP started");
    Serial.printf("SSID:     %s\n", ssid);
    Serial.printf("AP IP:    %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP MAC:   %s\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("Max conn: %d\n", maxConn);
  } else {
    Serial.println("Failed to start AP");
  }
}

void loop() {
  static int last = -1;
  int n = WiFi.softAPgetStationNum();
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  if (n != last) {
    Serial.printf("Connected stations: %d\n", n);
    last = n;
  }
  delay(1000);
}
