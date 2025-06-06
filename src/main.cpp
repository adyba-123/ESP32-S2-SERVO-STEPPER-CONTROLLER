#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h> // Include the ESP32Servo library
#include <queue>        // Include the queue library for command buffering
#include <WiFi.h>       // Include WiFi library for access point
#include <ESPAsyncWebServer.h> // Include ESPAsyncWebServer library
#include <SPIFFS.h>     // Include SPIFFS for file storage

#define PIN_NEOPIXEL 39  // Define the pin where the NeoPixel is connected
#define NUM_PIXELS 1     // Number of NeoPixels
#define SERVO_PIN 35     // Change the servo pin to pin 35
#define CPX 18           // Define CPX as pin 18
#define CWX 17           // Define CWX as pin 17
#define CPY 9            // Define CPY as pin 16
#define CWY 8            // Define CWY as pin 15
#define VERSION "0.9"    // Define the current version of the program

Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Servo servo; // Create a Servo object
std::queue<String> commandQueue; // Queue to store commands

// Define feedrate for X and Z axes (steps per second)
int X_FEEDRATE = 1000; // Feedrate for X-axis
int Z_FEEDRATE = 1000; // Feedrate for Z-axis

// Global variables to hold X and Z values from the web page
int globalXValue = -2000;

// Global variable to hold the command buffer
String globalCommandBuffer = "";

int globalDelayMs = 50; // Global delay in milliseconds between commands for stability
int servoStabilizationDelayMs = 500; // Configurable delay for servo stabilization

#define OFF 0x000000
#define RED 0xFF0000
#define GREEN 0x00FF00
#define YELLOW 0xFFFF00
#define BLUE 0x0000FF
#define MAGENTA 0xFF00FF

#define WIFI_PASSWORD ""             // Open network (no password)

AsyncWebServer server(80); // Create an AsyncWebServer object on port 80

void led_on(uint32_t color) {
  strip.fill(color);
  strip.show();
}

void led_off() {
  strip.fill(OFF);
  strip.show();
}

void setProcessingState() {
  led_on(RED); // Set LED to RED to indicate processing state
}

void setReadyState() {
  led_on(GREEN); // Set LED to GREEN to indicate ready state
}

void moveStepper(int steps, int pulsePin, int directionPin, int feedrate) {
  digitalWrite(directionPin, steps > 0 ? HIGH : LOW); // Set direction
  steps = abs(steps); // Use absolute value for step count

  unsigned long delayTime = 1000000UL / (2 * feedrate); // Calculate delay time
  for (int i = 0; i < steps; i++) {
    digitalWrite(pulsePin, HIGH); // Generate pulse
    delayMicroseconds(delayTime); // Half-period delay for feedrate
    digitalWrite(pulsePin, LOW); // End pulse
    delayMicroseconds(delayTime); // Half-period delay for feedrate
  }
}

// Define soft limits for the servo
const int LowAngle = 170; // Minimum allowed angle
const int HighAngle = 345; // Maximum allowed angle

void moveServo(int angle) {
  if (angle >= LowAngle && angle <= HighAngle) { // Enforce soft limits
    int mappedAngle = map(angle, 0, 360, 0, 180); // Map 0-360 to 0-180 for ESP32Servo
    servo.write(mappedAngle); // Move servo to the specified angle
    delay(servoStabilizationDelayMs); // Use configurable delay
    Serial.printf("Servo moved to angle: %d\n", angle);
  } else {
    Serial.printf("Invalid servo angle: %d. Allowed range is %d to %d.\n", angle, LowAngle, HighAngle);
  }
}

void moveStepperWithDelay(int steps, int pulsePin, int directionPin, int feedrate, int delayMs) {
  delay(delayMs); // Delay before starting stepper movement
  moveStepper(steps, pulsePin, directionPin, feedrate); // Move stepper motor
  Serial.printf("Stepper moved %d steps with delay of %d ms\n", steps, delayMs);
}

