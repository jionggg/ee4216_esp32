IoT dashboard starter - Lab 2

Contents: 
1. index.html (self-contained dashboard) 
2. lab_page.h (the same page embedded in Arduino PROGMEM)
3. this guide. 
No external assets or Internet access are needed. The page includes only ultrasonic distance and 8 LED controls. Wi-Fi provisioning must use a separate page/implementation.

To use it,
1. Copy lab_page.h into your sketch folder and include it;
2. With WebServer server(80):
server.on("/", HTTP_GET, []() { 
server.send_P(200, "text/html", LAB_PAGE); 
});

Register handlers, call server.begin(), and service server.handleClient() in loop().

Required student handlers:
GET /api/distance -> {"distance_cm":25.4,"valid":true}
No valid echo -> {"distance_cm":null,"valid":false}
GET /api/leds -> {"leds":[eight state objects]}
Each state object: {"id":0,"r":255,"g":0,"b":0,"brightness":128}
PUT /api/leds/{id} body: {"r":255,"g":0,"b":0,"brightness":128}
Return the resulting state including id, only after applying it to the hardware.
LED labels 1-8 map to IDs 0-7. RGB/brightness are integers from 0 to 255.
Use JSON responses; for errors use an appropriate HTTP error status and
{"error":"description"}. Preserve all other LEDs when changing one LED.

No reading is taken on page load or on a timer. Request distance triggers one GET.
Load device settings is read-only. A colour/brightness edit is an unsaved draft;
Apply updates that LED. Synced means acknowledged by the latest API response,
not continuous monitoring. Load device settings overwrites all local drafts.

Opening the HTML locally previews its design only. Serve it from the ESP32-S3 for
working relative API URLs. Handlers are intentionally left for students. Frontend tested with mocked responses; no physical ESP32 hardware test performed.
