#include "Arduino.h"
#include "Ramp.h"


Ramp::Ramp() {}

Ramp::Ramp(int temps[],int rates[],int times[],int stages) {
  _stages = stages;
  _temps = temps;
  _rates = rates;
  _times = times;
}

void Ramp::debug(){
  Serial.println(_stages);
  for (int i = 0; i < 3; i++) {
    Serial.println("Stage " + String(i) + ": goto " + _temps[i] + " at rate " + _rates[i] + " and hold for " + _times[i]);
  }
}

void Ramp::startRamp(int startTemp) {
  setNewTarget(startTemp, 0);
}

int Ramp::getStage() {
  return _stage + 1;
}

int Ramp::getState() {
  return _state;
}

void Ramp::setNewTarget(int startTemp, int stage) {
  _stage = stage;
  _timer = millis();
  _startTemp = startTemp;
  _target = _temps[_stage];
  _rate = _rates[_stage];
  _holdTime = _times[_stage];
  if (_target >= _startTemp) {
    _state = 0;
  } else {
    _state = 1;
  }
}

String Ramp::getStateText() {
  if (_state == 0)
    return "HEAT";
  if (_state == 1)
    return "COOL";
  if (_state == 2)
    return "WAIT";
  if (_state == 3)
    return "HOLD";
}

int Ramp::getTotalStages() {
  return _stages;
}

String Ramp::getTimeRemaining() {
  if (_state == 3) {
    long elapsedTimeMins = (millis() - _timer) / 60000L;
    long remaining = _holdTime - elapsedTimeMins;
    int mins = remaining % 60;
    int hours = remaining / 60;
    return String(hours) + ":" + String(mins);
  } else {
    return "    ";
  }
}

int Ramp::getSetpoint(int temp) {
  if (_target == 0) {//END CASE
    return 0;
  }

  if (_state == 0 || _state == 1) {
    //HEAT or COOL modes

    if (_rate == -1) {//for MAX heat mode, skip to WAIT and direct PID to final Temp.
      _state = 2;
      return _target;
    }

    float ratePerSecond = float(_rate) / 3600.0;
    long elapsedTime = (millis() - _timer) / 1000;

    if (_state == 0) {
      //HEAT Mode
      int setpoint = _startTemp + int(ratePerSecond * float(elapsedTime));
      if (setpoint >= _target) {
        _state = 2;
        return _target;
      }
      return setpoint;
    } else {
      //COOL Mode
      int setpoint = _startTemp - int(ratePerSecond * float(elapsedTime));
      if (setpoint <= _target) {
        _state = 2;
        return _target;
      }
      return setpoint;
    }

  } else if (_state == 2) {
    //state == 2, WAIT
    int absDiff = abs(temp - _target);
    if (absDiff <= 5) { //Temp must be within 10 Degrees
      _state = 3;
      _timer = millis();
    }
    return _target;

  } else {
    //state: 3, HOLD
    long elapsedTimeSeconds = (millis() - _timer) / 1000L;
    long holdTimeSeconds = _holdTime * 60L;
    if (elapsedTimeSeconds >= holdTimeSeconds) {
      setNewTarget(temp, _stage + 1);
      return temp; //will start next itteration, for now hold at temp
    }
    return _target;
  }
}

