#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "SpeedStepper.h"
#include "config.h"

// ─── UDP ─────────────────────────────────────────────────────────────────────
WiFiUDP   udp;
IPAddress g_streamTarget;          // set on WiFi connect, destination for the position stream

// ─── Stepper ─────────────────────────────────────────────────────────────────
SpeedStepper m1Pan(M1_STEP_PIN, M1_DIR_PIN);
SpeedStepper m1Tilt(M2_STEP_PIN, M2_DIR_PIN);
SpeedStepper m2Pan(M3_STEP_PIN, M3_DIR_PIN);
SpeedStepper m2Tilt(M4_STEP_PIN, M4_DIR_PIN);

// ─── Shared state (single 32-words: aligned reads/writes are atomic) ─────────

/// these values are short enough that they get written too fast to be read mid-writing making it
/// safe for multicore applications; int32_t is 4byte chunk; 8bits=1byte 4byte=32bits so cpu
/// can read in one go, and it doesn't get read in pieces and because it's 4bytes it is aligned
/// in memory so it is good to put values at the start of chunks
volatile int32_t g_speed[4]  = {0, 0, 0, 0};
volatile int32_t g_target[4] = {0, 0, 0, 0};
volatile bool g_homed        = false;
volatile uint8_t g_mode      = MODE_TRACK;
volatile uint32_t g_lastSpeedMs = 0;

// ─────────────────────────────────────────────────────────────────────────────

/// static so no other .cpp file can interfere with this endstopHit functions. I can have multiple
/// and compiler won't complain
/// and inline because just doing digitalRead() with all the steps going on when doing a
/// function call is pointless and inline just does what writing manually in loop digialRead
/// would do
static inline bool endstopHit(int pin) {
  return digitalRead(pin) == HIGH;
}

/// this converts the value to absolute value -v = v, v = v, could've used abs() but this
/// prevents running things twice
static inline int32_t iabs32(int32_t v) {
  return v < 0 ? -v : v;
}

///multiple axis homing
enum motor_State { H_APPROACH, H_BACKOFF, H_DONE, H_PARK, T_MOVEMENT, TEST_MODE};
/// Pamas: peedStepper* st; int endstopPin;int32_t rangeSteps;int homeDir;
///float homeSpeed;float homeAccel;float moveSpeed; float moveAccel;HomePhase phase;int32_t backoffStart;
struct motorState {
  bool        homed;
  motor_State state;
};
struct motorConfig {
  SpeedStepper* st;
  int       endstopPin;
  int32_t   rangeSteps;
  int       dir;
  float     homeSpeed;
  float     homeAccel;
  float     moveSpeed;
  float     moveAccel;
  int32_t   endstopClearance;
  int8_t    deadband;
  float_t   Kp;
  float     parkDeg;   // where to sit after homing, degrees from home. Does NOT re-zero.

};
struct Motor {
  const motorConfig cfg;
  motorState        state;
};

