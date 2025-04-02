#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h> // Include the ESP32Servo library
#include <queue> // Include the queue library for command buffering

#define PIN_NEOPIXEL 39  // Define the pin where the NeoPixel is connected
#define NUM_PIXELS 1     // Number of NeoPixels
#define SERVO_PIN 35     // Change the servo pin to pin 35
#define CPX 18           // Define CPX as pin 18
#define CWX 17           // Define CWX as pin 17
#define CPY 9           // Define CPY as pin 16
#define CWY 8          // Define CWY as pin 15
#define VERSION "0.1" // Define the current version of the program

Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Servo servo; // Create a Servo object
std::queue<String> commandQueue; // Queue to store commands

// Define feedrate for X and Z axes (steps per second)
int X_FEEDRATE = 1000; // Feedrate for X-axis
int Z_FEEDRATE = 1000; // Feedrate for Z-axis

#define OFF 0x000000
#define RED 0xFF0000
#define GREEN 0x00FF00
#define YELLOW 0xFFFF00
#define BLUE 0x0000FF
#define MAGENTA 0xFF00FF

void led_on(uint32_t color) {
  strip.fill(color);
  strip.show();
}

void led_off() {
  strip.fill(OFF);
  strip.show();
}

void setReadyState() {
  led_on(GREEN); // Turn LED to GREEN to indicate ready state
}

bool isBlinking = false; // Flag to indicate if blinking is active
unsigned long blinkStartTime = 0; // Start time for blinking
unsigned long lastBlinkTime = 0; // Last time the LED toggled
int blinkCount = 0; // Counter for the number of blinks
const unsigned long blinkInterval = 200; // Blink interval in milliseconds
const int maxBlinks = 6; // Total number of blinks (3 ON/OFF cycles)

void startExecutingState() {
  isBlinking = true; // Start blinking
  blinkStartTime = millis(); // Record the start time
  lastBlinkTime = millis(); // Initialize the last blink time
  blinkCount = 0; // Reset the blink counter
}

void updateExecutingState() {
  if (isBlinking) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = currentMillis; // Update the last blink time
      blinkCount++; // Increment the blink counter

      // Toggle the LED state
      if (blinkCount % 2 == 1) {
        led_on(RED); // Turn LED ON
      } else {
        led_off(); // Turn LED OFF
      }

      // Stop blinking after the maximum number of blinks
      if (blinkCount >= maxBlinks) {
        isBlinking = false; // Stop blinking
        led_off(); // Ensure the LED is OFF
      }
    }
  }
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

void processCommand(String command) {
  char axis = command.charAt(0); // Extract the first character (e.g., 'S', 'Z', 'X', 'D')
  int value = command.substring(1).toInt(); // Extract the numeric value after the axis

  static int stepCounter = 0; // Counter to track the step in the program

  switch (axis) {
    case 'S': // Servo control
      Serial.printf("[Step %d] Servo command received with angle: %d\n", ++stepCounter, value);
      if (value >= 0 && value <= 360) { // Support angles between 0 and 360
        int mappedAngle = map(value, 0, 360, 0, 180); // Map 0-360 to 0-180 for ESP32Servo
        servo.write(mappedAngle); // Move servo to the specified angle
        delay(20); // Add a delay to stabilize the PWM signal
        led_on(GREEN); // Indicate successful movement
      } else {
        Serial.println("Invalid servo angle. Please enter a value between 0 and 360.");
        led_on(RED); // Indicate error
      }
      break;

    case 'Z': // Z-axis stepper motor control
      Serial.printf("[Step %d] Z-axis command received with value: %d\n", ++stepCounter, value);
      moveStepper(value, CPY, CWY, Z_FEEDRATE); // Move Z-axis stepper motor
      break;

    case 'X': // X-axis stepper motor control
      Serial.printf("[Step %d] X-axis command received with value: %d\n", ++stepCounter, value);
      moveStepper(value, CPX, CWX, X_FEEDRATE); // Move X-axis stepper motor
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
        led_on(RED); // Indicate error
      }
      break;

    default:
      Serial.printf("[Step %d] Invalid command received: %s\n", ++stepCounter, command.c_str());
      Serial.println("Invalid command. Use 'S', 'Z', 'X', or 'D' followed by a value.");
      led_on(RED); // Indicate error
      delay(500);  // Allow time for the user to notice the error
      break;
  }

  led_off(); // Turn off LED after indication
}

void processBuffer(String buffer) {
  int repeatCount = 1; // Default repeat count is 1
  if (buffer.startsWith("(")) {
    int endParen = buffer.indexOf(")");
    if (endParen != -1) {
      repeatCount = buffer.substring(1, endParen).toInt(); // Extract repeat count
      buffer = buffer.substring(endParen + 1); // Remove the repeat count from the buffer
    }
  }

  for (int i = 0; i < repeatCount; i++) { // Repeat the sequence
    int start = 0;
    while (start < buffer.length()) {
      int end = buffer.indexOf(',', start); // Find the next comma
      if (end == -1) { // If no more commas, process the last command
        end = buffer.length();
      }
      String command = buffer.substring(start, end); // Extract the command
      command.trim(); // Remove any extra whitespace
      commandQueue.push(command); // Add the command to the queue
      start = end + 1; // Move to the next command
    }
  }
}

void processNextCommand() {
  if (!commandQueue.empty()) {
    startExecutingState(); // Start blinking to indicate execution
    String command = commandQueue.front(); // Get the next command
    commandQueue.pop(); // Remove the command from the queue
    processCommand(command); // Process the command
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
    commandBuffer = ""; // Clear the buffer
  }

  // Update the blinking state
  updateExecutingState();

  // Execute the next command in the queue if available
  if (!commandQueue.empty() && !isBlinking) { // Only process commands when not blinking
    processNextCommand();
  }

  // Indicate the system is ready if no commands are in the queue and not blinking
  if (commandQueue.empty() && !isBlinking) {
    setReadyState();
  }
}

void setup() {
  Serial.begin(115200); // Initialize serial communication
  while (!Serial) {
    // Wait for the serial connection to be established
    delay(1000);
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
  servo.write(0);                     // Set the servo to its initial position (0 degrees)

  led_on(GREEN); // Turn LED to GREEN to indicate ready state
  Serial.print("READY "); // Output READY message to serial
  Serial.println(VERSION); // Append the version to the READY message
}
