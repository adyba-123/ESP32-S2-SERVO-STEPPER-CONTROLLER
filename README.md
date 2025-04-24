# ESP32-S2-SERVO-STEPPER-CONTROLLER
Simple control for Servo and 2 steppers on ESP32-S2  
Connect to the USB or OPEN WIFI AP  
http://192.168.4.1/

![Interface Screenshot](INTERFACE-0.4.JPG)

## Parts
https://www.adafruit.com/product/5325

## Setup
- Make `S0` straight at 6 o'clock.
- Example commands:
  ```
  Z-1800
  S320, Z1800
  ```

## ZTIE RUN CODE Example
```
S170, S345, X-4250, S170, S345, Z-1800, X-700, S170, Z1800, S345, S170, X-4250, S345, S170, Z-1800, X-700, S345, Z1800, S170, S345, X-4250, S170, S345, X-2000
```

---

## Supported Commands
- **S**: Servo control (e.g., `S90` for 90 degrees).  
- **D**: Delay in milliseconds (e.g., `D500` for 500ms delay).  
- **Z**: Z-axis stepper motor control (e.g., `Z100` for 100 steps).  
- **X**: X-axis stepper motor control (e.g., `X100` for 100 steps).  
- **F**: Change feedrate (speed) for X-axis (e.g., `F1500` for 1500 steps/second).  
- **G**: Change feedrate (speed) for Z-axis (e.g., `G1200` for 1200 steps/second).  
- **H**: Set `globalXValue` (e.g., `H-1300` to set `globalXValue` to -1300).  
- **C**: Set `globalDelayMs` (e.g., `C100` to set delay to 100 ms).  
- **LOAD WIRE**: Moves X-axis by the predefined `globalXValue`.

---

## Functions Overview
### `processCommand(String command)`
Processes individual commands:
- Parses the command type and value.
- Executes the corresponding action (e.g., move servo, stepper motor, or set parameters).

### `processBuffer(String buffer)`
Processes a buffer of commands separated by commas:
- Splits the buffer into individual commands.
- Adds each command to the queue for execution.

### `processNextCommand()`
Executes the next command in the queue:
- Processes the command.
- Adds a delay (`globalDelayMs`) for stability.

### `saveValues(AsyncWebServerRequest *request)`
Saves the current configuration to the SPIFFS file system:
- Saves `X_FEEDRATE`, `Z_FEEDRATE`, `globalXValue`, `globalDelayMs`, and the command buffer.

### `loadValues()`
Loads the configuration from the SPIFFS file system:
- Restores `X_FEEDRATE`, `Z_FEEDRATE`, `globalXValue`, `globalDelayMs`, and the command buffer.

### `setupWiFi()`
Sets up the WiFi access point:
- Creates an open WiFi network with the SSID `ESP32-S2-Control`.

### `setupWebServer()`
Sets up the web server:
- Serves the control interface.
- Handles commands and configuration requests.

### `moveStepper(int steps, int pulsePin, int directionPin, int feedrate)`
Moves a stepper motor:
- Controls the direction and number of steps based on the input parameters.

### `led_on(uint32_t color)` / `led_off()`
Controls the NeoPixel LED:
- Turns the LED on with a specified color or off.

### `setReadyState()`
Indicates the system is ready:
- Sets the LED to green.

---