Motor motors[] = {
  {
    {&m1Pan,
      M1_ENDSTOP_PIN,
      M1_RANGE_STEPS,
      DIR_M1,
      M1_HOMING_SPEED,
      M1_HOMING_ACCEL,
      M1_MOVE_SPEED,
      M1_MOVE_ACCEL,
      ENDSTOP_CLEARANCE,
      DEADBAND,
      M1_KP,
      M1_PARK_DEG},
   {false, H_APPROACH}},
  {
    {&m1Tilt,
      M2_ENDSTOP_PIN,
      M2_RANGE_STEPS,
      DIR_M2,
      M2_HOMING_SPEED,
      M2_HOMING_ACCEL,
      M2_MOVE_SPEED,
      M2_MOVE_ACCEL,
      ENDSTOP_CLEARANCE,
      DEADBAND,
      M2_KP,
      M2_PARK_DEG },
  {false, H_APPROACH}},
  {
    {
      &m2Pan,
      M3_ENDSTOP_PIN,
      M3_RANGE_STEPS,
      DIR_M3,
      M3_HOMING_SPEED,
      M3_HOMING_ACCEL,
      M3_MOVE_SPEED,
      M3_MOVE_ACCEL,
      ENDSTOP_CLEARANCE,
      DEADBAND,
      M3_KP,
      M3_PARK_DEG},
    {false, H_APPROACH}},
  {
      {
        &m2Tilt,
        M4_ENDSTOP_PIN,
        M4_RANGE_STEPS,
        DIR_M4,
        M4_HOMING_SPEED,
        M4_HOMING_ACCEL,
        M4_MOVE_SPEED,
        M4_MOVE_ACCEL,
        ENDSTOP_CLEARANCE,
        DEADBAND,
        M4_KP,
        M4_PARK_DEG
      },
    {false, H_APPROACH}}
};
static const size_t MOTOR_COUNT = sizeof(motors) / sizeof(motors[0]);
size_t homedCount = 0;
// Separate from homedCount: an axis is HOMED once it has found its endstop and backed
// off, but it is not READY until it has also parked facing forward. g_homed reports the
// second, because the PC gates triangulation on it and step counts taken mid-park are
// meaningless.
size_t parkedCount = 0;

void motorEnter(Motor& m, motor_State s) {
  m.state.state = s;

  switch (s) {
    case H_APPROACH:
      Serial.println(C_CYN "[homing] both axes homed - H_APPROACH state" C_RST);
      m.cfg.st->setMaxSpeed(m.cfg.homeSpeed);
      m.cfg.st->setAcceleration(m.cfg.homeAccel);
      m.cfg.st->setMinusLimit(-SpeedStepper::MAX_INT32_T);
      m.cfg.st->setPlusLimit(SpeedStepper::MAX_INT32_T);
      m.cfg.st->setSpeed(m.cfg.dir * m.cfg.homeSpeed);
      break;

      case H_BACKOFF:
      Serial.println(C_CYN "H_BACKOFF state" C_RST);
      m.cfg.st->setSpeed(-m.cfg.dir * m.cfg.homeSpeed * 0.5f);
      break;

      case H_DONE:
      Serial.println(C_CYN "H_DONE state" C_RST);
      m.cfg.st->stop();
      m.cfg.st->setPlusLimit(m.cfg.rangeSteps);
      m.cfg.st->setMaxSpeed(m.cfg.moveSpeed);
      m.cfg.st->setAcceleration(m.cfg.moveAccel);
      break;

      case H_PARK:
      // Speed and acceleration were already switched to the move profile in H_DONE,
      // and positionControl supplies the motion, so there is nothing to set up here.
      Serial.println(C_CYN "H_PARK state - driving to forward-facing park" C_RST);
      break;

      case T_MOVEMENT:
      Serial.println(C_CYN "T_MOVEMENT STATE" C_RST);
      break;

      case TEST_MODE:
      Serial.println(C_CYN "TEST_MODE state" C_RST);
      break;
      
  }
}

void positionControl(Motor& m, int32_t targetDegrees) {
  int32_t targetSteps = lroundf(targetDegrees * stepsPerDegree);
  targetSteps = constrain(targetSteps, 0, m.cfg.rangeSteps);

  int32_t error = targetSteps - m.cfg.st->getCurrentPosition();   // +ve ⇒ target is ahead

  if (iabs32(error) <= m.cfg.deadband) {        // close enough → hold
    m.cfg.st->setSpeed(0);
    return;
  }
  // proportional: speed shrinks as you approach, clamped to the move ceiling
  m.cfg.st->setSpeed(constrain(m.cfg.Kp * error, -m.cfg.moveSpeed, m.cfg.moveSpeed));

}

