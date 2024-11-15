
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *    File: WatsonMarlowPumpControl.ino
 *    Description: Main src file for controlling Watson Marlow 205U peristaltic pump. 
 *      Useful for running pump as chemostat. Originally written for an Arduino R4 WiFi.
 *      You may have to reduce MAX_STEPS if your Arduino has limited memory.
 *    Author: Kevin A. Kasper, Aug 2024
 *    See github repo: https://github.com/kkasper/WatsonMarlow205U_Control
 *    Contact me at: KevinAKasper at gmail dot com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Command to abort, command to pause
// When clearing steps, also stop current sequence
// List all steps
// 

#include "RTC.h"
#include "EEPROM.h"

// Define a reasonable maximum number of steps (has to fit in Arduino memory)
#define MAX_STEPS 100
#define MAX_VOLTAGE_MV 4710.0
#define EEProm_Addr_MinFlowRate 0x00
#define EEProm_Addr_MaxFlowRate 0x04
#define EEProm_Addr_OffsetVoltage 0x08

float minFlowRate = 0.5f;
float maxFlowRate = 90.0f;
int offsetVoltage = 0;

// Structure to hold the integer and float steps
typedef struct {
  long time_seconds;
  float flow_rate;
} IntFloatStep;

// Array to store the steps
IntFloatStep steps[MAX_STEPS];

uint8_t process_buffer[2] = { 0 };
float fraction_voltage = 0.0;

int numSteps = 0;

const byte numChars = 64;
char receivedChars[numChars];  // an array to store the received data
char tempChars[numChars];      // temporary array for use when parsing

boolean newData = false;
boolean newSequence = false;
boolean sequenceRunning = false;
boolean calibrationMode = false;
int calibrationStep = 0;
int calibrationVoltageADC = 0;
int ref_voltage = 0;
int current_step = 0;

RTCTime step_start_time;
RTCTime cur_time;


void print_menu(void) {
  Serial.println("----------------------------");
  Serial.println("Enter sequence information without spaces at any time, beginning with '<' and ending with '>' : ");
  Serial.println("Format: <Step1Duration_seconds,Step1FlowRate_rpm,...,StepNDuration_seconds,StepNFlowRate_rpm>");
  Serial.println("Example sequence: ");
  Serial.println("\tStep 1: 12 seconds at 2.5 RPM");
  Serial.println("\tStep 2: 3500 seconds at 2.7 RPM");
  Serial.println("\t\t...");
  Serial.println("\tStep 5: 2150 seconds at 3.8 RPM");
  Serial.println("\t<12,2.5,3500,2.7,...,2150,3.8>");
  Serial.println("Keep sending step sequences until you have loaded your desired steps.");
  Serial.println("Because the Arduino's serial input buffer is limited, insert at most 5 steps per Serial message.");
  Serial.println("The Excel file in the GitHub repo will generate these strings for you.");
  Serial.println("GitHub repo: https://github.com/kkasper/WatsonMarlow205U_Control");
  Serial.println("Send \"<s>\" after inputting all your steps (up to 100 steps maximum) to begin processing.");
  Serial.println("Send \"<r>\" at any point to clear all steps.");
  Serial.println("Send \"<c>\" to enter calibration mode and set min/max flow rate and signal offset voltage.");
  Serial.println("Note that voltages will not be 100% accurate due to unavoidable limitations in Arduino hardware.");
  printCalibrationParams();
}

void setup() {
  // initialize serial communications at 115200 bps:
  Serial.begin(115200);
  analogWriteResolution(12);

  EEPROM.get(EEProm_Addr_MinFlowRate, minFlowRate);
  EEPROM.get(EEProm_Addr_MaxFlowRate, maxFlowRate);
  EEPROM.get(EEProm_Addr_OffsetVoltage, offsetVoltage);

  RTC.begin();
  RTCTime mytime(15, Month::NOVEMBER, 2024, 0, 00, 00, DayOfWeek::FRIDAY, SaveLight::SAVING_TIME_ACTIVE);
  RTC.setTime(mytime);

  print_menu();
}

void loop() {
  // read the analog in value:
  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseInputData();
    // showParsedData();
    newData = false;
  }

  if (calibrationMode) {
    processCalibrationStep();
  }

  processSequenceStep();

  // wait 2 milliseconds before the next loop for the analog-to-digital
  // converter to settle after the last reading:
  delay(2);
}
