/*
 * Created by Andy Waplinger
 * 2024-02-18
 *
 * Uses HomeSpan to natively control NeoPixel LEDs with HomeKit
 *
 * https://github.com/HomeSpan/HomeSpan
 *
 * Notable features:
 *  Auto-launches Wi-Fi setup if no Wi-Fi credentials are found
 *  Over-the-air (OTA) Wi-Fi updates through Arduino IDE
 *  WebSerial provides serial output through a webpage (http://<IP_ADDRESS>:8080/webserial)
 *  Control pin to enable Device Configuration mode via momentary button
 *  Onboard status LED shows Wi-Fi and HomeKit connected status
 *  Simple boolean to enable/disable USB serial input
 *
 * Setup notes:
 *  You must choose a Partition Scheme that supports OTA but minimizes SPIFFS
 *   I'm using "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
 *   If WebSerial is too large or isn't needed, it can be removed
 *  You must disable USB serial input to power the ESP32 with 5V input and not a USB cable
 *   HomeSpan will otherwise get stuck in a loop looking for serial input that doesn't exist and then never boot
 *  Data and Control pins can be changed to other GPIO pins
 *  HomeKit code can be changed during the Wi-Fi configuration process
 *   Default is 466-37-726
 *  OTA password can be changed through USB serial input in the Arduino IDE
 *   Default is "homespan-ota"; send the letter "O" through the serial monitor to change OTA password
 *   Don't forget to disable serial input after you're done
 */

/////////////////////////////////////////////// [ Libraries ] ///////////////////////////////////////////////////

#include <Arduino.h>
#include "HomeSpan.h"

// Libraries for WebSerial
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

///////////////////////////////////////////// [ Key Variables ] /////////////////////////////////////////////////

#define NUM_PIXELS             100                          // Number of pixels in LED strip
char* name =                   "Pixel LED";                 // Customize controller's name, will be shown in the serial output
bool disableUSBserial =        false;                       // Disables USB serial input, allowing HomeSpan to boot without a USB cable
bool enableWebSerial =         false;                       // Enables WebSerial server
String hostname =              String(name);                // Device's hostname generated from its name; e.g., name "Media Center" will become hostname "media-center"

#define NEOPIXEL_PIN       15                               // Data pin for LED strip
#define STA_PIN                2                            // Onboard status LED on ESP32, optional
#define CTL_PIN                4                            // Control pin for physical button, optional

/////////////////////////////////////////////// [ WebSerial ] ///////////////////////////////////////////////////

// Status messages timing in ms
int period = 10000;
unsigned long time_now = 0;

// WebSerial server set to port 8080 to not conflict with HomeSpan/HomeKit on port 80
AsyncWebServer server(8080);

// Message callback of WebSerial 
void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
};

////////////////////////////////////////////// [ NeoPixel RGB ] /////////////////////////////////////////////////

struct NeoPixel_RGB : Service::LightBulb {                      // Addressable single-wire RGB LED strand (e.g. NeoPixel)
 
  Characteristic::On power{0,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{0,true};
  Characteristic::Brightness V{100,true};
  Pixel *pixel;
  int nPixels;
  
  NeoPixel_RGB(uint8_t pin, int nPixels) : Service::LightBulb(){

    V.setRange(1,100,1);                                        // Range of Brightness has a min of 1%, a max of 100%, in steps of 1%
    pixel=new Pixel(pin);                                       // Creates Pixel LED on specified pin
    this->nPixels=nPixels;                                      // Save number of Pixels in this LED strip
    update();                                                   // Manually call update() to set pixel with restored initial values
  }

  boolean update() override {

    int p=power.getNewVal();
    
    float h=H.getNewVal<float>();                               // range = [0,360]
    float s=S.getNewVal<float>();                               // range = [0,100]
    float v=V.getNewVal<float>();                               // range = [0,100]

    Pixel::Color color;

    pixel->set(color.HSV(h*p, s*p, v*p),nPixels);               // sets all nPixels to the same HSV color
          
    return(true);  
  }
};

////////////////////////////////////////////// [ NeoPixel RGBW ] ////////////////////////////////////////////////

struct NeoPixel_RGBW : Service::LightBulb {                     // Addressable single-wire RGBW LED Strand (e.g. NeoPixel)
 
