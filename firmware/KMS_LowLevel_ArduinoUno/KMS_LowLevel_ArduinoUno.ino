/*
 * ==========================================================================
 *  KMS Low-Level Controller - Arduino Uno Port
 * ==========================================================================
 *  Ported from the PlatformIO ESP8266 project (low_level_esp32).
 *  All modules (Config, Protocol, StepperController, SolenoidController,
 *  BatteryMonitor, main) are consolidated into this single .ino file.
 *
 *  Required Library:  AccelStepper by Mike McCauley
 *    Install via Arduino IDE -> Sketch -> Include Library -> Manage Libraries
 *
 *  Board: Arduino Uno (ATmega328P)
 * ==========================================================================
 */

#include <AccelStepper.h>
#include <SoftwareSerial.h>

// =====================================================================
//  CONFIGURATION  (was Config.h)
// =====================================================================

// --- UART Communication (SoftwareSerial) ---
//     On Uno, avoid pins 0/1 (hardware serial used for USB debug).
#define COMM_RX_PIN 2 // SoftwareSerial RX  (connect to High-Level TX)
#define COMM_TX_PIN 3 // SoftwareSerial TX  (connect to High-Level RX)
#define COMM_BAUD_RATE 9600

// --- Stepper Motor (TB6600, common ground wiring) ---
//     Wiring: PUL-/DIR- -> GND, PUL+/DIR+ -> GPIOs
//     Signals are ACTIVE-HIGH (GPIO HIGH triggers optocoupler)
#define STEPPER_STEP_PIN 6          // -> PUL+
#define STEPPER_DIR_PIN 7           // -> DIR+
#define STEPPER_PINS_INVERTED false // common ground = normal logic
#define STEPPER_MAX_SPEED 4000.0    // steps/sec (AccelStepper AVR limit ~4000)
#define STEPPER_ACCELERATION 500.0  // steps/sec^2

// --- Microstepping and rack geometry ---
//     Both live in rack_geometry.h, which is plain C so the slot -> step
//     arithmetic can be tested on a laptop (firmware/tools/test_rack_geometry.c)
//     rather than only on a real rack. Change SLOT_COUNT, MICROSTEP_DIVISOR
//     and the home offset there — this board is the only place in the whole
//     system that knows how many steps a slot is worth.
#include "rack_geometry.h"

// --- Solenoid Relay ---
#define SOLENOID_RELAY_PIN 4 // Digital pin for relay module

// --- Battery Monitor ---
#define VBAT_PIN A0
#define VBAT_DIVIDER_RATIO 2.0 // Adjust for your resistor divider
#define ADC_REF_VOLTAGE 5.0    // Uno uses 5V reference (was 3.3V on ESP)

// =====================================================================
//  PROTOCOL  (was Protocol.h)
// =====================================================================
/*
  Simple ASCII Protocol for High <-> Low Level Communication
  Commands terminated by newline '\n'

  High -> Low Commands:
    SLOT:<n>           Move to key slot <n>, 1-based. This is what the ESP32
                       sends in normal operation; the step maths lives here.
    GOTO:<position>    Move stepper to <position> (in steps)
    ANGLE:<degrees>    Move stepper to <degrees> (converted to steps)
    ACTUATE:<1/0>      Engage(1) or Disengage(0) the solenoid
    BATT:?             Request battery percentage
    STATUS:?           Request system status

  Low -> High Responses:
    ACK:<cmd>          Command acknowledged and started
    DONE:<cmd>         Command finished successfully
    ERR:<msg>          Error occurred
    BATT:<%>           Battery level response
    STATUS:<state>     Status response (IDLE, MOVING, ERROR)
*/

enum SystemState { STATE_IDLE, STATE_MOVING, STATE_ERROR };

// =====================================================================
//  STEPPER CONTROLLER CLASS  (was StepperController.h/.cpp)
// =====================================================================

class StepperController {
public:
  StepperController()
      : motor(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN) {}

  void begin() {
    // TB6600 common ground wiring: signals are active-HIGH
    motor.setPinsInverted(STEPPER_PINS_INVERTED, // direction inverted
                          STEPPER_PINS_INVERTED  // step inverted
    );

    // Configure speed and acceleration
    motor.setMaxSpeed(STEPPER_MAX_SPEED);
    motor.setAcceleration(STEPPER_ACCELERATION);

    // Set minimum pulse width for TB6600 (needs >=2.5us)
    motor.setMinPulseWidth(5);
  }

  void moveTo(long targetPosition) { motor.moveTo(targetPosition); }

  // Move to a target angle (degrees) - converted using STEPS_PER_REV
  void moveToAngle(float degrees) {
    long steps = angleToSteps(degrees);
    motor.moveTo(steps);
  }

  // Move to key slot `slot` (1-based). Absolute, like every other move here.
  void moveToSlot(int slot) { motor.moveTo(slotToSteps(slot)); }

  // The arithmetic itself is in rack_geometry.h so the host test can reach it.
  static long slotToSteps(int slot) { return rack_slot_to_steps(slot); }

