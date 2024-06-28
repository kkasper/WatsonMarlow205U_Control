/*
  Analog input, analog output, serial output

  Reads an analog input pin, maps the result to a range from 0 to 255 and uses
  the result to set the pulse width modulation (PWM) of an output pin.
  Also prints the results to the Serial Monitor.

  The circuit:
  - potentiometer connected to analog pin 0.
    Center pin of the potentiometer goes to the analog pin.
    side pins of the potentiometer go to +5V and ground
  - LED connected from digital pin 9 to ground through 220 ohm resistor

  created 29 Dec. 2008
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

  https://www.arduino.cc/en/Tutorial/BuiltInExamples/AnalogInOutSerial
*/

#include "RTC.h"


// Define a reasonable maximum number of steps
#define MAX_PAIRS 20
#define MAX_VOLTAGE_MV 4710.0
// Structure to hold the integer and float steps
typedef struct {
  long time_seconds;
  float flow_rate;
} IntFloatStep;

// Array to store the steps
IntFloatStep steps[MAX_PAIRS];

// These constants won't change. They're used to give names to the pins used:
const int analogInPin = A0;  // Analog input pin that the potentiometer is attached to
const int analogOutPin = 9;  // Analog output pin that the LED is attached to

int sensorValue = 0;  // value read from the pot
int outputValue = 0;  // value output to the PWM (analog out)
int process_signal_output = 0;
uint8_t process_buffer[2] = { 0 };
float fraction_voltage = 0.0;
float minFlowRate = 0.5;
float maxFlowRate = 90;
int numSteps = 0;
int offsetVoltage = 0;

const byte numChars = 64;
char receivedChars[numChars];  // an array to store the received data
char tempChars[numChars];      // temporary array for use when parsing

boolean newData = false;
boolean newSequence = false;
boolean sequenceRunning = false;
boolean calibrationMode = false;
int calibrationStep = 0;
int calibrationVoltageADC = 0;
int dataNumber = 0;  // new for this version
int ref_voltage = 0;
int current_step = 0;

RTCTime step_start_time;
RTCTime cur_start_time;

void printSteps(int numSteps) {
  // Example: Output the steps to the serial monitor
  for (int i = 0; i < numSteps; i++) {
    Serial.print("Step ");
    Serial.print(i + 1);
    Serial.print(": Time (s) = ");
    Serial.print(steps[i].time_seconds);
    Serial.print(", Flow Rate = ");
    Serial.println(steps[i].flow_rate, 2);
  }
}

//============

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

