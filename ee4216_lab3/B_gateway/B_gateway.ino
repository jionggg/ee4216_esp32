// EE4216 Lab 3 - Gateway (ESP32-S3 B), Tasks 1-4
//   Task 1: WIFI_AP_STA. AP = local sensor network (SSID = matric number, 192.168.4.1),
//           STA = Internet router. Prints gateway AP IP, gateway STA IP and sensor-node IP (Table 2).
//   Task 2: receives POST /api/sensor from sensor node A, validates and stores the latest reading
//   Task 3: uploads the latest valid reading to ThingSpeak (HTTPS POST /update) every ~20 s
//   Task 4: polls the LED Control channel (HTTPS GET feeds/last.json) every ~5 s, uses entry_id
//           as the command ID, maps colour code -> RGB and forwards POST /api/led to the
//           sensor node. A command is marked done only after the sensor node acks with 200.

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <esp_system.h>

// Upstream router (STA) - must be 2.4 GHz (iPhone: Maximize Compatibility ON)
const char* ssid_STA     = "jionggg";
const char* password_STA = "56781234";

// Local sensor network (AP) - SSID = matric number; must match A_sensor
const char* ssid_AP     = "A0282723A";
const char* password_AP = "password123";

// ThingSpeak "EE4216 Sensor Data" channel -> API Keys tab -> Write API Key
const char* TS_WRITE_KEY = "LSMOAK77R1E3AHO6";

// ThingSpeak "EE4216 LED Control" channel -> Channel ID + API Keys tab -> Read API Key
const char* TS_LED_CHANNEL_ID = "3556736";  
const char* TS_LED_READ_KEY   = "6L8DROGK3B95K4MC";
// ================================================================================

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

const int port = 80;
WebServer server(port);

// ---- ThingSpeak (EE4216 Sensor Data channel) ----
const char* TS_URL       = "https://api.thingspeak.com/update";
const unsigned long UPLOAD_INTERVAL_MS = 20000;   // free channel: >= 15 s between updates
unsigned long lastUpload = 0;

// Root CA of api.thingspeak.com (DigiCert Global Root G2, valid to 2038)
const char* TS_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// ---- Latest sensor reading (overwritten by every valid POST) ----
String latestNode;
float latestDistance = NAN;
unsigned long latestRxMillis = 0;
char latestRxTime[32] = "";
bool newReading = false;          // true = received but not yet uploaded

// ---- Task 1: sensor-node IP (DHCP lease, then confirmed by each /api/sensor POST) ----
IPAddress sensorNodeIp;

// ---- Task 4: LED command state ----
const unsigned long POLL_INTERVAL_MS    = 5000;   // poll ThingSpeak every ~5 s
const unsigned long FORWARD_RETRY_MS    = 3000;   // retry forwarding to the sensor node
const uint16_t      LOCAL_HTTP_TIMEOUT  = 3000;
unsigned long lastPoll = 0;
unsigned long lastForward = 0;

long lastDoneEntryId = 0;      // last entry_id acknowledged by the sensor node
struct LedCommand {
    bool pending = false;
    long entryId = 0;
    int  index = 0;            // 0-7
    int  color = 0;            // 0=OFF 1=RED 2=GREEN 3=BLUE
};
LedCommand cmd;

const char* COLOR_NAMES[4] = {"OFF", "RED", "GREEN", "BLUE"};
const uint8_t COLOR_RGB[4][3] = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}};

// ---- Upstream reconnect (non-blocking) ----
const unsigned long STA_RETRY_MS = 15000;
unsigned long lastStaAttempt = 0;
int staAttempt = 0;

// ---------------- Router connection status ----------------
// Prints a line whenever the STA link to the router comes up or goes down
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.printf("[STA] Associated with \"%s\", waiting for IP from DHCP...\n", ssid_STA);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            staAttempt = 0;
            Serial.printf("[STA] CONNECTED to router \"%s\" | IP %s | RSSI %d dBm | channel %d\n",
                          ssid_STA, WiFi.localIP().toString().c_str(),
                          WiFi.RSSI(), WiFi.channel());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("[STA] DISCONNECTED from router \"%s\" (reason %d)\n",
                          ssid_STA, info.wifi_sta_disconnected.reason);
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            sensorNodeIp = IPAddress(info.wifi_ap_staipassigned.ip.addr);
            Serial.printf("[AP ] Sensor node joined, assigned IP %s\n",
                          sensorNodeIp.toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("[AP ] Sensor node left");
            break;
        default:
            break;
    }
}

