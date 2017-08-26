#ifndef Ramp_h
#define Ramp_h
#include "Arduino.h"

class Ramp
{
  public:
    Ramp();
    Ramp(int temps[], int rates[], int times[], int stages);
    int getSetpoint(int temp);
    void startRamp(int startTemp);
    int getStage();
    int getState();
    String getStateText();
    int getTotalStages();
    String getTimeRemaining();
  private:
    int* _temps;
    int* _rates;
    int* _times;
    int _stages;
    int _target;
    int _rate;
    int _holdTime;
    unsigned long _timer;
    int _stage;
    int _state; //0:HEAT 1:COOL 2:WAIT 3:HOLD
    int _startTemp;
    void setNewTarget(int temp, int stage);
};

#endif