// Function to parse data
void parseData() {

  Serial.print("Received characters: ");
  Serial.println(receivedChars);

  char* input = receivedChars;  // Use the global buffer
  char* strtokIndx = strtok(input, ",");

  // Read min and max flow rates
  if (strtokIndx == NULL) {
    Serial.println("Malformed string received, failed parsing minimum flow rate. Should be a floating point number.");
    return;
  }
  minFlowRate = atof(strtokIndx);
  strtokIndx = strtok(NULL, ",");

  if (minFlowRate == 0) {
    if (calibrationStep == 0) {
      if (strtokIndx == NULL) {
        Serial.println("Malformed calibration string received, failed parsing min offset voltage (mV).");
        return;
      }
      offsetVoltage = atoi(strtokIndx);
      offsetVoltage = (offsetVoltage > MAX_VOLTAGE_MV) ? MAX_VOLTAGE_MV : offsetVoltage;  // Limit the offset voltage
      Serial.print("Offset Voltage: ");
      Serial.print(offsetVoltage);
      Serial.println("mV");
    }
    calibrationMode = true;
    calibrationStep += 1;
    return;
  }

  if (strtokIndx == NULL) {
    Serial.println("Malformed string received, failed parsing maximum flow rate. Should be a floating point number.");
    Serial.print("Minimum flow rate parsed successfully: ");
    Serial.print(minFlowRate);
    Serial.println(" RPM");
    return;
  }
  maxFlowRate = atof(strtokIndx);
  Serial.println("------------------------");
  Serial.print("Min Flow Rate: ");
  Serial.print(minFlowRate);
  Serial.print(" RPM. Max Flow Rate: ");
  Serial.print(maxFlowRate);
  Serial.println(" RPM.");

  // Read the offset voltage in mV
  strtokIndx = strtok(NULL, ",");
  if (strtokIndx == NULL) {
    Serial.println("Malformed string received, failed parsing offset voltage (mV).");
    return;
  }
  offsetVoltage = atoi(strtokIndx);
  offsetVoltage = (offsetVoltage > MAX_VOLTAGE_MV) ? MAX_VOLTAGE_MV : offsetVoltage;  // Limit the offset voltage
  Serial.print("Offset Voltage: ");
  Serial.print(offsetVoltage);
  Serial.println("mV");

  // Read the number of steps
  strtokIndx = strtok(NULL, ",");
  if (strtokIndx == NULL) {
    Serial.println("Malformed string received, failed parsing number of steps.");
    return;
  }
  numSteps = atoi(strtokIndx);
  numSteps = (numSteps > MAX_PAIRS) ? MAX_PAIRS : numSteps;  // Limit the number of steps to the size of the array
  Serial.print("Num steps: ");
  Serial.println(numSteps);

  // Process each step
  for (int i = 0; i < numSteps; i++) {
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx == NULL) {
      Serial.print("Malformed string received, failed parsing time at step [ ");
      Serial.print(i + 1);
      Serial.println(" ]. Steps processed so far (if any): ");
      printSteps(i);
      return;  // Safeguard against malformed input
    }
    steps[i].time_seconds = atol(strtokIndx);

    strtokIndx = strtok(NULL, ",");
    if (strtokIndx == NULL) {
      Serial.print("Malformed string received, failed parsing floW_rate at step [ ");
      Serial.print(i + 1);
      Serial.println(")]. Steps processed so far (if any): ");
      printSteps(i);
      return;  // Safeguard against malformed input
    }
    steps[i].flow_rate = atof(strtokIndx);
  }

  printSteps(numSteps);
  Serial.println("------------------------");

  newSequence = true;
}


// input flow rate = 2.763
// min flow rate = 0.5
// max flow rate = 14.5
// --- * 1000 -->
// input = 2763, min = 0500, max = 14500
long floatMap(float x, float in_min, float in_max, long out_min, long out_max) {
  int new_x = x * 1000;
  int new_in_min = in_min * 1000;
  int new_in_max = in_max * 1000;

  return (new_x - new_in_min) * (out_max - out_min) / (new_in_max - new_in_min) + out_min;
}


void setFlowRate(float newRate) {
  int offsetVoltageADC;
  int outputValue;

  if (newRate == 0) {
    outputValue = 0;
  } else {
    // Map new flow rate to voltage value corresponding to between min and max flow rate, considering offsetVoltage
    offsetVoltageADC = floor((float)(offsetVoltage / MAX_VOLTAGE_MV) * 4095.0);
    outputValue = floatMap(newRate, minFlowRate, maxFlowRate, offsetVoltageADC, 4095);
    Serial.print("Calibration voltage: ");
    Serial.print(offsetVoltage);
    Serial.print("\tCalibration voltage ADC: ");
    Serial.println(offsetVoltageADC);
    Serial.print("\tFlow Rate Mapped ADC: ");
    Serial.println(outputValue);
  }
  // change the analog out value:
  analogWrite(A0, outputValue);
  fraction_voltage = (float)(outputValue / 4095.0);

  // print the results to the Serial Monitor:
  Serial.print("New flow rate received: ");
  Serial.print(newRate);
  Serial.print(" RPM.\t Output DAC = ");
  Serial.print(outputValue);
  Serial.print("\t Output fraction = ");
  Serial.print(fraction_voltage);
  Serial.print("\t Output_voltage = ");
  Serial.print(fraction_voltage * MAX_VOLTAGE_MV);
  Serial.print(" mV.");
  Serial.print("\t Output_flow_rate = ");
  Serial.print( (((maxFlowRate - minFlowRate) * (outputValue/4095.0)) + minFlowRate));
  Serial.println(" RPM.");
}


