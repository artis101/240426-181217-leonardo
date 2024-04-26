#include <Arduino.h>
#include <Joystick.h>
#include <LedControl.h>
#include <RotaryEncoder.h>

#define LC_DIN_PIN 3           // DIN (black to yellow)
#define LC_CLK_PIN 5           // CLK (white to blue)
#define LC_CS_PIN 6            // LOAD/CS (grey to green)
#define LC_NUM_CHAIN_DEVICES 2 // num MAX72xx devices on the chain

#define RESET_ALTITUDE_BTN_PIN 4

#define JOYSTICK_NUM_BTNS 5

#define INNER_RING_LEFT_TURN_BTN 1
#define INNER_RING_RIGHT_TURN_BTN 0
#define OUTER_RING_LEFT_TURN_BTN 3
#define OUTER_RING_RIGHT_TURN_BTN 2
#define RESET_ALTITUDE_BTN 4

// Create the Joystick
Joystick_ Joystick(0x03, JOYSTICK_TYPE_JOYSTICK, 5, 0, false, false, false,
                   false, false, false, false, false, false, false, false);

LedControl lc =
    LedControl(LC_DIN_PIN, LC_CLK_PIN, LC_CS_PIN, LC_NUM_CHAIN_DEVICES);

RotaryEncoder innerEncoder(A3, A4, RotaryEncoder::LatchMode::TWO03);
RotaryEncoder outerEncoder(A1, A2, RotaryEncoder::LatchMode::TWO03);

int innerPos = 0;
int outerPos = 0;

bool altitudeButtonPressed = false;
bool innerEncoderTurningLeft = false;
bool innerEncoderTurningRight = false;
bool outerEncoderTurningLeft = false;
bool outerEncoderTurningRight = false;

long currentAltitude = 0;
unsigned long lastEncoderChangeTime = 0;

void printNumberOnLCD(long v) {
  int ones;
  int tens;
  int hundreds;
  int thousands;
  int tenThousands;
  int hundredThousands;
  int millions;
  int tenMillions;

  boolean negative = false;

  if (v < -99999 || v > 99999)
    return; // Number out of range
  if (v < 0) {
    negative = true;
    v = -v; // Make the number positive for processing
  }

  // Extract digits
  ones = v % 10;
  v /= 10;
  tens = v % 10;
  v /= 10;
  hundreds = v % 10;
  v /= 10;
  thousands = v % 10;
  v /= 10;
  tenThousands = v % 10;

  // Decide where to print the '-' sign or a space
  if (negative) {
    // Assuming first digit space of the first display (device 0) is reserved
    // for sign
    lc.setChar(0, 5, '-', false);
  } else {
    lc.setChar(0, 5, ' ', false);
  }

  // zero padded 5 digit number print
  lc.setDigit(0, 4, (byte)tenThousands, false);
  lc.setDigit(0, 3, (byte)thousands, false);
  lc.setDigit(0, 2, (byte)hundreds, false);
  lc.setDigit(0, 1, (byte)tens, false);
  lc.setDigit(0, 0, (byte)ones, false);
}

long readEncoderRing(RotaryEncoder &encoder, int *posRef, int step = 1,
                     int leftTurnJoystickButton = 0,
                     int rightTurnJoystickButton = 1) {
  encoder.tick(); // Read the encoder, make sure this is called.
                  // @TODO: cover function call with tests

  int newPos = encoder.getPosition();
  if (newPos != *posRef) {
    int adjustment = 0;
    lastEncoderChangeTime = millis();

    if (newPos < *posRef) {
      adjustment += step;

      Joystick.pressButton(leftTurnJoystickButton);
      Joystick.releaseButton(rightTurnJoystickButton);
    } else if (newPos > *posRef) {
      adjustment -= step;
      Joystick.pressButton(rightTurnJoystickButton);
      Joystick.releaseButton(leftTurnJoystickButton);
    }

    *posRef = newPos; // Update the position reference

    return adjustment;
  } else if (millis() - lastEncoderChangeTime > 100) {
    Joystick.releaseButton(leftTurnJoystickButton);
    Joystick.releaseButton(rightTurnJoystickButton);
  }

  return 0;
}

void setup() {
  Joystick.begin();

  for (int index = 0; index < lc.getDeviceCount(); index++) {
    lc.shutdown(index, false);
    // set a medium brightness for the Leds
    lc.setIntensity(index, 8);
  }

  // use internal pull-up resistor for the reset button
  pinMode(RESET_ALTITUDE_BTN_PIN, INPUT_PULLUP);

  printNumberOnLCD(currentAltitude);
}

void loop() {
  long targetAltitude = currentAltitude;

  /**
   * Read the encoder rings and adjust the target altitude.
   * Rotating the inner encoder ring adjusts the altitude by 100 feet per click.
   * Rotating the outer encoder ring adjusts the altitude by 1000 feet per
   * click. Pressing the inner encoder ring resets the altitude to 0.
   */
  targetAltitude +=
      readEncoderRing(innerEncoder, &innerPos, 100, INNER_RING_LEFT_TURN_BTN,
                      INNER_RING_RIGHT_TURN_BTN);
  targetAltitude +=
      readEncoderRing(outerEncoder, &outerPos, 1000, OUTER_RING_LEFT_TURN_BTN,
                      OUTER_RING_RIGHT_TURN_BTN);

  // reset altitude to 0 if the reset button is pressed
  if (digitalRead(RESET_ALTITUDE_BTN_PIN) == LOW) {
    targetAltitude = 0;

    if (altitudeButtonPressed == false) {
      Joystick.pressButton(RESET_ALTITUDE_BTN);
      altitudeButtonPressed = true;
    }
  } else if (altitudeButtonPressed == true) {
    Joystick.releaseButton(RESET_ALTITUDE_BTN);
    altitudeButtonPressed = false;
  }

  // read from serial port and parse commands
  if (Serial.available() > 0) {
    char command = Serial.read();
    switch (command) {
    case 'r': // reset altitude
      currentAltitude = 0;
      printNumberOnLCD(currentAltitude);
      break;
    case 's': // send current hardware selected altitude
      Serial.println(currentAltitude);
      break;
    case 'i': // increase altitude by 100 feet
      currentAltitude += 100;
      printNumberOnLCD(currentAltitude);
      Serial.println(currentAltitude);
      break;
    case 'I': // increase altitude by 1000 feet
      currentAltitude += 1000;
      printNumberOnLCD(currentAltitude);
      Serial.println(currentAltitude);
      break;
    case 'd': // decrease altitude by 100 feet
      currentAltitude -= 100;
      printNumberOnLCD(currentAltitude);
      Serial.println(currentAltitude);
      break;
    case 'D': // decrease altitude by 1000 feet
      currentAltitude -= 1000;
      printNumberOnLCD(currentAltitude);
      Serial.println(currentAltitude);
      break;
    default:
      break;
    }
  } else {
    // functions that modify target altitude
    if (targetAltitude != currentAltitude) {
      Serial.println(targetAltitude);

      printNumberOnLCD(targetAltitude);

      currentAltitude = targetAltitude;
    }
  }
}