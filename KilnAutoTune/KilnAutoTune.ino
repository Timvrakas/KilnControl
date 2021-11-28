#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include "Adafruit_MAX31855.h"

const int THERM_DO = 3; // (data out) is an output from the MAX31855 (input to the microcontroller) which carries each bit of data
const int THERM_CS = 4; // (chip select) is an input to the MAX31855 (output from the microcontroller) which tells the chip when its time to read the thermocouple and output more data.
const int THERM_CLK = 5; // (clock) is an input to the MAX31855 (output from microcontroller) which indicates when to present another bit of data
const int RELAY = 9; // the pin for turning on and off the heating coils

byte ATuneModeRemember=2;
double input=400, output=50, setpoint=400;
double kp=2,ki=5,kd=1;

double aTuneStep=25, aTuneNoise=2, aTuneStartValue=400;
unsigned int aTuneLookBack=20;

boolean tuning = false;
unsigned long serialTime;

const int WindowSize = 5000;
unsigned long windowStartTime;

PID myPID(&input, &output, &setpoint,kp,ki,kd, DIRECT);
PID_ATune aTune(&input, &output);
Adafruit_MAX31855 thermocouple(THERM_CLK, THERM_CS, THERM_DO);


void setup()
{
  pinMode(RELAY, OUTPUT);
  //Setup the pid 
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, WindowSize);
  if(tuning)
  {
    tuning=false;
    changeAutoTune();
    tuning=true;
  }
  
  serialTime = 0;
  Serial.begin(9600);
}

void loop()
{

  unsigned long now = millis();
  input = readTemp(0);
  
  if(tuning)
  {
    byte val = (aTune.Runtime());
    if (val!=0)
    {
      tuning = false;
    }
    if(!tuning)
    { //we're done, set the tuning parameters
      kp = aTune.GetKp();
      ki = aTune.GetKi();
      kd = aTune.GetKd();
      myPID.SetTunings(kp,ki,kd);
      AutoTuneHelper(false);
    }
  }
  else myPID.Compute();
  
  while (now - windowStartTime > WindowSize)
  { //time to shift the Relay Window
    windowStartTime += WindowSize;
  }
  bool relayOn = (output > now - windowStartTime);
  if (relayOn)
    digitalWrite(RELAY, HIGH);
  else
    digitalWrite(RELAY, LOW); 
  
  //send-receive with processing if it's time
  if(millis()>serialTime)
  {
    SerialReceive();
    SerialSend();
    serialTime+=500;
  }
}

double readTemp(int tryCount)
{
  if (tryCount > 10)
    return -1;
  if (tryCount > 2)
  {
    //try resetting the chips
    thermocouple = Adafruit_MAX31855(THERM_CLK, THERM_CS, THERM_DO);
    // wait for MAX chip to stabilize
    delay(1500);
  }
  double temperature = thermocouple.readCelsius();
  if (isnan(temperature))
  {
    delay(3000); //3sec delay
    temperature = readTemp(tryCount + 1);
  }
  return temperature;
}// readTemp


void changeAutoTune()
{
 if(!tuning)
  {
    //Set the output to the desired starting frequency.
    output=aTuneStartValue;
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    aTune.SetControlType(1);//THIS ADDS THE D
    AutoTuneHelper(true);
    tuning = true;
  }
  else
  { //cancel autotune
    aTune.Cancel();
    tuning = false;
    AutoTuneHelper(false);
  }
}

void AutoTuneHelper(boolean start)
{
  if(start)
    ATuneModeRemember = myPID.GetMode();
  else
    myPID.SetMode(ATuneModeRemember);
}


void SerialSend()
{
  Serial.print("setpoint: ");Serial.print(setpoint); Serial.print(" ");
  Serial.print("input: ");Serial.print(input); Serial.print(" ");
  Serial.print("output: ");Serial.print(output); Serial.print(" ");
  if(tuning){
    Serial.println("tuning mode");
  } else {
    Serial.print("kp: ");Serial.print(myPID.GetKp());Serial.print(" ");
    Serial.print("ki: ");Serial.print(myPID.GetKi());Serial.print(" ");
    Serial.print("kd: ");Serial.print(myPID.GetKd());Serial.println();
  }
}

void SerialReceive()
{
  if(Serial.available())
  {
   char b = Serial.read(); 
   Serial.flush(); 
   if((b=='1' && !tuning) || (b!='1' && tuning))changeAutoTune();
  }
}