// Human-readable router status for the periodic summary
const char* staStatusText() {
    switch (WiFi.status()) {
        case WL_CONNECTED:       return "CONNECTED";
        case WL_NO_SSID_AVAIL:   return "router SSID not found (check name / 2.4 GHz)";
        case WL_CONNECT_FAILED:  return "connect failed (check password)";
        case WL_CONNECTION_LOST: return "connection lost";
        case WL_DISCONNECTED:    return "disconnected";
        case WL_IDLE_STATUS:     return "idle / connecting";
        default:                 return "unknown";
    }
}

// Scan the air and report whether the router is visible (blocks ~2-3 s)
void scanForRouter() {
    Serial.printf("[SCAN] Looking for router \"%s\"...\n", ssid_STA);
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        Serial.println("[SCAN] No networks found at all");
        WiFi.scanDelete();
        return;
    }
    bool found = false;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == ssid_STA) {
            found = true;
            Serial.printf("[SCAN] FOUND \"%s\" | RSSI %d dBm | channel %d | %s\n",
                          ssid_STA, WiFi.RSSI(i), WiFi.channel(i),
                          WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
        }
    }
    if (!found) {
        Serial.printf("[SCAN] \"%s\" NOT visible. Networks seen (%d):\n", ssid_STA, n);
        for (int i = 0; i < n && i < 10; i++)
            Serial.printf("         %-24s %4d dBm  ch %d\n",
                          WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        Serial.println("[SCAN] Check: name spelling/case, router is 2.4 GHz, hotspot is on");
    }
    WiFi.scanDelete();
}

void startStaAttempt() {
    staAttempt++;
    Serial.printf("[STA] Attempt #%d: connecting to router \"%s\"...\n", staAttempt, ssid_STA);
    WiFi.disconnect();
    WiFi.begin(ssid_STA, password_STA);
    lastStaAttempt = millis();
}

const unsigned long STATUS_MS = 30000;   // print network summary every 30 s
unsigned long lastStatus = 0;

// ---------------- Task 1: Table 2 summary ----------------
void printNetworkSummary() {
    Serial.println("------------ Table 2 ------------");
    Serial.printf("gateway AP IP   : %s  (SSID %s, %d station(s))\n",
                  WiFi.softAPIP().toString().c_str(), ssid_AP,
                  WiFi.softAPgetStationNum());
    if (WiFi.status() == WL_CONNECTED)
        Serial.printf("gateway STA IP  : %s  (router %s, RSSI %d dBm)\n",
                      WiFi.localIP().toString().c_str(), ssid_STA, WiFi.RSSI());
    else
        Serial.printf("gateway STA IP  : (not connected - %s)\n", staStatusText());
    if (sensorNodeIp != IPAddress((uint32_t)0))
        Serial.printf("sensor-node IP  : %s\n", sensorNodeIp.toString().c_str());
    else
        Serial.println("sensor-node IP  : (no station yet)");
    Serial.printf("LED command     : last done entry %ld%s\n", lastDoneEntryId,
                  cmd.pending ? " (one pending)" : "");
    if (!isnan(latestDistance))
        Serial.printf("latest reading  : %s %.1f cm at %s\n",
                      latestNode.c_str(), latestDistance, latestRxTime);
    Serial.println("---------------------------------");
}

// ---------------- Task 2: POST /api/sensor ----------------
void handleSensor() {
    Serial.println("Sensor POST request received");

    time_t timestamp = time(nullptr);
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    char timeString[32];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("Received at: %s\n", timeString);

    String payload = server.arg("plain");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc["node"].is<const char*>() || !doc["distance"].is<float>()) {
        server.send(400, "application/json", "{\"error\":\"Missing or invalid fields\"}");
        return;
    }

    String node = doc["node"].as<String>();
    float distance = doc["distance"].as<float>();

    if (isnan(distance) || distance <= 0 || distance > 400) {
        server.send(400, "application/json", "{\"error\":\"distance out of range (0-400 cm)\"}");
        return;
    }

    // Store latest reading
    latestNode = node;
    latestDistance = distance;
    latestRxMillis = millis();
    strncpy(latestRxTime, timeString, sizeof(latestRxTime));
    newReading = true;
    sensorNodeIp = server.client().remoteIP();   // actual local IP of the sensor node

    Serial.printf("Node: %s (%s)\n", node.c_str(), sensorNodeIp.toString().c_str());
    Serial.printf("Distance: %.2f cm\n", distance);

    server.send(200, "application/json", "{\"status\":\"success\"}");
}

