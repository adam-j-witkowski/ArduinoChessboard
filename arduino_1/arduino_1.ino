/**
 * Group 61 - Smart Chessboard - Arduino #1
 * - Seth Traman (stram3, stram3@uic.edu)
 * - Adam Witkowski (awitk, awitk@uic.edu)
 * - Paul Quinto (pquin6, pquin6@uic.edu)
 * 
 * Arduino #1 is responsible for reading the state of the hall-effect sensors
 * and sending that data to Arduino #3 over serial.  The analog sensors are
 * finicky, so we need to take a few samples and average them out to get a
 * stable reading.  We also need to calibrate the sensors at startup to
 * determine the baseline value for each sensor.
 * 
 * For messaging, we use '<' and '>' to indicate the start and end of a message.
 * After the start character, we send the number of sensors, followed by a colon,
 * and then a comma-separated list of sensor states (1 or 0) indicating whether
 * a magnet is present or not.
 * 
 * Reference: https://electronoobs.com/eng_arduino_tut82.php
 * Reference: https://docs.arduino.cc/language-reference/en/functions/communication/serial/
 * Reference: https://learn.sparkfun.com/tutorials/multiplexer-breakout-hookup-guide/74hc4051-breakout-overview
 * Reference: https://forum.arduino.cc/t/coding-74hc4051-multiplexing/298069
 */

#define SENSOR_COUNT 6               // how many hall effect sensors?
#define SENSOR_SAMPLE_COUNT 10       // how many samples to take for each sensor reading?
#define SENSOR_SAMPLE_INTERVAL_MS 20 // how long to wait between each sample?
#define SENSOR_DETECTION_THRESHOLD 4 // how far from the baseline must the sensor reading be to indicate "1"?
#define CALIBRATION_SAMPLES 50       // how many calibration samples should we take?
#define REPORT_INTERVAL_MS 500       // how often should we send a status report?

const int sensorPins[SENSOR_COUNT] = {A0, A1, A2, A3, A4, A5};

int sensorSamples[SENSOR_COUNT];    // we accumulate the sensor samples over time and take the average
int sensorSampleCounter = 0;        // how many samples have we taken so far?
int sensorBaselines[SENSOR_COUNT];  // the "resting" value for each sensor, calibrated during startup
bool sensorHasMagnet[SENSOR_COUNT]; // array of booleans indicating absence/presence of magnet

/**
 * I have this function because the analog hall-effect sensors tend to rest at a stable "default" state,
 * but that state is not consistent across sensors and it would be prohibitively difficult to manually
 * calibrate each of the hall-effect sensors in the code using constant values.

 * Basically, we want to take a couple of preliminary readings during the start-up phase, when NO magnets are present,
 * and then average out those values and save them for later.  That way, we have a 'baseline' to compare against for
 * each connected sensor.

 * TODO: maybe not needed for digital hall-effect sensors?
 */
void calibrateSensors()
{
  Serial.println();
  Serial.println("Starting sensor calibration...");

  long calibrationSamples[SENSOR_COUNT] = {0};

  unsigned long calibrationTimer = millis();
  int sampleCount = 0;
  bool calibrating = true;

  while (calibrating)
  {
    if (millis() - calibrationTimer >= SENSOR_SAMPLE_INTERVAL_MS)
    {
      // This whole block runs every few ms until calibration is complete

      calibrationTimer = millis();

      if (sampleCount < CALIBRATION_SAMPLES)
      {
        // Accumulate up to CALIBRATION_SAMPLES samples
        for (int i = 0; i < SENSOR_COUNT; i++)
        {
          calibrationSamples[i] += analogRead(sensorPins[i]);
        }
        sampleCount++;
      }
      else
      {
        // Average the calibration samples and save as the baseline
        for (int i = 0; i < SENSOR_COUNT; i++)
        {
          sensorBaselines[i] = calibrationSamples[i] / CALIBRATION_SAMPLES;
          Serial.print("Sensor ");
          Serial.print(i);
          Serial.print(" baseline: ");
          Serial.println(sensorBaselines[i]);
        }

        calibrating = false;
        Serial.println("Calibration complete!");
      }
    }
  }
}

void setup()
{
  Serial.begin(9600);

  for (int i = 0; i < SENSOR_COUNT; i++)
  {
    pinMode(sensorPins[i], INPUT);
    sensorSamples[i] = 0;
    sensorHasMagnet[i] = false;
  }

  calibrateSensors();
}

void sendSensorStatusReport()
{
  Serial.print("<"); // message-begin
  // First send the number of sensors
  Serial.print(SENSOR_COUNT);
  Serial.print(":");
  // Then send the sensor data
  for (int i = 0; i < SENSOR_COUNT; i++)
  {
    Serial.print(sensorHasMagnet[i] ? "1" : "0");
    if (i < SENSOR_COUNT - 1)
    {
      Serial.print(",");
    }
  }
  Serial.print(">"); // message-end
}

void loop()
{
  static unsigned long sensorTimer = millis();
  static unsigned long reportTimer = millis();

  bool sensorDataReady = false;
  int sensorData[SENSOR_COUNT];

  // Sample sensors
  if (millis() - sensorTimer >= SENSOR_SAMPLE_INTERVAL_MS)
  {
    sensorTimer = millis();

    if (sensorSampleCounter < SENSOR_SAMPLE_COUNT)
    {
      // Accumulate sensor samples
      for (int i = 0; i < SENSOR_COUNT; i++)
      {
        sensorSamples[i] += analogRead(sensorPins[i]);
      }
      sensorSampleCounter += 1;
    }
    else
    {
      // Average the samples we have so far, and save the reading for later
      for (int i = 0; i < SENSOR_COUNT; i++)
      {
        sensorData[i] = sensorSamples[i] / SENSOR_SAMPLE_COUNT;
        sensorSamples[i] = 0;
      }
      sensorSampleCounter = 0;
      sensorDataReady = true;
    }
  }

  if (sensorDataReady)
  {
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
      // 'true' if the difference between the baseline and the reading is greater than whatever threshold we defined
      sensorHasMagnet[i] = abs(sensorBaselines[i] - sensorData[i]) > SENSOR_DETECTION_THRESHOLD;
    }
  }

  // Send periodic reports
  if (millis() - reportTimer >= REPORT_INTERVAL_MS)
  {
    reportTimer = millis();
    sendSensorStatusReport();
  }
}