void print_menu(void) {
  Serial.println("----------------------------");
  Serial.println("Enter sequence information without spaces, beginning with '<' and ending with '>' : ");
  Serial.println("Format: <MinFlowRate_RPM,MaxFlowRate_RPM,OffsetVoltage_mV,#Steps,Step1Duration_seconds,Step1FlowRate_rpm,...,StepNDuration_seconds,StepNFlowRate_rpm>");
  Serial.println("Example: ");
  Serial.println("\tMin flow rate: 0.7 RPM, Max flow rate: 14.5 RPM");
  Serial.println("\tProcess offset voltage: 40 mV, Number of steps: 2");
  Serial.println("\tStep 1: 12 seconds at 2.5 RPM");
  Serial.println("\tStep 2: 3500 seconds at 2.7 RPM");
  Serial.println("\t<0.7,14.5,40,2,12,2.5,3500,2.7>");
  Serial.println("Send <0, OffsetVoltage_mV> to enter calibration mode outputting signal offset voltage. ");
  Serial.println("Example: ");
  Serial.println("\tMin offset voltage: 400 mV");
  Serial.println("\t<0, 400>");
  Serial.println("Note that voltages will not be 100% accurate due to unavoidable limitations in Arduino hardware.");
  Serial.println("----------------------------");
}

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(115200);
  analogWriteResolution(12);

  RTC.begin();
  RTCTime mytime(24, Month::JUNE, 2024, 0, 00, 00, DayOfWeek::MONDAY, SaveLight::SAVING_TIME_ACTIVE);
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
    parseData();
    // showParsedData();
    newData = false;
  }

  if (calibrationMode) {
    if (calibrationStep == 1) {
      calibrationVoltageADC = floor((((float)offsetVoltage / MAX_VOLTAGE_MV) * 4095.0));
      Serial.print("Calibration voltage: ");
      Serial.print(offsetVoltage);
      Serial.print("\tCalibration voltage ADC: ");
      Serial.print(calibrationVoltageADC);
      Serial.print("\tFraction: ");
      Serial.println((float)(offsetVoltage / MAX_VOLTAGE_MV));

      analogWrite(A0, calibrationVoltageADC);
      Serial.println("Outputting minimum process signal voltage.");
      Serial.println("Send <0> to proceed to max process signal output voltage (~4.71 to 5 Volts).\n");
      calibrationMode = false;
    }
    if (calibrationStep == 2) {
      analogWrite(A0, 4095);
      Serial.println("Outputting maximum process signal voltage.");
      Serial.println("Send <0> again to finish calibration.\n");
      calibrationMode = false;
    }
    if (calibrationStep == 3) {
      analogWrite(A0, 0);
      calibrationMode = false;
      calibrationStep = 0;
      Serial.println("Calibration mode exited.\n");
      print_menu();
    }
  }

  // Set the step start time to now, start at step 0, set flow rate for step 0, start sequence
  if (newSequence == true) {
    newSequence = false;
    current_step = 0;

    Serial.println("Starting new sequence:");
    Serial.print("Step ");
    Serial.print(current_step + 1);
    Serial.print(" : ");
    Serial.print(steps[current_step].time_seconds);
    Serial.print(" seconds at ");
    Serial.print(steps[current_step].flow_rate);
    Serial.println(" RPM.");

    RTC.getTime(step_start_time);
    setFlowRate(steps[current_step].flow_rate);
    sequenceRunning = true;
  }

  // Get current time, check if sequence is running, check if elapsed seconds is greater than current step runtime
  RTC.getTime(cur_start_time);
  if ((sequenceRunning == true) && (difftime(cur_start_time.getUnixTime(), step_start_time.getUnixTime()) > steps[current_step].time_seconds)) {
    current_step = current_step + 1;
    if (current_step == numSteps) {
      sequenceRunning = false;
      setFlowRate(0);  // Set flow rate to off
      Serial.println("Sequence run successfully.");
    } else {
      Serial.println("Changing to next step...\n");
      Serial.print("Step ");
      Serial.print(current_step + 1);
      Serial.print(": ");
      Serial.print(steps[current_step].time_seconds);
      Serial.print(" seconds at ");
      Serial.print(steps[current_step].flow_rate);
      Serial.println(" RPM.");

      RTC.getTime(step_start_time);
      // set step(current_step).flow_rate
      setFlowRate(steps[current_step].flow_rate);
    }
  }

  // wait 2 milliseconds before the next loop for the analog-to-digital
  // converter to settle after the last reading:
  delay(2);
}
