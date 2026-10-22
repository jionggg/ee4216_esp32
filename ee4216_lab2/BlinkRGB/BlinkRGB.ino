#include <Adafruit_NeoPixel.h>
#include <NewPing.h>


// Define the GPIO pin where the built-in RGB LED is connected
#define LED_PIN GPIO_NUM_38  // Adjust this pin number to your specific board's configuration
const int NUM_LEDS = 1;
const int TRIG_PIN = 4;
const int ECHO_PIN = 5;
const int MAX_DISTANCE_CM = 400;  // HC-SR04 max range for newping library
const int LED_BAR_PIN  = 6;   // LED bar DIN
const int NUM_BAR_LEDS = 8;
const float SOUND_SPEED_CM_PER_US = 0.0343;
const unsigned long ECHO_TIMEOUT_US = 30000;
const uint8_t neoIntensity = 128;  
// Define the number of LEDs in the strip. For a single built-in RGB LED, this is 1.


NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE_CM); // newping pbject

// Create an instance of the Adafruit_NeoPixel class
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel bar(NUM_BAR_LEDS, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);

// functions init
void cycleLedColors();
float readNewPingCm();
float readNewPingCm();
void showStaticPattern();
void showDynamicPattern(float distanceCm);
// void showRangeColor();


void setup() {
  Serial.begin(115200);
  
  // Initialize the NeoPixel library
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  bar.begin();
  // bar.show();

  
  Serial.println("Onboard LED blink starts");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void loop() {
  // TRIG starts LOW for a clean pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  // trigger pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // how long ECHO stays HIGH
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);

  // // maunual
  // if (duration == 0) {
  //     Serial.println("No echo (out of range or timeout)");
  //   } else {
  //     float distanceCm = (duration * SOUND_SPEED_CM_PER_US) / 2.0;
  //     Serial.print("Echo time: ");
  //     Serial.print(duration);
  //     Serial.print(" us  |  Distance: ");
  //     Serial.print(distanceCm, 1);
  //     Serial.println(" cm");
  //   }
  

  // newPrint library
  float distanceCm = readNewPingCm();

  if (distanceCm < 0) {
    Serial.println("NewPing: no echo (out of range)");
  } else {
    Serial.print("NewPing distance: ");
    Serial.print(distanceCm, 1);
    Serial.println(" cm");
  }

  // showStaticPattern();// task b
  // showDynamicPattern(distanceCm); // part c
  showRangeColor(distanceCm); // part d
  // showRangeColor(cm);
  // showDistanceBar(cm);
  //     Serial.print(" | LEDs lit: ");
  //     Serial.print(ledsForDistance(cm));

  // uint32_t colorForDistance(float cm) {
  //   if (cm > 100) return bar.Color(0, LEVEL, 0);      // Green
  //   if (cm >= 30) return bar.Color(LEVEL, LEVEL, 0);  // Yellow
  //   return bar.Color(LEVEL, 0, 0);                    // Red
  // }


  
  delay(100);
}























// Cycles the LED through white, red, green, blue, then off
void cycleLedColors() {
  // Set the built-in RGB LED to white
  strip.setPixelColor(0, strip.Color(64, 64, 64)); // White color
  strip.show();
  Serial.println("white");
  delay(1000); // Wait for 1 second
  
  // Set the built-in RGB LED to red
  strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red color
  strip.show();
  Serial.println("red");
  delay(1000); // Wait for 1 second

  // Set the built-in RGB LED to green
  strip.setPixelColor(0, strip.Color(0, 255, 0)); // Green color
  strip.show();
  Serial.println("green");
  delay(1000); // Wait for 1 second

  // Set the built-in RGB LED to blue
  strip.setPixelColor(0, strip.Color(0, 0, 255)); // Blue color
  strip.show();
  Serial.println("blue");
  delay(1000); // Wait for 1 second

  // Turn off the built-in RGB LED
  strip.setPixelColor(0, strip.Color(0, 0, 0)); // Off
  strip.show();
  Serial.println("OFF");
  delay(1000); // Wait for 1 second
}

  // newPing ultrasonic sensor library
  float readNewPingCm() {
    // Take 5 pings, return the median echo time in µs (filters out odd readings)
    unsigned long duration = sonar.ping_median(5);

    if (duration == 0) {
      return -1;  // no echo / out of range
    }

    return (duration * SOUND_SPEED_CM_PER_US) / 2.0;
  }



  // neoPixel 
  void showStaticPattern() { // part b
    bar.clear(); // all off first
    bar.setPixelColor(0, bar.Color(neoIntensity, neoIntensity, neoIntensity)); // white
    bar.setPixelColor(2, bar.Color(neoIntensity, 0, 0));  // red
    bar.setPixelColor(4, bar.Color(0, neoIntensity, 0)); // green
    bar.setPixelColor(6, bar.Color(0, 0, neoIntensity)); // blue
    bar.show(); // 1, 3, 5, 7 stay off
  }
  void showDynamicPattern(float distanceCm) { //part c
    bar.clear();   
    if (distanceCm>100) {
      bar.setPixelColor(4, bar.Color(0, neoIntensity, 0)); // Green
    }
    else if (distanceCm >=30) {
      bar.setPixelColor(2, bar.Color(neoIntensity/2, neoIntensity/2, 0)); // yellow/orange?
    }
    else {
      bar.setPixelColor(2, bar.Color(neoIntensity, 0, 0)); // red
    }
    bar.show();
  }
  void showRangeColor(float distanceCm) {
    uint32_t color = strip.Color(0, 0, 0);
    uint16_t count = 0;
    bar.clear(); 

    // part d
    if (distanceCm <= 10) count = 8;
    else if (distanceCm <= 23) count = 7;
    else if (distanceCm <= 36) count = 6;
    else if (distanceCm <= 49) count = 5;
    else if (distanceCm <= 61) count = 4;
    else if (distanceCm <= 74) count = 3;
    else if (distanceCm <= 87) count = 2;
    else count = 1;

    if (distanceCm>100) {
      color = bar.Color(0, neoIntensity, 0); // Green
    }
    else if (distanceCm >=30) { 
      color = bar.Color(neoIntensity/2, neoIntensity/2, 0); // yellow/orange?
    }

    else {
      color = bar.Color(neoIntensity, 0, 0); // red
    }

    bar.fill(color, 0, count);
    bar.show();
  }


