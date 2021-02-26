/*
 * Nema17.h
 *
 *  Created on: 23 Feb 2021
 *      Author: jp112sdl
 */

#ifndef NEMA17_H_
#define NEMA17_H_

#include <A4988.h> //https://github.com/laurb9/StepperDriver

namespace as {

class Nema17 {
private:
  A4988 stepper;
  int32_t totalSteps;
  int32_t numSteps;
  bool newstart, fromLeftStopper;
public:
  Nema17() : stepper(MOTOR_STEPS, A4988_DIR, A4988_STEP, A4988_SLEEP, A4988_MS1, A4988_MS2, A4988_MS3),  totalSteps(0), numSteps(0), newstart(false), fromLeftStopper(false) { }
  ~Nema17() {}

  void init() {
    pinMode(A4988_STOPPER_RIGHT, INPUT_PULLUP);
    pinMode(A4988_STOPPER_LEFT, INPUT_PULLUP);
    pinMode(A4988_ENABLE, OUTPUT);
    digitalWrite(A4988_ENABLE, LOW);

    stepper.begin(RPM, MICROSTEPS);
  }

  void drivePct(uint8_t pct) {
    fromLeftStopper = true;
    newstart = true;
    stepper.enable();
    stepper.setSpeedProfile(stepper.LINEAR_SPEED, MOTOR_ACCEL, MOTOR_DECEL);
    numSteps = (totalSteps * pct) / 100UL;
  }

  void setSteps(uint32_t s) {
    totalSteps = s;
  }

  uint32_t calibrate() {
      stepper.enable();

      unsigned long start_ms = millis();
      while (digitalRead(A4988_STOPPER_LEFT) == LOW) {
        stepper.move(1);
        if (millis() - start_ms > 5000) {
          DPRINTLN(F("TIMEOUT"));
          return 0xFFFFFFFF;
        }
      }

      start_ms = millis();
      while (digitalRead(A4988_STOPPER_LEFT) == HIGH) {
        stepper.move(-1);
        if (millis() - start_ms > 5000) {
          DPRINTLN(F("TIMEOUT"));
          return 0xFFFFFFFF;
        }
      }

      start_ms = millis();
      totalSteps = 0;
      while (digitalRead(A4988_STOPPER_RIGHT) == HIGH) {
        stepper.move(1);
        totalSteps++;
        if (millis() - start_ms > 5000) {
          DPRINTLN(F("TIMEOUT"));
          return 0xFFFFFFFF;
        }
      }

      stepper.move(totalSteps * -1);

      stepper.disable();

      DPRINT(F("calibration done. steps = "));DDECLN(totalSteps);
      return totalSteps;
  }

  bool run(bool dis) {
    if (fromLeftStopper == true && newstart == true) {
      stepper.startMove(totalSteps * -1);
      newstart = false;
    }

    if (digitalRead(A4988_STOPPER_LEFT) == LOW){
      if (fromLeftStopper == true) {
        fromLeftStopper = false;
        stepper.startMove(numSteps);
      }
    }

    if ((unsigned)stepper.nextAction() <= 0) { if (dis) stepper.disable(); return false;}

    return true;
  }
};
}


#endif /* NEMA17_H_ */