// ---------------- Task 3: upload to ThingSpeak ----------------
void uploadToThingSpeak() {
    WiFiClientSecure client;
    client.setCACert(TS_ROOT_CA);          // proper certificate validation (no setInsecure)

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(client, TS_URL)) {
        Serial.println("[TS] http.begin failed");
        return;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "api_key=" + String(TS_WRITE_KEY) +
                  "&field1=" + String(latestDistance, 1) +
                  "&field2=" + String(WiFi.RSSI());        // optional Wi-Fi RSSI

    int code = http.POST(body);
    String resp = (code > 0) ? http.getString() : "";
    http.end();

    // ThingSpeak returns the new entry ID; "0" means the update was NOT stored
    long entryId = resp.toInt();
    if (code == 200 && entryId > 0) {
        Serial.printf("[TS] Stored entry %ld: field1=%.1f cm (from %s, received %s)\n",
                      entryId, latestDistance, latestNode.c_str(), latestRxTime);
        newReading = false;
    } else if (code > 0) {
        Serial.printf("[TS] Not stored: HTTP %d, body \"%s\"\n", code, resp.c_str());
    } else {
        Serial.printf("[TS] HTTPS request failed: %s\n", http.errorToString(code).c_str());
    }
}

// ---------------- Task 4: poll ThingSpeak for LED commands ----------------
// ThingSpeak fields arrive as strings ("3"); accept only a whole integer in [lo, hi]
bool parseField(JsonVariant v, long lo, long hi, int& out) {
    const char* str = v.as<const char*>();
    if (str == nullptr || *str == '\0') return false;
    char* end;
    long x = strtol(str, &end, 10);
    if (*end != '\0' || x < lo || x > hi) return false;
    out = (int)x;
    return true;
}

void pollLedCommand() {
    String url = String("https://api.thingspeak.com/channels/") + TS_LED_CHANNEL_ID +
                 "/feeds/last.json?api_key=" + TS_LED_READ_KEY;

    WiFiClientSecure client;
    client.setCACert(TS_ROOT_CA);
    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(client, url)) {
        Serial.println("[CMD] http.begin failed");
        return;
    }
    int code = http.GET();
    String resp = (code > 0) ? http.getString() : "";
    http.end();

    if (code <= 0) {
        Serial.printf("[CMD] HTTPS GET failed: %s\n", http.errorToString(code).c_str());
        return;
    }
    if (code != 200) {
        Serial.printf("[CMD] HTTP %d: %s\n", code, resp.c_str());
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp) || !doc.is<JsonObject>()) {
        // Empty channel returns "-1"; a wrong key/ID returns an error body
        Serial.printf("[CMD] No valid entry (body \"%s\")\n", resp.c_str());
        return;
    }

    long entryId = doc["entry_id"] | 0L;
    if (entryId <= 0) {
        Serial.println("[CMD] Response has no entry_id");
        return;
    }
    // Duplicate check: already executed, or already waiting to be forwarded
    if (entryId == lastDoneEntryId || (cmd.pending && entryId == cmd.entryId)) return;

    int index, color;
    if (!parseField(doc["field1"], 0, 7, index) || !parseField(doc["field2"], 0, 3, color)) {
        Serial.printf("[CMD] Entry %ld invalid (field1=%s, field2=%s) - ignored\n", entryId,
                      doc["field1"].as<String>().c_str(), doc["field2"].as<String>().c_str());
        lastDoneEntryId = entryId;   // never retry an invalid command
        return;
    }

    cmd.pending = true;
    cmd.entryId = entryId;
    cmd.index = index;
    cmd.color = color;
    lastForward = 0;                 // forward immediately
    Serial.printf("[CMD] New command entry %ld: LED %d -> %s\n",
                  entryId, index, COLOR_NAMES[color]);
}