void motorRun(Motor& m, int32_t target, int32_t speedDegS) {
  if (m.state.state == T_MOVEMENT || m.state.state == TEST_MODE) {
    motor_State wanted = (g_mode == MODE_TEST) ? TEST_MODE : T_MOVEMENT;
    if (m.state.state != wanted) {
      motorEnter(m, wanted);
    }
  }
    switch (m.state.state) {
      case H_APPROACH:
        if (endstopHit(m.cfg.endstopPin) == true) {
          m.state.homed = true;
          m.cfg.st->stopAndSetHome();
          m.cfg.st->setMinusLimit(m.cfg.st->getCurrentPosition());
          m.state.state = H_BACKOFF;
          motorEnter(m, H_BACKOFF);
        }
        break;

        case H_BACKOFF: {
          int32_t backoffCurPos = m.cfg.st->getCurrentPosition();
          static uint32_t t = 0;
          if (millis() - t > 200) { t = millis();
            Serial.printf(C_RED "[backoff] pos=%ld  clearance=%ld  diff=%ld\n" C_RST,
                         static_cast<long>(backoffCurPos), static_cast<long>(m.cfg.endstopClearance),
                          static_cast<long>(backoffCurPos - m.cfg.endstopClearance) );
          }

          if (backoffCurPos - m.cfg.endstopClearance > 0) {
            m.cfg.st->stopAndSetHome();
            motorEnter(m, H_DONE);
            homedCount++;
            Serial.println("H_BACKOFF state completed");
          }
        }

        break;

        case H_DONE:
          // Barrier: nobody parks until every axis has found its endstop, so the heads
          // never swing while a neighbour is still hunting.
          if (homedCount == MOTOR_COUNT) {
            motorEnter(m, H_PARK);
          }
        break;

        case H_PARK: {
          // Position 0 stays where the backoff left it. This drives PAST it to where the
          // camera actually looks forward and deliberately does NOT re-zero - so the
          // parked step count is a real angle from home, and that number is the PC's
          // zeroSteps for this axis.
          int32_t parkDegI = static_cast<int32_t>(m.cfg.parkDeg);
          positionControl(m, parkDegI);
          // Mirror positionControl's own clamp exactly. If PARK_DEG is ever set past the
          // axis range, the motor stops at rangeSteps while an unclamped target would sit
          // beyond it - the deadband check would never pass, H_PARK would never exit, and
          // g_homed would stay false forever with no error anywhere.
          int32_t parkSteps = lroundf(parkDegI * stepsPerDegree);
          parkSteps = constrain(parkSteps, 0, m.cfg.rangeSteps);
          // Position alone is NOT enough. positionControl commands speed = Kp * error,
          // which needs a deceleration of 9*error to follow - about 900 steps/s^2 at
          // error 100, against a moveAccel of 500. The axis therefore reaches the
          // deadband still moving fast; exiting here would hand it to T_MOVEMENT, which
          // sets speed 0 and lets it COAST tens of steps past the target. Waiting for
          // it to actually stop is what makes the parked count repeatable - and that
          // count is the PC's starting value for zeroSteps.
          bool atRest = fabsf(m.cfg.st->getSpeed()) < 1.0f;
          if (atRest && iabs32(parkSteps - m.cfg.st->getCurrentPosition()) <= m.cfg.deadband) {
            parkedCount++;
            Serial.printf(C_GRN "[park] axis parked at %ld steps (%.2f deg)\n" C_RST,
                          static_cast<long>(m.cfg.st->getCurrentPosition()),
                          m.cfg.st->getCurrentPosition() / stepsPerDegree);
            motorEnter(m, T_MOVEMENT);
            if (parkedCount == MOTOR_COUNT) {
              g_homed = true;
              Serial.println(C_GRN "[park] all axes parked - g_homed = true" C_RST);
            }
          }
        }
        break;

      case T_MOVEMENT: {
        // Velocity command is a STANDING order: if the PC dies, drops WiFi, or is closed,
        // the last speed would run forever. Treat a stale command as zero.
        int32_t cmd = speedDegS;
        if (millis() - g_lastSpeedMs > SPEED_TIMEOUT_MS) {
          cmd = 0;
        }
        // deg/s -> steps/s, clamped to this axis' move ceiling.
        // No cfg.dir here: direction comes from the sign of the command, exactly as
        // positionControl takes it from the sign of the error.
        float stepsPerSec = cmd * stepsPerDegree;
        m.cfg.st->setSpeed(constrain(stepsPerSec, - m.cfg.moveSpeed, m.cfg.moveSpeed));
      }
        break;

    case TEST_MODE:
        positionControl(m, target);
        break;

        default:
        break;
  }
}
/// void* pv is a pointer to an adress there's no type being pointed to but to use it u need to cast it
/// and it is needed as a parameter for FreeRTOS
void stepperTask(void* pv) {
  Serial.println("[stepperTask] start");
  for (size_t i = 0; i < MOTOR_COUNT; ++i) {
    pinMode(motors[i].cfg.endstopPin, INPUT_PULLUP);
  }
  
  for (size_t i = 0; i < MOTOR_COUNT; i++) {
    motorEnter(motors[i], H_APPROACH);
  }


  g_target[0] = 0;
  g_target[1] = 0;
  g_target[2] = 0;
  g_target[3] = 0;
  uint32_t lastDbg  = millis();

  for (;;) {
    for (size_t i = 0; i < MOTOR_COUNT; ++i) {
      motorRun(motors[i], g_target[i], g_speed[i]);
    }

    static uint32_t lastSpd = millis();
    if (millis() - lastSpd > 200) {
      lastSpd = millis();
      Serial.print(C_GRN "[speed] (");
      for (size_t i = 0; i < MOTOR_COUNT; ++i) {
        if (i > 0) {
          Serial.print(',');
        }
        Serial.print(motors[i].cfg.st->getSpeed());
      }
      Serial.println(")" C_RST);
    }

    static uint32_t lastDir = millis();
    if (millis() - lastDir > 500) {
      lastDir = millis();
      Serial.print("[dir] (");
      for (size_t i = 0; i < MOTOR_COUNT; ++i) {
        if (i > 0) {
          Serial.print(',');
        }
        Serial.print(motors[i].cfg.st->isDirForward());
      }
      Serial.println(")");
    }

    for (size_t i = 0; i < MOTOR_COUNT; ++i) {
      motors[i].cfg.st->run();
    }

    // rate-limited tracking diagnostic: shows what trackAxis actually sees.
    if (millis() - lastDbg > 500) {
      lastDbg = millis();
      Serial.print("[track] cmd=(");
      for (size_t i = 0; i < MOTOR_COUNT; ++i) {
        if (i > 0) {
          Serial.print(',');
        }
        Serial.print(static_cast<long>(g_speed[i]));
      }
      Serial.print(") pos=(");
      for (size_t i = 0; i < MOTOR_COUNT; ++i) {
        if (i > 0) {
          Serial.print(',');
        }
        Serial.print(static_cast<long>(motors[i].cfg.st->getCurrentPosition()));
      }
      Serial.println(")");
    }
  }
}