  Characteristic::On power{0,true};
  Characteristic::Brightness V{100,true};
  Characteristic::ColorTemperature T{140,true};
  Pixel *pixel;
  int nPixels;
  
  NeoPixel_RGBW(uint8_t pin, int nPixels) : Service::LightBulb(){

    V.setRange(5,100,1);                                        // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
    pixel=new Pixel(pin,true);                                  // creates Pixel RGBW LED (second parameter set to true for RGBW) on specified pin
    this->nPixels=nPixels;                                      // save number of Pixels in this LED Strand
    update();                                                   // manually call update() to set pixel with restored initial values
  }

  boolean update() override {

    int p=power.getNewVal();
    
    float v=V.getNewVal<float>();                               // range = [0,100]
    float t=T.getNewVal<float>();                               // range = [140,500] (140=coldest, 500=warmest)

    float hue=240-(t-140)/3;                                    // add in a splash of color between blue and green to simulated change of color temperature

    // Pixel::Color color;                                      // if static HSV method is used (below), there is no need to first create a Color object

    pixel->set(pixel->HSV(hue, 100, v*p, v*p),nPixels);         // sets all nPixels to the same HSV color (note use of static method pixel->HSV, instead of defining and setting Pixel::Color)
          
    return(true);  
  }
};

///////////////////////////////////////////////// [ SETUP ] /////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  if (disableUSBserial == true) {
    pinMode(3,INPUT_PULLUP);
  };

  // Create a hostname based on the name
  hostname.replace(" ","-");
  hostname.toLowerCase();

  homeSpan.setControlPin(CTL_PIN);
  homeSpan.setStatusPin(STA_PIN);
  homeSpan.setHostNameSuffix("");
  homeSpan.enableOTA();
  homeSpan.enableAutoStartAP();
  homeSpan.begin(Category::Lighting,name,hostname.c_str());

  if (enableWebSerial == true) {
    WiFi.mode(WIFI_STA);                                        // Required for WebSerial to work
    WebSerial.begin(&server);                                   // Accessible at http://<IP Address>:8080/webserial
    WebSerial.msgCallback(recvMsg);                             // Attach message callback
    server.begin();
  };

  SPAN_ACCESSORY("Pixel RGB");
    new NeoPixel_RGB(NEOPIXEL_PIN,NUM_PIXELS);                  // Create NeoPixel RGB Strand with full color control
  
  /*
  SPAN_ACCESSORY("Neo RGBW");
    new NeoPixel_RGBW(NEOPIXEL_RGBW_PIN,NUM_PIXELS);            // create NeoPixel RGBW Strand with simulated color temperature control 
  */
}

///////////////////////////////////////////////// [ LOOP ] //////////////////////////////////////////////////////

void loop() {
  homeSpan.poll();

  /* Status message */
  if(millis() > time_now + period){
    time_now = millis();
    // USB serial
    Serial.print(F("Name: "));
    Serial.println(name);
    Serial.print(F("LEDs: "));
    Serial.println(NUM_PIXELS);
    Serial.print(F("Hostname: "));
    Serial.println(hostname);
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("SSID: "));
    Serial.print(WiFi.SSID());
    Serial.print(F(" | RSSI: "));
    Serial.println(WiFi.RSSI());
    Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
    Serial.println("--------------------");
    if (enableWebSerial == true) {
      // WebSerial
      WebSerial.print(F("Name: "));
      WebSerial.println(name);
      WebSerial.print(F("LEDs: "));
      WebSerial.println(NUM_PIXELS);
      WebSerial.print(F("Hostname: "));
      WebSerial.println(hostname);
      WebSerial.print(F("IP: "));
      WebSerial.println(WiFi.localIP());
      WebSerial.print(F("SSID: "));
      WebSerial.print(WiFi.SSID());
      WebSerial.print(F(" | RSSI: "));
      WebSerial.println(WiFi.RSSI());
      WebSerial.printf("Free heap: %u\n", ESP.getFreeHeap());
      WebSerial.println("--------------------");
    }
  }
}