// ---------------- Task 4: forward LED command to sensor node ----------------
void forwardLedCommand() {
    if (sensorNodeIp == IPAddress((uint32_t)0)) {
        Serial.println("[FWD] Sensor node IP unknown yet, waiting");
        return;
    }

    JsonDocument doc;
    doc["index"] = cmd.index;
    doc["r"] = COLOR_RGB[cmd.color][0];
    doc["g"] = COLOR_RGB[cmd.color][1];
    doc["b"] = COLOR_RGB[cmd.color][2];
    String payload;
    serializeJson(doc, payload);

    String url = "http://" + sensorNodeIp.toString() + "/api/led";
    HTTPClient http;
    http.setTimeout(LOCAL_HTTP_TIMEOUT);
    http.setConnectTimeout(LOCAL_HTTP_TIMEOUT);
    if (!http.begin(url)) {
        Serial.println("[FWD] http.begin failed");
        return;
    }
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    String resp = (code > 0) ? http.getString() : "";
    http.end();

    if (code == 200) {
        lastDoneEntryId = cmd.entryId;   // completed only after the sensor node acks
        cmd.pending = false;
        Serial.printf("[FWD] Entry %ld done: %s -> %s\n", cmd.entryId, payload.c_str(), resp.c_str());
    } else if (code > 0) {
        Serial.printf("[FWD] Sensor rejected (HTTP %d): %s\n", code, resp.c_str());
        if (code == 400) {               // sensor says the command itself is bad: drop it
            lastDoneEntryId = cmd.entryId;
            cmd.pending = false;
        }
    } else {
        Serial.printf("[FWD] POST %s failed: %s - will retry\n", url.c_str(),
                      http.errorToString(code).c_str());
    }
}

// ---------------- Setup / loop ----------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("\n[BOOT] B_gateway starting, last reset reason = %d "
                  "(1=power-on 3=sw 4=panic 5/6/7=watchdog 9=brownout)\n",
                  (int)esp_reset_reason());

    WiFi.onEvent(onWiFiEvent);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);   // same as Lab 2: full power resets this board

    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure static AP IP");
    }
    WiFi.softAP(ssid_AP, password_AP);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);   // re-apply after AP start
    Serial.println("Access Point (AP) started");

    WiFi.setAutoReconnect(true);
    scanForRouter();
    startStaAttempt();
    Serial.print("[STA] Waiting");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
        Serial.printf("Router: NOT connected to \"%s\" - %s. Will keep retrying.\n",
                      ssid_STA, staStatusText());
    printNetworkSummary();

    configTime(8 * 3600, 0, "pool.ntp.org");

    if (strncmp(TS_LED_CHANNEL_ID, "YOUR_", 5) == 0 || strncmp(TS_LED_READ_KEY, "YOUR_", 5) == 0)
        Serial.println("[CMD] WARNING: set TS_LED_CHANNEL_ID and TS_LED_READ_KEY at the top of the sketch");

    server.on("/api/sensor", HTTP_POST, handleSensor);
    server.onNotFound([]() {
        server.send(404, "application/json", "{\"error\":\"not found\"}");
    });
    server.begin();
    Serial.println("HTTP server started on port 80");
}

void loop() {
    server.handleClient();
    unsigned long now = millis();

    // Recover upstream Wi-Fi without blocking the local server
    if (WiFi.status() != WL_CONNECTED && now - lastStaAttempt >= STA_RETRY_MS) {
        Serial.printf("[STA] Still not connected (%s)\n", staStatusText());
        if (staAttempt % 3 == 0) scanForRouter();   // re-check visibility every 3rd attempt
        startStaAttempt();
    }

    // Periodic Table 2 / status summary
    if (now - lastStatus >= STATUS_MS) {
        lastStatus = now;
        printNetworkSummary();
    }

    // Upload latest valid reading every ~20 s
    if (now - lastUpload >= UPLOAD_INTERVAL_MS) {
        lastUpload = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[TS] No upstream Wi-Fi, skipping upload");
        } else if (!newReading) {
            Serial.println("[TS] No new reading since last upload, skipping");
        } else {
            uploadToThingSpeak();
        }
        return;   // one HTTPS request per loop pass keeps handleClient() responsive
    }

    // Task 4: poll ThingSpeak for a new LED command every ~5 s (needs Internet)
    if (WiFi.status() == WL_CONNECTED && now - lastPoll >= POLL_INTERVAL_MS) {
        lastPoll = now;
        pollLedCommand();
        return;
    }

    // Task 4: forward a pending command (local only - works without Internet)
    if (cmd.pending && now - lastForward >= FORWARD_RETRY_MS) {
        lastForward = now;
        forwardLedCommand();
    }
}
