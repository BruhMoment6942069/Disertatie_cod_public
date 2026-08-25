// speedStepperSetup.ino

// install SafeString library using the Arduino library manager for millisDelay, also see https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html
#include <millisDelay.h>
// the SpeedStepper library is available from https://www.forward.com.au/pfod/Robots/SpeedStepper/index.html
#include <SpeedStepper.h>
// these next two includes come from the pfodParser library V3.36+.
// install pfodParser library using the Arduino library manager for pfodNonBlockingInput and pfodBufferedStream, also see https://www.forward.com.au/pfod/pfodParserLibraries/index.html
#include <pfodNonBlockingInput.h>
#include <pfodBufferedStream.h>
#include "setupMenus.h"

/*
   (c)2019 Forward Computing and Control Pty. Ltd.
   NSW Australia, www.forward.com.au
   This code is not warranted to be fit for any purpose. You may only use it at your own risk.
   This generated code may be freely used for both private and commercial use
   provided this copyright is maintained.
*/

// speedStepper_setup.ino
// this sketch allows you to set the home position and limits
// prior to your run

// Uno      LATENCY: stepper:968us loop:6044us
// Mega2560 LATENCY: stepper:964us loop:6136us
//
// but LATENCY: stepper ~10ms for SparkFun Redboard Turbo on SerialUSB
//     LATENCY: stepper ~350us for SparkFun Redboard Turbo on Serial1
//
// SparkFun Redboard Turbo (Serial1) LATENCY: stepper:345us loop:2016us
//
// Max speed on SparkFun Redboard Turbo is 990/sec with a setting of 1000/sec

// ========== SERIAL setting ================
// change this to match your board,  UNO/Mega use Serial, Redboard Turbo USB (slow) use SerialUSB, Redboard Turbo D0/D1 (fast) use Serial1
#define SERIAL_OUT Serial
const uint32_t SERIAL_OUT_BAUD_RATE = 115200;

// for position output every 2sec #define DEBUG
#define DEBUG

// change these pin definitions to match you motor driver
const int STEP_PIN = 6;
const int DIR_PIN = 7;

// change this to limit max run speed
// when setting position and limits max speed is set by the MAX_SETUP_SPEED const in setupMenus.cpp
const int MAX_RUN_SPEED = 100;

void runStepper(Stream* io);


SpeedStepper stepper(STEP_PIN, DIR_PIN);
pfodNonBlockingInput nonBlocking;

millisDelay printDelay;
uint32_t PRINT_DELAY_MS = 2000; // ms
Stream* serialIO;

const size_t OUTPUT_BUFFER_SIZE = 360;
uint8_t outputBuffer[OUTPUT_BUFFER_SIZE];
pfodBufferedStream bufferedStream(SERIAL_OUT_BAUD_RATE, outputBuffer, OUTPUT_BUFFER_SIZE, false); // do not block just drop chars if buffere full

// timing latency for runStepper calls
uint32_t maxDelayCallingRunStepper = 0;
uint32_t runStepper_us = 0;

// timing latency for calls to loop() i.e. how long between loop() runs
uint32_t maxLoopTime = 0;
uint32_t lastLoopTime_us = 0;

void printPositionAndLimits(Stream* io) {
  runStepper(io);
  io->print(F(" pos:"));
  io->print(stepper.getCurrentPosition());
  runStepper(io);
  io->print(F(" sp:"));
  io->print(stepper.getSpeed());
  runStepper(io);
  io->print(F(" +Lim:"));
  io->print(stepper.getPlusLimit());
  runStepper(io);
  io->print(F(" -Lim:"));
  io->print(stepper.getMinusLimit());
  runStepper(io);
  io->print(F(" LATENCY:"));
  io->print(F(" stepper:"));
  io->print(maxDelayCallingRunStepper);
  io->print(F("us"));
  runStepper(io);
  io->print(F(" loop:"));
  io->print(maxLoopTime);
  io->print(F("us"));
  runStepper(io);
  io->println();
}

bool stepperWasRunning = true; // force display of initial menu