void processCommand(String command) {
  char axis = command.charAt(0); // Extract the first character (e.g., 'S', 'Z', 'X', 'D', 'F')
  int value = command.substring(1).toInt(); // Extract the numeric value after the axis

  static int stepCounter = 0; // Counter to track the step in the program

  setProcessingState(); // Indicate processing state

  switch (axis) {
    case '\x03': // CTRL+C (ASCII code 3)
      Serial.printf("[Step %d] CTRL+C received. Stopping all operations.\n", ++stepCounter);
      commandQueue = std::queue<String>(); // Clear the command queue
      setReadyState(); // Indicate ready state
      break;

    case 'S': // Servo control
      Serial.printf("[Step %d] Servo command received with angle: %d\n", ++stepCounter, value);
      moveServo(value); // Move the servo
      break;

    case 'Z': // Z-axis stepper motor control
      Serial.printf("[Step %d] Z-axis command received with value: %d\n", ++stepCounter, value);
      moveStepperWithDelay(value, CPY, CWY, Z_FEEDRATE, globalDelayMs); // Move Z-axis stepper motor with delay
      break;

    case 'X': // X-axis stepper motor control
      Serial.printf("[Step %d] X-axis command received with value: %d\n", ++stepCounter, value);
      moveStepperWithDelay(value, CPX, CWX, X_FEEDRATE, globalDelayMs); // Move X-axis stepper motor with delay
      break;

    case 'D': // Delay in milliseconds
      Serial.printf("[Step %d] Delay command received with value: %d ms\n", ++stepCounter, value);
      if (value > 0) {
        unsigned long startTime = millis();
        while (millis() - startTime < value) {
          // Non-blocking delay: Allow other tasks to run during the delay
          if (Serial.available() > 0) {
            char incomingChar = Serial.read(); // Read incoming characters
            Serial.print("Ignoring input during delay: ");
            Serial.println(incomingChar);
          }
        }
      } else {
        Serial.println("Invalid delay value. Please enter a positive number.");
      }
      break;

    case 'F': // Change feedrate for X-axis
      if (value > 0) {
        X_FEEDRATE = value;
        Serial.printf("Feedrate for X-axis updated to %d steps per second.\n", value);
      } else {
        Serial.println("Invalid feedrate value for X-axis. Please enter a positive number.");
      }
      break;

    case 'G': // Change feedrate for Z-axis
      if (value > 0) {
        Z_FEEDRATE = value;
        Serial.printf("Feedrate for Z-axis updated to %d steps per second.\n", value);
      } else {
        Serial.println("Invalid feedrate value. Please enter a positive number.");
      }
      break;

    case 'H': // Set globalXValue
      Serial.printf("[Step %d] H command received with value: %d\n", ++stepCounter, value);
      globalXValue = value;
      Serial.printf("globalXValue updated to: %d\n", globalXValue);
      break;

    case 'C': // Set globalDelayMs
      Serial.printf("[Step %d] C command received with value: %d\n", ++stepCounter, value);
      if (value >= 0) {
        globalDelayMs = value;
        Serial.printf("globalDelayMs updated to: %d ms\n", globalDelayMs);
      } else {
        Serial.println("Invalid delay value. Please enter a non-negative number.");
      }
      break;

    default:
      Serial.printf("[Step %d] Invalid command received: %s\n", ++stepCounter, command.c_str());
      Serial.println("Invalid command. Use 'S', 'Z', 'X', 'D', 'F', 'G' or 'H' followed by a value.");
      break;
  }

  setReadyState(); // Indicate ready state
}

