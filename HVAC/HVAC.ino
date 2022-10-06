/**
 * @file HVAC.ino
 * @author Jimmy Wang (jwwang_03@outlook.com)
 * @brief HVAC controller code for Arduino Pro Micro
 * @version 0.1
 * @date 2022-10-06
 * 
 * @copyright Copyright (c) 2022
 * 
 * HISTORY:
 *  0.1 - 2022-10-06
 * 
 */

#ifndef __AVR__
typedef std::function<void(void)> custom_callback;
#else
typedef void (*custom_callback)();
#endif // __AVR__

#define SKETCH_NAME "HVAC"
#define SKETCH_VERSION "0.1"

// Enable debug prints
#define MY_DEBUG

#define DEBUGGING

// Radio configuration for mySensors

#define MY_RADIO_RFM69
#define MY_RFM69_FREQUENCY RFM69_433MHZ
#define MY_IS_RFM69HW

// NOTE: All wiring for RFM69 and Arduino Pro mini is default

// W (Wite/Yellow wire) - Heating
// G (Green wire) - Fan
// When heating is triggered, fan turns on automatically, no need to apply voltage to fan separately

#define FAN_RELAY_PIN 9
#define HEAT_RELAY_PIN 8
#define FAN_SENSE_PIN 7
#define HEAT_SENSE_PIN 6

#define CHILD_ID_FAN_STATE 6
#define CHILD_ID_HEAT_STATE 7
#define CHILD_ID_IS_OVERRIDE 8

#include <MySensors.h>
#include "SimpleTimer.h"

void presentation() {
  sendSketchInfo(SKETCH_NAME, SKETCH_VERSION);

  present(CHILD_ID_FAN_STATE, S_BINARY, "HVAC Fan");
  // MyMessage msg(CHILD_ID,V_STATUS);
  present(CHILD_ID_HEAT_STATE, S_BINARY, "HVAC Heat");
  // MyMessage msg(CHILD_ID,V_STATUS);
  present(CHILD_ID_IS_OVERRIDE, S_LIGHT, "HVAC Override");
  // MyMessage msg(CHILD_ID,V_LIGHT);
}

MyMessage MsgFan(CHILD_ID_FAN_STATE, V_STATUS);
MyMessage MsgHeat(CHILD_ID_HEAT_STATE, V_STATUS);
MyMessage MsgOverride(CHILD_ID_IS_OVERRIDE, V_LIGHT);

// Main prorgam starting below
enum State {
  off,
  fan,
  heat
};

State readExternalState();
State readRelayState();
void report(State s);
void setRelay(State s);
void setState(State s);
void receiveCommand(State s, bool b);

State CurrentState = off;
State ExternalState = off;
bool Overridden = false;

State readExternalState() {
  bool fanSense = digitalRead(FAN_SENSE_PIN);
  bool heatSense = digitalRead(HEAT_SENSE_PIN);

  if(fanSense) return fan;
  else if(heatSense) return heat;
  else return off;
}

State readRelayState() {
  // This function should return false when called in setup() under typical circumstances
  bool fanRelay = digitalRead(FAN_RELAY_PIN);
  bool heatRelay = digitalRead(HEAT_RELAY_PIN);

  if(fanRelay) return fan;
  else if(heatRelay) return heat;
  else return off;
}

/**
 * @brief Only called by the handler function!
 * 
 * @param s
 */
void report(State s) {
  if(s == off) {
    send(MsgFan.set(false));
    send(MsgHeat.set(false));
  } else if(s == fan) {
    send(MsgFan.set(true));
    send(MsgHeat.set(false));
  } else if(s == heat) {
    send(MsgFan.set(false));
    send(MsgHeat.set(true));
  }
  send(MsgOverride.set(Overridden));
  return;
}

void setRelay(State s) {
  switch(s) {
    case off:
      digitalWrite(FAN_RELAY_PIN, HIGH);
      digitalWrite(HEAT_RELAY_PIN, HIGH);
      break;
    case heat:
      digitalWrite(FAN_RELAY_PIN, HIGH);
      digitalWrite(HEAT_RELAY_PIN, LOW);
      break;
    case fan:
      digitalWrite(FAN_RELAY_PIN, LOW);
      digitalWrite(HEAT_RELAY_PIN, HIGH);
      break;
    default:
      digitalWrite(FAN_RELAY_PIN, HIGH);
      digitalWrite(HEAT_RELAY_PIN, HIGH);
  }
}

void setState(State s) {
  setRelay(s);
  return;
}

bool healthCheck() {
  bool responce = sendHeartbeat();

  const State resState = Overridden ? ExternalState : CurrentState;

  State externalState = readExternalState();
  State relayState = readRelayState();

  const State resReadState = externalState != off ? externalState : relayState;

  if(resState != resReadState) {
    // This is bad because somehow current state and the relay state is mismatched
    // Try setting current state to relay state
    CurrentState = relayState;
    ExternalState = externalState;
    Overridden = externalState != off;
    report(resReadState);
    return false;
  }
  
  return true;
}

void Handler() {
  if(ExternalState != off) {
    Overridden = true;
  } else {
    Overridden = false;
  }

  const State resState = Overridden ? ExternalState : CurrentState;

  report(resState);

  setState(resState);
}

SimpleTimer timer;

void setup() {
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(HEAT_RELAY_PIN, OUTPUT);

  digitalWrite(FAN_RELAY_PIN, HIGH);
  digitalWrite(HEAT_RELAY_PIN, HIGH);

  pinMode(FAN_SENSE_PIN, INPUT);
  pinMode(HEAT_SENSE_PIN, INPUT);

  bool responce = sendHeartbeat();

  State externalState = readExternalState();
  State relayState = readRelayState();

  CurrentState = relayState;
  ExternalState = externalState;
  Overridden = externalState != off;

  Handler();

  timer.setInterval(75, checkExternalRoutine);
  timer.setInterval(200, healthCheck);
}

void checkExternalRoutine() {
  // called every 75ms
  State now = readExternalState();
  if(now == ExternalState) return;
  else {
    // update state
    ExternalState = now;
    Handler();
  }
}

void receiveCommand(State s, bool b) {
  if(Overridden) return;
  if(b == false) return CurrentState = off;
  
  CurrentState = s;
  Handler();
}

void loop() {
  timer.run();
}

void receive(const MyMessage &msg) {
  // message is coming from the gateway, safety checks, and error "handling"
  if(msg.sender == 0) {
    switch(msg.type) {
      case V_STATUS:
        switch(msg.sensor) {
          case 6:
            receiveCommand(fan, msg.getBool());
            break;
          case 7:
            receiveCommand(heat, msg.getBool());
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
}