 Uses HomeSpan to natively control NeoPixel LEDs with HomeKit
 
 Notable features:
 * Auto-launches Wi-Fi setup if no Wi-Fi credentials are found
 * Over-the-air (OTA) Wi-Fi updates through Arduino IDE
 * WebSerial provides serial output through a webpage (http://<IP_ADDRESS>:8080/webserial)
 * Control pin to enable Device Configuration mode via momentary button
 * Onboard status LED shows Wi-Fi and HomeKit connected status
 * Boolean to quickly enable/disable USB serial input

Setup notes:
 * You must choose a Partition Scheme that supports OTA but minimizes SPIFFS
   * I'm using "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
   * If WebSerial is too large or isn't needed, it can be removed
 * You must disable USB serial input to power the ESP32 with 5V input and not a USB cable
   * HomeSpan will otherwise get stuck in a loop looking for serial input that doesn't exist and then never boot
 * Data and Control pins can be changed to other GPIO pins
 * HomeKit code can be changed during the Wi-Fi configuration process
   * Default is 466-37-726
 * OTA password can be changed through USB serial input in the Arduino IDE
   * Default is "homespan-ota"; send the letter "O" through the serial monitor to change OTA password
   * Don't forget to disable serial input after you're done
