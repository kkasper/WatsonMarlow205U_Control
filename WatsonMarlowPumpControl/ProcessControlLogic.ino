
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *    File: ParseControlLogic.ino
 *    Description: Contains functional logic for controlling pump.
 *      
 *    Author: Kevin A. Kasper, Aug 2024
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */



void processSequenceStep(void) {
  // Set the step start time to now, start at step 0, set flow rate for step 0, start sequence
  if (newSequence == true) {
    newSequence = false;
    current_step = 0;

    Serial.println("Processing new sequence:");
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
  RTC.getTime(cur_time);
  if ((sequenceRunning == true) && (difftime(cur_time.getUnixTime(), step_start_time.getUnixTime()) > steps[current_step].time_seconds)) {
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
}


void processCalibrationStep(void) {
  switch (calibrationStep) {
    case 0:
      Serial.println("Send calibration string to proceed: ");
      Serial.println("Format: <MinFlowRateRPM,MaxFlowRateRPM,OffsetVoltage_mV");
      Serial.println("Example: ");
      Serial.println("\tMin flow rate: 0.7 (RPM), Max flow rate: 14.5 (RPM)");
      Serial.println("\tMin offset voltage: 400 (mV)");
      Serial.println("\t<0.7,14.5,400>");
      Serial.println("Send \"<e>\" at any point to exit calibration.");
      calibrationStep += 1;
      calibrationMode = false;
      break;

    case 1:
      calibrationVoltageADC = floor((((float)offsetVoltage / MAX_VOLTAGE_MV) * 4095.0));
      Serial.print("Calibration voltage: ");
      Serial.print(offsetVoltage);
      Serial.print("\tCalibration voltage ADC: ");
      Serial.print(calibrationVoltageADC);
      Serial.print("\tFraction: ");
      Serial.println((float)(offsetVoltage / MAX_VOLTAGE_MV));
      analogWrite(A0, calibrationVoltageADC);
      Serial.println("Outputting minimum process signal voltage.");
      Serial.println("Send <c> to proceed to max process signal output voltage (~4.71 to 5 Volts).\n");
      calibrationStep += 1;
      calibrationMode = false;
      break;

    case 2:
      analogWrite(A0, 4095);
      Serial.println("Outputting maximum process signal voltage.");
      Serial.println("Send <c> again to finish calibration.\n");
      calibrationStep += 1;
      calibrationMode = false;
      break;

    case 3:
      analogWrite(A0, 0);
      Serial.println("Calibration complete. Parameters stored in Arduino EEPROM.\n");
      EEPROM.update(EEProm_Addr_MinFlowRate, minFlowRate);
      EEPROM.update(EEProm_Addr_MaxFlowRate, maxFlowRate);
      EEPROM.update(EEProm_Addr_OffsetVoltage, offsetVoltage);
      print_menu();
      calibrationStep = 0;
      calibrationMode = false;
  }
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
  Serial.print("New flow rate set: ");
  Serial.print(newRate);
  Serial.print(" RPM.\t Output DAC = ");
  Serial.print(outputValue);
  Serial.print("\t Output fraction = ");
  Serial.print(fraction_voltage);
  Serial.print("\t Output_voltage = ");
  Serial.print(fraction_voltage * MAX_VOLTAGE_MV);
  Serial.print(" mV.");
  Serial.print("\t Output_flow_rate = ");
  Serial.print((((maxFlowRate - minFlowRate) * (outputValue / 4095.0)) + minFlowRate));
  Serial.println(" RPM.");
}
