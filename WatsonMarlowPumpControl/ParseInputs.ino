
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *    File: ParseInputs.ino
 *    Description: Contains logic for interpreting input strings. 
 *      All inputs must be enclosed within '<' and '>' to properly parse. 
 *      The Arduino Uno R4 serial buffer size is 512 bytes and in the Arduino language a single 
 *      character (char) is 1-2 bytes, so your input can be maximum 256-512 characters long.
 *    Author: Kevin Kasper, Aug 2024
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */



// This was just copied from online
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


// Function to parse input received over serial
void parseInputData() {

  Serial.print("Received characters: ");
  Serial.println(receivedChars);

  char* input = receivedChars;  // Use the global buffer
  char* strtokIndx = strtok(input, ",");

  if (strlen(input) == 1) {
    // If a calibration is in process and command is not End Calibration, print error
    if ((calibrationStep != 0) && !(*input == 'e' || *input == 'E' || *input == 'c' || *input == 'C')) {
      Serial.println("Calibration ongoing. Send \"<e>\" to cancel calbration.");
      return;
    }

    switch (*input) {

      // Change so that c just completes and saves the calibration
      case 'c':
      case 'C':
        if (calibrationStep == 0) {
          Serial.println("Starting calibration.");
        } else {
          calibrationStep = 3;
        }
        calibrationMode = true;
        break;

      case 'e':
      case 'E':
        if (calibrationStep == 0) {
          Serial.println("Not currently calibrating.");
        } else {
          Serial.println("Calibration ended.");
          calibrationStep = 0;
          calibrationMode = false;
        }
        break;

      case 'h':
      case 'H':
        calibrationStep = 2;
        calibrationMode = true;
        break;

      case 'l':
      case 'L':
        calibrationStep = 1;
        calibrationMode = true;
        break;

      case 'r':
      case 'R':
        Serial.println("Cleared all steps.");
        numSteps = 0;
        break;

      case 's':
      case 'S':
        Serial.println("Commencing sequence.");
        newSequence = true;
        break;

      default:
        Serial.println("Invalid command received.");
    }
  } else {
    //  If a non-command was sent over, check if a calibration is ongoing.
    //  Otherwise assume the incoming data string contains steps

    // Sometimes calibration step leaks through to parse step string?
    if (calibrationStep == 1) {
      parseCalibrationString(strtokIndx);
      return;
    } else if (calibrationStep != 0) {
      Serial.println("Calibration ongoing. Send \"<e>\" to cancel calbration.");
      return;
    } else {
      // Moved this into this conditional which is only true when calibrationStep == 0
      // which should be the case for normal operation???
      parseStepString(strtokIndx);
    }
  }
}

void printCalibrationParams(void) {
  Serial.println("------------------------");
  Serial.print("Min Flow Rate setting: ");
  Serial.print(minFlowRate);
  Serial.print(" RPM. Max Flow Rate setting: ");
  Serial.print(maxFlowRate);
  Serial.println(" RPM. ");
  Serial.print("Offset Voltage setting: ");
  Serial.print(offsetVoltage);
  Serial.println("mV.");
  Serial.println("------------------------\n");
}

void parseCalibrationString(char* strtokIndx) {
  // Read min and max flow rates
  if (strtokIndx == NULL) {
    Serial.println("Malformed calibration string received, failed parsing minimum flow rate. Should be a floating point number.");
    return;
  }
  minFlowRate = atof(strtokIndx);
  strtokIndx = strtok(NULL, ",");

  if (strtokIndx == NULL) {
    Serial.println("Malformed string received, failed parsing maximum flow rate. Should be a floating point number.");
    return;
  }
  maxFlowRate = atof(strtokIndx);
  strtokIndx = strtok(NULL, ",");

  // Read the offset voltage in mV
  if (strtokIndx == NULL) {
    Serial.println("Malformed calibration string received, failed parsing offset voltage (mV). Should be integer.");
    return;
  }
  offsetVoltage = atoi(strtokIndx);
  offsetVoltage = (offsetVoltage > MAX_VOLTAGE_MV) ? MAX_VOLTAGE_MV : offsetVoltage;  // Limit the offset voltage

  printCalibrationParams();

  calibrationMode = true;
}

void printSteps(int numSteps) {
  if (numSteps < 1) {
    Serial.println("No steps received so far.");
    return;
  }
  // Example: Output the steps to the serial monitor
  Serial.print("Steps received so far (if any): ");
  for (int i = 0; i < numSteps; i++) {
    Serial.print("\t[");
    Serial.print(i + 1);
    Serial.print("] Time (s) = ");
    Serial.print(steps[i].time_seconds);
    Serial.print(", Flow Rate = ");
    Serial.println(steps[i].flow_rate, 2);
  }
}

// Takes in string containing step information and inserts into IntFloatStep steps array
void parseStepString(char* strtokIndx) {

  do {
    if (strtokIndx == NULL) {
      Serial.println("Malformed string received, failed parsing step duration.");
      break;
    }
    steps[numSteps].time_seconds = atol(strtokIndx);

    strtokIndx = strtok(NULL, ",");
    if (strtokIndx == NULL) {
      Serial.println("Malformed string received, failed parsing flow rate.");
      break;
    }
    steps[numSteps].flow_rate = atof(strtokIndx);

    numSteps++;
    if (numSteps == MAX_STEPS) {
      Serial.print("Maximum step limit reached: ");
      Serial.print(numSteps);
      Serial.println(".");
      break;
    }
    strtokIndx = strtok(NULL, ",");
  } while ((strtokIndx != NULL) && (numSteps < MAX_STEPS));

  Serial.print("Num steps received: ");
  Serial.println(numSteps);
  printSteps(numSteps);
}
