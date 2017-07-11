#include <EEPROM.h>
#include <SPI.h>
#include <LiquidCrystal.h>
#include "Adafruit_MAX31855.h"
#include <PID_v1.h>
#include <SD.h>
#include "Ramp.h"

const int RELAY = 9; // the pin for turning on and off the heating coils

const int SD_CS = 8; // sd card

const int THERM_DO = 3; // (data out) is an output from the MAX31855 (input to the microcontroller) which carries each bit of data
const int THERM_CS = 4; // (chip select) is an input to the MAX31855 (output from the microcontroller) which tells the chip when its time to read the thermocouple and output more data.
const int THERM_CLK = 5; // (clock) is an input to the MAX31855 (output from microcontroller) which indicates when to present another bit of data

//Define Variables we'll be connecting to
double Setpoint, Input, Output;

//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint, 2, 5, 1, DIRECT);

const int WindowSize = 5000;
unsigned long windowStartTime;

Adafruit_MAX31855 thermocouple(THERM_CLK, THERM_CS, THERM_DO);
LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);

/*
   Heating Stages: For each coresponding entry in the array table set:
    Target Temp in Degrees Centigrade (0 Ends Program)
    Rate of heating Degrees Centigrade per Hour (-1 is Max Heat)
    Hold time in Mins
*/
const int stageTemps[] = {200, 700, 0};
const int stageRates[] = {200,  -1, 0};
const int stageTimes[] = {0,   120, 0};
const int stages = 3;

Ramp myRamp(stageTemps, stageRates, stageTimes, stages);

bool sdCardInited = false;
unsigned long systimer;
String filename;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Kilnerator 9000");
  Serial.println("Kilnerator 9000");
  pinMode(RELAY, OUTPUT);
  delay(1500);


  /**********SD SETUP**********/
  int sdIndex = EEPROM.read(0);
  sdIndex++;
  EEPROM.write(0, sdIndex);
  filename = String(sdIndex)+".csv";
  lcd.setCursor(0, 0);
  lcd.print("SD Initializing ");
  Serial.println("SD Initializing");
  pinMode(SD_CS, OUTPUT);
  if (!SD.begin(SD_CS)) {
    lcd.setCursor(0, 1);
    lcd.print("Card Not Found  ");
    Serial.println("Card failed, or not present");
  }
  else
  {
    lcd.setCursor(0, 1);
    lcd.print("Log: ");
    lcd.setCursor(5,1);
    lcd.print(filename);
    Serial.println("Log: "+filename);
    delay(3000);
    sdCardInited = true;
    SD.remove(filename);
    writeToSD("setup,lcd");
    writeToSD("setup,sd card");
    Serial.println("Card Initialized");
  }
  lcd.setCursor(0, 1);
  lcd.print("                ");

  /********** Starting Heat Cycle **********/
  //initialize the variables we're linked to
  lcd.setCursor(0, 0);
  lcd.print("Starting Cycle ");
  myRamp.startRamp(readTemp(0));
  windowStartTime = millis();
  Setpoint = myRamp.getSetpoint(readTemp(0));
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, WindowSize);
  //turn the PID on
  myPID.SetMode(AUTOMATIC);
  writeToSD("setup,pid");
  writeToSD("1 ,2 ,temperature,ambientTemp,thermocouple");
  systimer = millis();
  delay(1500);

}//setup

void loop() {
  /*******CALCULATE SETPOINT**********/
  double temperature = readTemp(0);
  Input = temperature;
  Setpoint = myRamp.getSetpoint(temperature);
  myPID.Compute();


  /*******CONTROL KILN RELAY**********/
  unsigned long now = millis();
  while (now - windowStartTime > WindowSize)
  { //time to shift the Relay Window
    windowStartTime += WindowSize;
  }
  bool relayOn = (Output > now - windowStartTime);
  if (relayOn)
    digitalWrite(RELAY, HIGH);
  else
    digitalWrite(RELAY, LOW);



  /*********UPDATE DISPLAY***********/
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  String relayState;
  if (relayOn)
    relayState = "ON  ";
  else
    relayState = "OFF ";

  lcd.print(relayState);
  lcd.print(temperature);
  lcd.setCursor(11, 0);
  lcd.print(myRamp.getStateText());
  lcd.setCursor(0, 1);
  lcd.print(myRamp.getStage());
  lcd.print("/");
  lcd.print(myRamp.getTotalStages());
  lcd.setCursor(4, 1);
  lcd.print(Setpoint, 0);
  lcd.setCursor(10, 1);
  lcd.print(myRamp.getTimeRemaining());


  /**********LOGGING**********/
  unsigned long seconds = (now - systimer) / 1000;
  char secStr[16];
  sprintf(secStr, "%lu", seconds);
  writeToSD(String(secStr) + ",loop," + relayState + "," + temperature + "," + Setpoint + "," + myRamp.getStateText() + "," + myRamp.getStage() + "/" + myRamp.getTotalStages() + "," + myRamp.getTimeRemaining());
  Serial.println(String(secStr) + ",loop," + relayState + "," + temperature + "," + Setpoint + "," + myRamp.getStateText() + "," + myRamp.getStage() + "/" + myRamp.getTotalStages() + "," + myRamp.getTimeRemaining());

  delay(500);
}//Loop

String doubleToString(double input) {
  float floatvar = millis();
  char dtostrfbuffer[15];
  dtostrf(input, 8, 1, dtostrfbuffer);
  return String(dtostrfbuffer);
}

void writeToSD(String msg)
{
  if (!sdCardInited)
    return;

  float floatvar = millis();
  char dtostrfbuffer[15];
  dtostrf(floatvar, 8, 2, dtostrfbuffer);
  String time = String(dtostrfbuffer);
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(filename, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(time + "," + msg);
    dataFile.close();
  }
  //if the file isn't open, pop up an error:
  else {
    Serial.println("error opening " + filename);
  }
}// writeToSD

double readTemp(int tryCount)
{
  if (tryCount > 10)
  {
    return -1;
  }

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