void processBuffer(String buffer) {
  globalCommandBuffer = buffer; // Update the global command buffer
  static int stepCounter = 0;   // Counter to track the step in the program
  stepCounter = 0;              // Reset step counter for each new program

  int start = 0;
  while (start < buffer.length()) {
    int end = buffer.indexOf(',', start); // Find the next comma
    if (end == -1) {                      // If no more commas, process the last command
      end = buffer.length();
    }
    String command = buffer.substring(start, end); // Extract the command
    command.trim();                                // Trim whitespace
    commandQueue.push(command);                   // Add the command to the queue
    start = end + 1;                               // Move to the next command
  }
}

void processNextCommand() {
  if (!commandQueue.empty()) {
    String command = commandQueue.front(); // Get the next command
    commandQueue.pop(); // Remove the command from the queue
    processCommand(command); // Process the command
    delay(globalDelayMs); // Add delay for stability
    Serial.printf("COMMAND SENT with delay: %d ms\n", globalDelayMs); // Print command sent message with delay
    led_on(GREEN); // Indicate ready state after program finishes
  }
}

void loop() {
  static String commandBuffer = ""; // Buffer to store incoming characters

  while (Serial.available() > 0) {
    char incomingChar = Serial.read(); // Read one character at a time

    if (incomingChar == ',') { // If a comma is received, buffer the current command
      commandBuffer.trim(); // Remove any extra whitespace
      if (!commandBuffer.isEmpty()) {
        commandQueue.push(commandBuffer); // Add the command to the queue
      }
      commandBuffer = ""; // Clear the buffer for the next command
    } else {
      commandBuffer += incomingChar; // Append the character to the buffer
    }
  }

  // If there are no more characters to read and the buffer is not empty, process the last command
  if (!commandBuffer.isEmpty() && commandBuffer.indexOf(',') == -1) {
    commandBuffer.trim(); // Remove any extra whitespace
    commandQueue.push(commandBuffer); // Add the last command to the queue
    commandBuffer = ""; // Clear the buffer after adding the command
  }

  // Execute the next command in the queue if available
  if (!commandQueue.empty()) {
    processNextCommand();
  }

  // Indicate the system is ready if no commands are in the queue
  if (commandQueue.empty()) {
    setReadyState();
  }
}

void deleteConfigFile() {
  if (SPIFFS.exists("/config.txt")) {
    if (SPIFFS.remove("/config.txt")) {
      Serial.println("Config file deleted successfully.");
    } else {
      Serial.println("Failed to delete config file.");
    }
  } else {
    Serial.println("Config file does not exist.");
  }
}

void saveValues(AsyncWebServerRequest *request) {
  File file = SPIFFS.open("/config.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open config file for writing.");
    return;
  }
  file.printf("XSpeed:%d\n", X_FEEDRATE);
  file.printf("ZSpeed:%d\n", Z_FEEDRATE);
  file.printf("globalXValue:%d\n", globalXValue); // Save globalXValue
  file.printf("globalDelayMs:%d\n", globalDelayMs); // Save globalDelayMs
  // Save the global command buffer
  file.printf("CommandBuffer:%s\n", globalCommandBuffer.c_str());
  Serial.printf("CommandBuffer:%s\n", globalCommandBuffer.c_str());

  file.close();
  Serial.println("Values saved to config file.");
}

void loadValues() {
  File file = SPIFFS.open("/config.txt", FILE_READ);
  if (!file) {
    Serial.println("Config file not found. Continuing with default values.");
    return; // Continue with default values if the file does not exist
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.startsWith("XSpeed:")) {
      X_FEEDRATE = line.substring(7).toInt();
    } else if (line.startsWith("ZSpeed:")) {
      Z_FEEDRATE = line.substring(7).toInt();
    } else if (line.startsWith("globalXValue:")) {
      globalXValue = line.substring(13).toInt(); // Load globalXValue
    } else if (line.startsWith("globalDelayMs:")) {
      globalDelayMs = line.substring(14).toInt(); // Load globalDelayMs
    } else if (line.startsWith("CommandBuffer:")) {
      globalCommandBuffer = line.substring(14); // Load the command buffer into the global variable
      globalCommandBuffer.trim(); // Remove any extra whitespace
    }
  }
  file.close();

  Serial.println("Values loaded from config file.");
  Serial.printf("Loaded globalXValue: %d\n", globalXValue);
  Serial.printf("Loaded globalDelayMs: %d ms\n", globalDelayMs);
  Serial.printf("Loaded CommandBuffer: %s\n", globalCommandBuffer.c_str());
}