  // Convert degrees to step count based on TB6600 microstepping config
  static long angleToSteps(float degrees) {
    return (long)((degrees / 360.0) * STEPS_PER_REV);
  }

  // Convert step count back to degrees
  static float stepsToAngle(long steps) {
    return (steps * 360.0) / STEPS_PER_REV;
  }

  // Must be called in the main loop - drives the AccelStepper state machine
  void update() { motor.run(); }

  // True while the motor is still traveling to the target
  bool isMoving() { return motor.distanceToGo() != 0; }

  // Returns the current step-counted position
  long getPosition() { return motor.currentPosition(); }

  // Returns the current position in degrees
  float getAngle() { return stepsToAngle(motor.currentPosition()); }

private:
  AccelStepper motor;
};

// =====================================================================
//  SOLENOID CONTROLLER CLASS  (was SolenoidController.h/.cpp)
// =====================================================================

class SolenoidController {
public:
  SolenoidController() : isEngaged(false), engagedTime(0) {}

  void begin() {
    pinMode(SOLENOID_RELAY_PIN, OUTPUT);
    digitalWrite(SOLENOID_RELAY_PIN, LOW); // LOW = relay off
  }

  void actuate(bool engage) {
    isEngaged = engage;
    if (engage) {
      digitalWrite(SOLENOID_RELAY_PIN, HIGH);
      engagedTime = millis();
    } else {
      digitalWrite(SOLENOID_RELAY_PIN, LOW);
    }
  }

  // Checks the safety timeout - disengage if held too long
  void update() {
    if (isEngaged && (millis() - engagedTime > maxEngageDuration)) {
      actuate(false); // Safety timeout reached
    }
  }

private:
  bool isEngaged;
  unsigned long engagedTime;
  static const unsigned long maxEngageDuration = 5000; // 5 seconds max
};

// =====================================================================
//  BATTERY MONITOR CLASS  (was BatteryMonitor.h/.cpp)
// =====================================================================

class BatteryMonitor {
public:
  BatteryMonitor() {}

  void begin() { pinMode(VBAT_PIN, INPUT); }

  // Returns the battery voltage (compensated for voltage divider)
  float getVoltage() {
    int raw = analogRead(VBAT_PIN); // 10-bit ADC, 0-1023

    // Uno ADC reference = 5V (ESP8266 was 3.3V)
    float pinVoltage = (raw / 1023.0) * ADC_REF_VOLTAGE;

    // Compensate for external resistor divider
    float vBat = pinVoltage * VBAT_DIVIDER_RATIO;
    return vBat;
  }

  // Returns estimated percentage (0-100) for 3S LiPo (12.6V max, 9.6V min)
  int getPercentage() {
    float voltage = getVoltage();

    float maxV = 12.6;
    float minV = 9.6;

    if (voltage >= maxV)
      return 100;
    if (voltage <= minV)
      return 0;

    int percent = (int)(((voltage - minV) / (maxV - minV)) * 100);
    return percent;
  }
};

// =====================================================================
//  GLOBALS  &  INSTANCES
// =====================================================================

// ---- VERBOSE LOGGING ----
// Controls how chatty the USB serial log is. It does NOT control where
// commands are read from — see loop(), which always listens to both.
//
// This used to be DEBUG_VIA_USB, a flag that *switched* the input source. It
// shipped set to `true`, which meant the sketch read the USB port and never
// commSerial: every command the ESP32 sent was discarded, silently, with no
// error on either side. The dispense path was dead and the only symptom was
// "the ESP isn't talking to the Arduino".
//
// Listening to both ports costs two lines in loop() and removes that entire
// failure mode, so the flag no longer gates anything that can break the link.
#define VERBOSE_LOG true

SoftwareSerial commSerial(COMM_RX_PIN, COMM_TX_PIN);

StepperController stepper;
SolenoidController solenoid;
BatteryMonitor battery;

SystemState currentState = STATE_IDLE;

// Which move command is currently running, so DONE can echo the right verb.
// It used to answer "DONE:GOTO" for every move, including ANGLE ones, which
// makes a serial log impossible to follow when two commands are in flight.
String lastMoveCmd = "GOTO";

// =====================================================================
//  HELPER: Send response to commSerial (and mirror to the USB log)
// =====================================================================

void sendResponse(const String &msg) {
  // print + '\n', NOT println(): println() emits "\r\n" on Arduino, and the
  // stray '\r' arrives on the far end inside the parameter — "IDLE\r" then
  // compares unequal to "IDLE". The ESP32 parser happens to trim it, but the
  // protocol says one '\n' and there is no reason to lean on that mercy.
  commSerial.print(msg);
  commSerial.print('\n');
  if (VERBOSE_LOG) {
    Serial.print("[RESP] ");
    Serial.println(msg);
  }
}

