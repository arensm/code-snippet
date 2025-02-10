#include <Arduino.h>
#include <ShiftRegister74HC595.h>
#include <EEPROM.h> // Include the EEPROM library

// Pin definitions for the shift registers
#define DIO 3         // Data pin (SDI)
#define CLK 2         // Clock pin (SCLK)
#define LOAD 4        // Latch pin (LOAD)

// Initialize shift register (2 shift registers in series)
ShiftRegister74HC595<2> sr(DIO, CLK, LOAD);

// Pin definitions for the rotary encoder
#define ENCODER_PIN_A 5 // Encoder output A
#define ENCODER_PIN_B 6 // Encoder output B
#define ENCODER_SWITCH 7 // Encoder switch (button)

// Pin for the relay module
#define RELAY_PIN 8

// Variables for the encoder and countdown
int currentCount = 0; // Stores the current countdown value (0-45)
bool counting = false; // Indicates if the countdown is active
unsigned long previousMillis = 0; // Stores the last time the countdown was updated
const int interval = 1000; // Countdown interval (1 second in milliseconds)
const int maxCount = 45; // Maximum countdown value (45 seconds)

// EEPROM address to store the last set timer value
const int eepromAddress = 0;

// 7-Segment display segment definitions (common cathode)
uint8_t numberB[] = {
    B11000000, // 0
    B11111001, // 1
    B10100100, // 2
    B10110000, // 3
    B10011001, // 4
    B10010010, // 5
    B10000011, // 6
    B11111000, // 7
    B10000000, // 8
    B10011000  // 9
};

// Function declaration for updateDisplay
void updateDisplay(int number);

// Variables for encoder debouncing
unsigned long lastEncoderChange = 0; // Timestamp of the last encoder change
const int debounceDelay = 50; // Debounce delay in milliseconds

void setup() {
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Ensure the relay is off at startup (active-low)

  pinMode(ENCODER_PIN_A, INPUT_PULLUP); // Encoder pin A with pullup resistor
  pinMode(ENCODER_PIN_B, INPUT_PULLUP); // Encoder pin B with pullup resistor
  pinMode(ENCODER_SWITCH, INPUT_PULLUP); // Encoder switch with pullup resistor

  // Initialize Serial Communication for debugging
  Serial.begin(9600);
  Serial.println("Timer ready. Waiting for input...");

  // Read the last set timer value from EEPROM
  currentCount = EEPROM.read(eepromAddress);
  if (currentCount > maxCount) {
    currentCount = 0; // Ensure the value is within the valid range
  }

  // Initialize the display to show the last set timer value
  updateDisplay(currentCount);
}

void loop() {
  static int lastA = digitalRead(ENCODER_PIN_A); // Store the initial state of ENCODER_PIN_A
  static bool lastSwitchState = HIGH; // Stores the previous state of the encoder switch

  // Handle encoder switch press
  bool currentSwitchState = digitalRead(ENCODER_SWITCH);
  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    // Switch pressed
    if (!counting) {
      // If countdown is not active, check if a countdown value is set
      if (currentCount > 0) {
        // Start the countdown only if the countdown value is greater than 0
        Serial.println("Encoder switch pressed. Countdown started.");
        counting = true;
        digitalWrite(RELAY_PIN, LOW); // Turn on the relay (active-low)
        previousMillis = millis(); // Reset the countdown timer
      } else {
        // No countdown value set: do not start the countdown
        Serial.println("Encoder switch pressed, but no countdown value set. Relay not activated.");
      }
    } else {
      // If countdown is active, stop the countdown
      Serial.println("Encoder switch pressed. Countdown stopped.");
      counting = false;
      digitalWrite(RELAY_PIN, HIGH); // Turn off the relay (active-low)
      updateDisplay(0); // Clear the display
      currentCount = 0; // Reset the countdown value to 0
    }
    delay(50); // Short delay to debounce the switch
  }
  lastSwitchState = currentSwitchState;

  if (!counting) {
    // Handle encoder rotation with improved debouncing
    int currentA = digitalRead(ENCODER_PIN_A);
    if (currentA != lastA) {
      if (millis() - lastEncoderChange > debounceDelay) { // Debounce check
        if (digitalRead(ENCODER_PIN_A) == LOW) {
          if (digitalRead(ENCODER_PIN_B) == HIGH) {
            // Clockwise rotation: increment the countdown value
            currentCount = (currentCount + 1) % (maxCount + 1); // Increment, wrap at maxCount
            Serial.print("Encoder turned clockwise. Current count: ");
            Serial.println(currentCount);
          } else {
            // Counter-clockwise rotation: decrement the countdown value
            currentCount = (currentCount - 1 + (maxCount + 1)) % (maxCount + 1); // Decrement, wrap at 0
            Serial.print("Encoder turned counter-clockwise. Current count: ");
            Serial.println(currentCount);
          }
          updateDisplay(currentCount); // Update the display with the new countdown value
          // Save the current countdown value to EEPROM
          EEPROM.write(eepromAddress, currentCount);
        }
        lastEncoderChange = millis(); // Update the last encoder change timestamp
      }
      lastA = currentA; // Update the last state of ENCODER_PIN_A
    }
  } else {
    // Handle the countdown
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();
      currentCount--; // Decrement the countdown value
      Serial.print("Counting down. Current value: ");
      Serial.println(currentCount);
      updateDisplay(currentCount); // Update the display with the new countdown value
      if (currentCount <= 0) {
        // Countdown finished
        Serial.println("Countdown finished. Relay turned off.");
        counting = false;
        digitalWrite(RELAY_PIN, HIGH); // Turn off the relay (active-low)
        updateDisplay(0); // Clear the display
        currentCount = 0; // Reset the countdown value to 0
      }
    }
  }
}

// Function to update the 7-segment display
void updateDisplay(int number) {
  int digit1 = number / 10; // Extract the tens digit
  int digit2 = number % 10; // Extract the ones digit

  // Send the digit patterns to the shift register
  uint8_t numberToPrint[] = {numberB[digit1], numberB[digit2]};
  sr.setAll(numberToPrint); // Update both digits
}

/*
Circuit Diagram:
Shift Register (74HC595):
- VCC → Arduino 5V
- GND → Arduino GND
- SDI (DIO) → Arduino Digital Pin 3
- SCLK (CLK) → Arduino Digital Pin 2
- LOAD (LATCH) → Arduino Digital Pin 4

7-Segment Displays (common cathode):
- Connect each segment (A-G) to the shift register outputs.
- Connect the common cathode of each display to GND.

Rotary Encoder:
- OUT A → Arduino Digital Pin 5
- OUT B → Arduino Digital Pin 6
- SWITCH → Arduino Digital Pin 7
- GND → Arduino GND

Relay Module:
- VCC → Arduino 5V
- GND → Arduino GND
- DIN → Arduino Digital Pin 8

Power Supply:
- All VCC pins are connected to Arduino 5V.
- All GND pins are connected to Arduino GND.
*/