void setupWiFi() {
  // Get the MAC address of the ESP32
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Format the AP name as "WIRE-BENDER-<VERSION>-<MAC>"
  char apName[40];
  snprintf(apName, sizeof(apName), "WIRE-BENDER-%s-%02X%02X%02X", VERSION, mac[3], mac[4], mac[5]);

  // Start the WiFi access point with the generated name
  WiFi.softAP(apName, WIFI_PASSWORD);

  Serial.print("Access Point started. AP Name: ");
  Serial.println(apName);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  // Serve the HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <title>ESP32-S2 Control )rawliteral" + String(VERSION) + R"rawliteral(</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
          input, textarea { padding: 10px; width: 600px; } /* Buffer text box 200% wider */
          textarea { resize: none; } /* Prevent resizing of the text area */
          .large-button { 
            padding: 30px 60px; /* 200% larger */
            font-size: 24px; 
            font-weight: bold; /* Bold text */
            margin: 10px; 
          }
          .small-button { 
            padding: 10px 20px; 
            font-weight: bold; /* Bold text */
            margin: 5px; 
          }
          .button-container { margin-top: 20px; }
          .example-command, h2 { font-size: 16px; } /* Match size of <label> text */
        </style>
        <script>
          // Removed unused variable `let isRunning = false;`

          function sendCommandBuffer() {
            const commandBuffer = document.getElementById('commandBuffer').value.trim();
            if (!commandBuffer) {
              document.getElementById('response').innerText = 'Error: Command buffer cannot be empty.';
              return;
            }
            updateBackgroundColor('red'); // Indicate program is running
            fetch(`/commandBuffer?buffer=${encodeURIComponent(commandBuffer)}`)
              .then(response => response.text())
              .then(data => {
                document.getElementById('response').innerText = data;
                // Removed the undefined checkStatus function call
              })
              .catch(err => {
                document.getElementById('response').innerText = 'Error sending command buffer';
                updateBackgroundColor('red'); // Indicate error
              });
          }

          function setSpeed(axis) {
            const value = document.getElementById(`${axis}Speed`).value.trim();
            if (!value || isNaN(value)) {
              document.getElementById('response').innerText = `Error: Enter a valid speed for ${axis}-axis.`;
              return;
            }
            const command = axis === 'X' ? `F${value}` : `G${value}`;
            fetch(`/command?cmd=${command}`)
              .then(response => response.text())
              .then(data => {
                document.getElementById('response').innerText = data;
              })
              .catch(err => {
                document.getElementById('response').innerText = `Error setting speed for ${axis}-axis.`;
              });
          }

          function saveValues() {
            fetch(`/saveValues`)
              .then(response => response.text())
              .then(data => {
                document.getElementById('response').innerText = data;
              })
              .catch(err => {
                document.getElementById('response').innerText = 'Error saving values.';
              });
          }

          function loadValues() {
            fetch(`/loadValues`)
              .then(response => response.text())
              .then(data => {
                const values = JSON.parse(data);
                document.getElementById('XSpeed').value = values.XSpeed;
                document.getElementById('ZSpeed').value = values.ZSpeed;
                document.getElementById('commandBuffer').value = values.Buffer;
                document.getElementById('response').innerText = 'Values loaded successfully.';
              })
              .catch(err => {
                document.getElementById('response').innerText = 'Error loading values.';
              });
          }

          function deleteConfig() {
            fetch(`/deleteConfig`)
              .then(response => response.text())
              .then(data => {
                document.getElementById('response').innerText = data;
              })
              .catch(err => {
                document.getElementById('response').innerText = 'Error deleting config file.';
              });
          }

          function updateBackgroundColor(color) {
            document.body.style.backgroundColor = color;
          }

          function loadWire() {
            fetch(`/loadWire`)
              .then(response => response.text())
              .then(data => {
                document.getElementById('response').innerText = data;
              })
              .catch(err => {
                document.getElementById('response').innerText = 'Error executing LOAD WIRE command.';
              });
          }

          function navigateToHelp() {
            window.location.href = '/help';
          }

          
        </script>
      </head>
      <body>
        <h1>WIRE BENDER - )rawliteral" + String(VERSION) + R"rawliteral(</h1>
        <div>
          <textarea id="commandBuffer" rows="4" cols="50" placeholder="Enter command buffer (e.g., S90,Z100,D500)"></textarea>
          <br>
          <div class="button-container">
            <button class="large-button" onclick="sendCommandBuffer()">Send Buffer</button>
            <button class="large-button" onclick="loadWire()">LOAD WIRE</button>
          </div>
          <div class="button-container">
            <button class="small-button" onclick="saveValues()">Save Values</button>
            <button class="small-button" onclick="loadValues()">Load Values</button>
            <button class="small-button" onclick="deleteConfig()">Delete Config File</button>
            <button class="small-button" onclick="window.location.href='/viewConfig'">View Config File</button>
            <button class="small-button" onclick="navigateToHelp()">HELP</button>
          </div>
        </div>
        <div style="margin-top: 20px;">
          <label for="XSpeed">X-axis Speed:</label>
          <input type="number" id="XSpeed" placeholder="Enter speed for X">
          <button class="small-button" onclick="setSpeed('X')">Set X Speed</button>
          <br><br>
          <label for="ZSpeed">Z-axis Speed:</label>
          <input type="number" id="ZSpeed" placeholder="Enter speed for Z">
          <button class="small-button" onclick="setSpeed('Z')">Set Z Speed</button>
        </div>
        <div id="response" style="margin-top: 20px; color: blue;"></div>
      </body>
      </html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  // Add a new endpoint for the help page
  server.on("/help", HTTP_GET, [](AsyncWebServerRequest *request) {
    String helpHtml = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <title>Help - Supported Commands</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
          ul { text-align: left; display: inline-block; }
          a { text-decoration: none; color: blue; font-weight: bold; }
        </style>
      </head>
      <body>
        <h1>Supported Commands</h1>
        <ul>
          <li><strong>S:</strong> Servo control (e.g., S90 for 90 degrees)</li>
          <li><strong>D:</strong> Delay in milliseconds (e.g., D500 for 500ms delay)</li>
          <li><strong>Z:</strong> Z-axis stepper motor control (e.g., Z100 for 100 steps)</li>
          <li><strong>X:</strong> X-axis stepper motor control (e.g., X100 for 100 steps)</li>
          <li><strong>F:</strong> Change feedrate (speed) for X-axis (e.g., F1500 for 1500 steps/second)</li>
          <li><strong>G:</strong> Change feedrate (speed) for Z-axis (e.g., G1200 for 1200 steps/second)</li>
          <li><strong>H:</strong> Set globalXValue (e.g., H-1300 to set globalXValue to -1300)</li>
          <li><strong>C:</strong> Set globalDelayMs (e.g., C100 to set delay to 100 ms)</li>
          <li><strong>LOAD WIRE:</strong> Moves X-axis by the predefined globalXValue.</li>
        </ul>
        <a href="/">Back to Home</a>
      </body>
      </html>
    )rawliteral";
    request->send(200, "text/html", helpHtml);
  });

  // Handle command buffer input
  server.on("/commandBuffer", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("buffer")) {
      String buffer = request->getParam("buffer")->value();
      processBuffer(buffer); // Update the global command buffer and process it
      request->send(200, "text/plain", "Command buffer received: " + buffer);
    } else {
      request->send(400, "text/plain", "Missing 'buffer' parameter");
    }
  });

  // Handle command input
  server.on("/command", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("cmd")) {
      String command = request->getParam("cmd")->value();
      commandQueue.push(command); // Add the command to the queue
      request->send(200, "text/plain", "Command received: " + command);
    } else {
      request->send(400, "text/plain", "Missing 'cmd' parameter");
    }
  });

  server.on("/saveValues", HTTP_GET, [](AsyncWebServerRequest *request) {
      saveValues(request);
    request->send(200, "text/plain", "Values saved successfully.");
  });

  server.on("/loadValues", HTTP_GET, [](AsyncWebServerRequest *request) {
    loadValues(); // Reload values from the config file
    String json = "{\"XSpeed\":" + String(X_FEEDRATE) + 
                  ",\"ZSpeed\":" + String(Z_FEEDRATE) + 
                  ",\"globalXValue\":" + String(globalXValue) + 
                  ",\"globalDelayMs\":" + String(globalDelayMs) + 
                  ",\"Buffer\":\"" + globalCommandBuffer + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/deleteConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    deleteConfigFile();
    request->send(200, "text/plain", "Config file deleted.");
  });

  // Serve the config file contents
  server.on("/viewConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/config.txt")) {
      File file = SPIFFS.open("/config.txt", FILE_READ);
      if (file) {
        String content = "";
        while (file.available()) {
          content += char(file.read()); // Read the entire file content
        }
        file.close();
        String html = "<!DOCTYPE html><html><head><title>Config File</title></head><body>";
        html += "<h1>Config File Contents</h1><pre>" + content + "</pre>";
        html += "<a href='/'>Back to Home</a></body></html>";
        request->send(200, "text/html", html);
      } else {
        request->send(500, "text/plain", "Failed to open config file.");
      }
    } else {
      request->send(404, "text/plain", "Config file not found.");
    }
  });

  // Add a new endpoint to handle the LOAD WIRE command
  server.on("/loadWire", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (globalXValue != 0) {
      String command = "X" + String(globalXValue);
      commandQueue.push(command); // Add the command to the queue
      request->send(200, "text/plain", "LOAD WIRE command executed: " + command);
    } else {
      request->send(400, "text/plain", "Error: globalXValue is not set.");
    }
  });

  server.begin(); // Start the server
  Serial.println("Web server started.");
}