// ─── Core 0: WiFi + UDP target ingest ────────────────────────────────────────

struct WiFiCred {const char* ssid; const char* password;const char* remoteIp;};
static const WiFiCred WIFI_NETWORKS[] = {
  { ssid,   password, remoteIp},
  { ssid2,  password2, remoteIp2},
};
static const size_t WIFI_NETWORKS_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

bool connectWiFi() {
  for (size_t i = 0; i < WIFI_NETWORKS_COUNT; i++) {
    Serial.print("Attempting to connect to the WiFi ");
    WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_TIMEOUT_MS) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (WiFi.status() == WL_CONNECTED) {
      g_streamTarget.fromString(WIFI_NETWORKS[i].remoteIp);
      Serial.println("WiFi connected: ");
      Serial.print(WIFI_NETWORKS[i].ssid);
      Serial.println(" IP address: ");
      Serial.println(WiFi.localIP());
      WiFi.setSleep(false);
      return true;
    }
    WiFi.disconnect();
  }
  return false;
}

void netTask(void* pv) {
  Serial.println("\n[netTask] start");
  WiFi.mode(WIFI_STA);
  while (!connectWiFi()) {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  udp.begin(UDP_PORT);

  uint8_t   buf[64];
  bool      streaming = false;     // turns true once we get the "Connected" handshake
  uint32_t  lastStream = millis();
  IPAddress peerIp;                // sender of the handshake; where we stream + ack to
  uint16_t  peerPort = 0;

  for (;;) {
    // if the link drops, refind one and stop streaming until re-handshaked
    if (WiFi.status() != WL_CONNECTED) {
      udp.stop();
      streaming = false;
      while (!connectWiFi()) {
        vTaskDelay(pdMS_TO_TICKS(2000));
      }
      udp.begin(UDP_PORT);
    }

    int sz = udp.parsePacket();
    if (sz > 0) {
      int n = udp.read(buf, sizeof(buf));
      if (n == 9 && memcmp(buf, "Connected", 9) == 0) { //memory compare if Connected is in buf and it has a size of 9 and returns an int
        // handshake: remember the sender, ack with "Connected", start streaming back to it
        peerIp     = udp.remoteIP();
        peerPort   = udp.remotePort();
        streaming  = true;
        lastStream = millis();
        udp.beginPacket(peerIp, peerPort);
        udp.write(reinterpret_cast<const uint8_t*>("Connected"), 9);
        udp.endPacket();
        Serial.print("[net] handshake from ");
        Serial.print(peerIp);
        Serial.print(":");
        Serial.print(peerPort);
        Serial.println(" -> acked, streaming on");
        vTaskDelay(pdMS_TO_TICKS(100));
      } else if (n >= 1) {
        switch (buf[0]) {
          case MSG_TARGET:
            if (n >= 9) {
              int32_t pan, tilt;
              memcpy(&pan, buf + 1, 4);
              memcpy(&tilt, buf + 5, 4);
              g_target[0] = pan;
              g_target[1] = tilt;
            }
            break;

          case MSG_MODE:
            if (n >= 2) {
              g_mode = buf[1];
            }
            break;

          case MSG_SPEED: {
            if (n >= 2) {
              uint8_t count = buf[1];
              if (count > MOTOR_COUNT) {
                count = MOTOR_COUNT;
              }
              if (n >= 2 + count * 4) {
                for (uint8_t i = 0; i < count; ++i) {
                  int32_t v;
                  memcpy(&v, buf + 2 + i * 4, 4);
                  g_speed[i] = v;
                }
                g_lastSpeedMs = millis();
              }
            }
            break;
          }

        }
      }
    }

    // stream current positions + homed flag at UDP_STREAM_RATE_MS (50 Hz)
    if (streaming && (millis() - lastStream) >= UDP_STREAM_RATE_MS) {
      lastStream += UDP_STREAM_RATE_MS;
      int32_t positions[MOTOR_COUNT];
      for (size_t i = 0; i < MOTOR_COUNT; ++i) {
        positions[i] = motors[i].cfg.st->getCurrentPosition();
      }
      const uint8_t count = MOTOR_COUNT;

      uint8_t out[2 + 4 * 8 + 1];
      out[0] = MSG_MOTOR_POS;
      out[1] = count;
      for (uint8_t i = 0; i < count; i++) {
        memcpy(out + 2 + i * 4, &positions[i], 4);
      }
      uint8_t homeIndex = 2 + count * 4;
      out[homeIndex] = g_homed ? 1 : 0;

      uint8_t packetLen = homeIndex + 1;


      udp.beginPacket(peerIp, peerPort);
      udp.write(out, packetLen);
      udp.endPacket();
    }

    vTaskDelay(1);
  }

}


// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);                       // let the USB/UART settle before first print
  Serial.println("\n[boot] setup() running");

  xTaskCreatePinnedToCore(netTask,     "net",     8192, NULL, 2, NULL, 0);
  Serial.print("\n[boot]NetTask created");
  xTaskCreatePinnedToCore(stepperTask, "stepper", 4096, NULL, 2, NULL, 1);
  Serial.println("\n[boot]StepperTask created");
  Serial.println("[boot] tasks created");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
vTaskDelay(pdMS_TO_TICKS(1000));
}