/**
   prints out postion if in DEBUG
*/
void runStepper(Stream* io) {
  if (printDelay.justFinished()) {
    printDelay.repeat();
    if (stepper.isRunning() || stepperWasRunning) {
#ifdef DEBUG
      printPositionAndLimits(io);
#endif
    }
    if (stepper.isRunning()) {
      stepperWasRunning = true; // print first time after stop
    } else {
      stepperWasRunning = false;
    }
  }
  uint32_t us = micros();
  if (runStepper_us != 0) {
    // not first time
    uint32_t d_us = us - runStepper_us;
    if (d_us > maxDelayCallingRunStepper) {
      maxDelayCallingRunStepper = d_us;
    }
  }
  runStepper_us = us;
  stepper.run();
}

bool displayRunMenuFlag = false;
bool running = false;

//extern void setRunning();
void setRunning() {
  running = true;
  displayRunMenuFlag = true;
}

void displayRunMenu(Stream* io) {
  io->println();
  io->println(F(" RUN MENU"));
  printPositionAndLimits(io);
  io->println(F(" + -- Speed up"));
  io->println(F(" - -- Speed down"));
  runStepper(io);
  io->println(F(" h -- goHome"));
  io->println(F(" . -- hardStop"));
  runStepper(io);
  io->println(F(" <else> -- freeze Speed"));
  io->print('>');
  runStepper(io);
}

void processRunCmd(int c_in, Stream* io) {
  // display menu when stepper stops
  if (c_in == -1) {
    if ((!stepper.isRunning()) && stepperWasRunning) {
      stepperWasRunning = false;
      displayRunMenuFlag = true;
    }
    return;
  }
  char c = (char)c_in;
  if ((c == '\n') || (c == '\r') ) {
    // got empty line just stop
    serialIO->println();
  } else {
    // echo char
    serialIO->println(c);
  }
  if (c == '+') {
    if (stepper.isRunning() && (stepper.getSpeed() < 0)) {
      stepper.setSpeed(0); // not as slow but do not reverse
    } else {
      stepper.setSpeed(MAX_RUN_SPEED); // speed up
    }
    displayRunMenuFlag = true; // display menu again
  } else if (c == '-') {
    if (stepper.isRunning() && (stepper.getSpeed() > 0)) {
      stepper.setSpeed(0);
    } else {
      stepper.setSpeed(-MAX_RUN_SPEED);
    }
    displayRunMenuFlag = true; // display menu again
  } else if (c == 'h') {
    stepper.goHome();
    displayRunMenuFlag = true; // display menu again
  } else if (c == '.') {
    stepper.hardStop();
    stepperWasRunning = false;
    displayRunMenuFlag = true; // display menu again
  } else {
    stepper.setSpeed(stepper.getSpeed());
    displayRunMenuFlag = true; // display menu again
  }
}

uint32_t start_ms;

void setup() {

  // change the #define SERIAL_OUT at the top of this sketch to use other Serial connections
  SERIAL_OUT.begin(SERIAL_OUT_BAUD_RATE);
  // ------------------------------
  for (int i = 10; i > 0; i--) {
    SERIAL_OUT.println(i);
    delay(500);
  }
  serialIO = nonBlocking.connect(bufferedStream.connect(&SERIAL_OUT)); // handle outputing menus buffered and buffered input

  stepper.setDebugPrint(serialIO); // also need to uncomment #define DEBUG in SpeedStepper.cpp
  printDelay.start(PRINT_DELAY_MS);

  serialIO->println(F("speedStepper_setup test!"));
  stepper.setPlusLimit(500000); // will used these unless changed in setup
  stepper.setMinusLimit(-500); // will used these unless changed in setup
  stepper.setMaxSpeed(1000); // the max speed that can be set
  stepper.setMinSpeed(1);
  stepper.setAcceleration(10);

  start_ms = millis();
  //setRunning();
}

void loop() {
  {
    uint32_t us = micros();
    if (lastLoopTime_us != 0) {
      // not first time
      uint32_t d_us = us - lastLoopTime_us;
      if (d_us > maxLoopTime) {
        maxLoopTime = d_us;
      }
    }
    lastLoopTime_us = us;
  }

  runStepper(serialIO);

  if (displayRunMenuFlag) {
    displayRunMenuFlag = false;
    displayRunMenu(serialIO);
  }

  int c = serialIO->read();
  if (c != -1) {
    nonBlocking.clearInput(); // clear the rest
  }

  if (!running) {
    processChar(c, serialIO); // handle menu for this char
    return;
  }
  // else normal running
  processRunCmd(c, serialIO);
}