// =====================================================================
//  COMMAND PROCESSOR
// =====================================================================

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0)
    return;

  if (VERBOSE_LOG) {
    Serial.print("[CMD] ");
    Serial.println(cmd);
  }

  int separatorIndex = cmd.indexOf(':');
  String command = cmd;
  String param = "";

  if (separatorIndex != -1) {
    command = cmd.substring(0, separatorIndex);
    param = cmd.substring(separatorIndex + 1);
  }

  if (command == "SLOT") {
    // param.toInt() maps anything non-numeric to 0, so an out-of-range check
    // is also the guard against a garbled line arriving as "go to slot 0".
    long slot = param.toInt();
    if (!rack_slot_valid(slot)) {
      sendResponse("ERR:BAD_SLOT");
      Serial.print("Rejected slot: ");
      Serial.println(param);
      return;
    }
    lastMoveCmd = "SLOT";
    stepper.moveToSlot((int)slot);
    currentState = STATE_MOVING;
    sendResponse("ACK:SLOT");
    Serial.print("Moving to slot ");
    Serial.print(slot);
    Serial.print(" (step ");
    Serial.print(StepperController::slotToSteps((int)slot));
    Serial.println(")");

  } else if (command == "GOTO") {
    long target = param.toInt();
    lastMoveCmd = "GOTO";
    stepper.moveTo(target);
    currentState = STATE_MOVING;
    sendResponse("ACK:GOTO");
    Serial.print("Moving to step: ");
    Serial.println(target);

  } else if (command == "ANGLE") {
    float degrees = param.toFloat();
    lastMoveCmd = "ANGLE";
    stepper.moveToAngle(degrees);
    currentState = STATE_MOVING;
    sendResponse("ACK:ANGLE");
    Serial.print("Moving to angle: ");
    Serial.print(degrees);
    Serial.print(" deg (step ");
    Serial.print(StepperController::angleToSteps(degrees));
    Serial.println(")");

  } else if (command == "ACTUATE") {
    bool engage = (param.toInt() == 1);
    solenoid.actuate(engage);
    sendResponse("ACK:ACTUATE");
    Serial.print("Solenoid Actuated: ");
    Serial.println(engage);

  } else if (command == "BATT") {
    int percent = battery.getPercentage();
    sendResponse("BATT:" + String(percent));

  } else if (command == "STATUS") {
    String stateStr;
    if (currentState == STATE_IDLE)
      stateStr = "IDLE";
    else if (currentState == STATE_MOVING)
      stateStr = "MOVING";
    else
      stateStr = "ERROR";
    sendResponse("STATUS:" + stateStr);

  } else {
    sendResponse("ERR:UNKNOWN_CMD_" + command);
  }
}

// =====================================================================
//  SETUP
// =====================================================================

void setup() {
  // USB debug serial
  Serial.begin(9600);

  // Communication with High-Level MCU
  commSerial.begin(COMM_BAUD_RATE);

  // Initialize hardware controllers
  stepper.begin();
  solenoid.begin();
  battery.begin();

  Serial.println("Low-Level Arduino Uno KMS Controller Initialized.");
  if (VERBOSE_LOG) {
    Serial.println("Listening on BOTH the ESP32 link and this USB monitor.");
    Serial.println("Try: SLOT:7, GOTO:100, ANGLE:90, ACTUATE:1, BATT:?, STATUS:?");
    Serial.print("Slots: ");
    Serial.print(SLOT_COUNT);
    Serial.print("   Steps/rev: ");
    Serial.print(STEPS_PER_REV);
    Serial.print("   Steps/slot: ");
    Serial.println((float)STEPS_PER_REV / SLOT_COUNT);
    // Position is step-counted from wherever the board powered up. Without a
    // home switch there is nothing to check that against, so say so out loud
    // rather than let a post-reset run quietly hand out the wrong key.
    Serial.println("WARNING: no home switch — slot 1 is wherever the rack was at boot.");
  }
}

// =====================================================================
//  LOOP
// =====================================================================

void loop() {
  // 1. Process incoming commands — from BOTH ports, always.
  //
  //    The ESP32 link is the one that matters in service; the USB monitor is
  //    how you bench-test without an ESP32 attached. There is no flag to get
  //    wrong: type "SLOT:7" into the Serial Monitor and it works, and the
  //    ESP32 sending the same line works at the same time.
  if (commSerial.available()) {
    processCommand(commSerial.readStringUntil('\n'));
  }
  if (Serial.available()) {
    processCommand(Serial.readStringUntil('\n'));
  }

  // 2. Update hardware controllers (non-blocking)
  stepper.update();
  solenoid.update();

  // 3. State management - detect when stepper finishes.
  //    AccelStepper's distanceToGo() reaching zero IS the "target reached"
  //    feedback; this is what turns into the dispense confirmation the
  //    website eventually shows.
  if (currentState == STATE_MOVING && !stepper.isMoving()) {
    currentState = STATE_IDLE;
    sendResponse("DONE:" + lastMoveCmd);
    Serial.print("Reached target position: ");
    Serial.println(stepper.getPosition());
  }
}
