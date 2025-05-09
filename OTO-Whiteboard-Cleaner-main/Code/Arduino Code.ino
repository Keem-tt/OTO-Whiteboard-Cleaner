#include <TMCStepper.h>
#include <SoftwareSerial.h>

#define EN_PIN        4
#define STEP_PIN      3
#define DIR_PIN       2
#define BTN_PIN       9
#define LS_LEFT_PIN   A1
#define LS_RIGHT_PIN  A0

#define R_SENSE       0.11f
#define UART_RX       11
#define UART_TX       10
#define DRIVER_ADDR   0x00

SoftwareSerial SoftSerial(UART_RX, UART_TX);
TMC2209Stepper driver(&SoftSerial, R_SENSE, DRIVER_ADDR);

const unsigned int PULSE_DELAY   = 250;   // µs
const int           CLEAN_SWEEPS_STANDARD = 10;
const int           CLEAN_SWEEPS_DEEP     = 20;
const unsigned long LONG_PRESS_TIME      = 2000; // ms
const unsigned long DOUBLE_PRESS_WINDOW  = 600;  // ms

enum Mode { READY, CALIBRATE, CLEAN_STANDARD, CLEAN_DEEP };
Mode currentMode = READY;

void setup() {
  pinMode(EN_PIN,       OUTPUT);
  pinMode(STEP_PIN,     OUTPUT);
  pinMode(DIR_PIN,      OUTPUT);
  pinMode(BTN_PIN,      INPUT_PULLUP);
  pinMode(LS_LEFT_PIN,  INPUT_PULLUP);
  pinMode(LS_RIGHT_PIN, INPUT_PULLUP);

  digitalWrite(EN_PIN, LOW);
  Serial.begin(9600);
  SoftSerial.begin(115200);
  delay(500);

  driver.begin();
  driver.toff(5);
  driver.rms_current(1800);
  driver.microsteps(1);
  driver.en_spreadCycle(true);
  driver.pwm_autoscale(true);

  Serial.println("Power on: starting calibration...");
  calibrate();
  Serial.println("Calibration complete, ready.");
}

void loop() {
  Mode selected = readButtonPattern();
  switch (selected) {
    case CALIBRATE:
      Serial.println("Re-calibrating...");
      calibrate();
      Serial.println("Calibration done, ready.");
      break;
    case CLEAN_DEEP:
      Serial.println("Deep clean mode selected.");
      runCleaning(CLEAN_SWEEPS_DEEP);
      Serial.println("Deep clean complete.");
      break;
    case CLEAN_STANDARD:
      Serial.println("Standard clean mode.");
      runCleaning(CLEAN_SWEEPS_STANDARD);
      Serial.println("Cleaning complete.");
      break;
    default:
      // no action, wait for next input
      break;
  }
}

// Moves to left limit, backs off, then to right limit, backs off
void calibrate() {
  // Home left
  digitalWrite(DIR_PIN, LOW);
  while (digitalRead(LS_LEFT_PIN)) stepOnce();
  delay(200);
  // Back off
  digitalWrite(DIR_PIN, HIGH);
  for (int i = 0; i < 5; i++) stepOnce();
  delay(200);

  // Home right
  digitalWrite(DIR_PIN, HIGH);
  while (digitalRead(LS_RIGHT_PIN)) stepOnce();
  delay(200);
  // Back off
  digitalWrite(DIR_PIN, LOW);
  for (int i = 0; i < 5; i++) stepOnce();
  delay(200);
}

// Perform N sweeps: left-to-right then right-to-left
void runCleaning(int cycles) {
  for (int c = 0; c < cycles; c++) {
    // to left
    digitalWrite(DIR_PIN, LOW);
    while (digitalRead(LS_LEFT_PIN)) stepOnce();
    delay(200);
    // to right
    digitalWrite(DIR_PIN, HIGH);
    while (digitalRead(LS_RIGHT_PIN)) stepOnce();
    delay(200);
  }
  // return home (left)
  digitalWrite(DIR_PIN, LOW);
  while (digitalRead(LS_LEFT_PIN)) stepOnce();
  delay(200);
}

// Detects single, double or long press
Mode readButtonPattern() {
  static unsigned long lastPressTime = 0;
  static bool         waitForRelease = false;

  if (digitalRead(BTN_PIN) == LOW) {
    unsigned long start = millis();
    while (digitalRead(BTN_PIN) == LOW) {
      if (millis() - start > LONG_PRESS_TIME) {
        // long press detected
        waitForRelease = true;
        while (digitalRead(BTN_PIN) == LOW) {}
        delay(100);
        return CALIBRATE;
      }
    }
    delay(50); // debounce
    unsigned long now = millis();
    if (now - lastPressTime < DOUBLE_PRESS_WINDOW) {
      lastPressTime = 0;
      return CLEAN_DEEP; // double-press
    }
    lastPressTime = now;
    // wait briefly for possible second press
    delay(DOUBLE_PRESS_WINDOW);
    if (millis() - now >= DOUBLE_PRESS_WINDOW) {
      return CLEAN_STANDARD; // single-press
    }
  }
  return READY;
}

// Single step pulse
void stepOnce() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(PULSE_DELAY);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(PULSE_DELAY);
}