void setup() {
  Serial.begin(115200); // Initialize serial communication
  unsigned long startTime = millis();
  while (!Serial && millis() - startTime < 5000) {
    // Wait for the serial connection to be established for up to 5 seconds
    delay(100);
  }

  Serial.println("Initializing...");
  Serial.print("Servo attached to pin: ");
  Serial.println(SERVO_PIN); // Output the pin number

  strip.begin();  // Initialize the NeoPixel library
  strip.show();   // Turn off all pixels initially

  pinMode(CPX, OUTPUT); // Set CPX as output
  pinMode(CWX, OUTPUT); // Set CWX as output
  pinMode(CPY, OUTPUT); // Set CPY as output
  pinMode(CWY, OUTPUT); // Set CWY as output

  servo.setPeriodHertz(50); // Set the PWM frequency to 50Hz
  servo.attach(SERVO_PIN, 500, 2500); // Attach the servo with min/max pulse widths
  servo.write(map(LowAngle, 0, 360, 0, 180)); // Set the servo to the minimum allowed angle (LowAngle)

  led_on(GREEN); // Turn LED to GREEN to indicate ready state
  Serial.print("READY "); // Output READY message to serial
  Serial.println(VERSION); // Append the version to the READY message

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS. Continuing without file system.");
  } else {
    loadValues(); // Attempt to load values from the config file
  }

  setupWiFi(); // Set up the WiFi access point
  setupWebServer(); // Set up the web server
}