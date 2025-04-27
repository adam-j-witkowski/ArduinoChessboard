/**
 * Reference: https://electronoobs.com/eng_arduino_tut82.php
 */

#define SENSOR_COUNT 6
#define SENSOR_SAMPLE_COUNT 10
#define SENSOR_SAMPLE_INTERVAL_MS 20
#define SENSOR_DETECTION_THRESHOLD 4
#define CALIBRATION_SAMPLES 50  // Number of samples to take during calibration
#define REPORT_INTERVAL_MS 500  // How often to send the sensor status report

const int sensorPins[SENSOR_COUNT] = {A0, A1, A2, A3, A4, A5};

int sensorSamples[SENSOR_COUNT]; // one integer of accumulated samples per sensor
int sensorSampleCounter = 0; // how many samples have we taken so far?
int sensorBaselines[SENSOR_COUNT]; // calibrated baseline values for each sensor
bool sensorHasMagnet[SENSOR_COUNT]; // table of binary values indicating magnet presence

void calibrateSensors() {
  Serial.println();
  Serial.println("Starting sensor calibration...");
  
  // Initialize arrays for calibration
  long calibrationAccumulator[SENSOR_COUNT] = {0};
  
  // Turn on built-in LED during calibration
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Variables for timing
  unsigned long calibrationTimer = millis();
  unsigned long finishTimer = 0;
  int sampleCount = 0;
  bool calibrationDone = false;
  bool showingComplete = false;
  
  // Run calibration until complete
  while (!showingComplete) {
    unsigned long currentMillis = millis();
    
    // Sample collection phase
    if (!calibrationDone && currentMillis - calibrationTimer >= SENSOR_SAMPLE_INTERVAL_MS) {
      calibrationTimer = currentMillis;
      
      if (sampleCount < CALIBRATION_SAMPLES) {
        for (int i = 0; i < SENSOR_COUNT; i++) {
          calibrationAccumulator[i] += analogRead(sensorPins[i]);
        }
        sampleCount++;
      } else {
        // Calculate average baseline values
        for (int i = 0; i < SENSOR_COUNT; i++) {
          sensorBaselines[i] = calibrationAccumulator[i] / CALIBRATION_SAMPLES;
          Serial.print("Sensor ");
          Serial.print(i);
          Serial.print(" baseline: ");
          Serial.println(sensorBaselines[i]);
        }
        
        // Mark calibration as complete
        calibrationDone = true;
        Serial.println("Calibration complete!");
        finishTimer = currentMillis;
      }
    }
    
    // After calibration complete, wait 1000ms before continuing
    if (calibrationDone && !showingComplete) {
      if (currentMillis - finishTimer >= 1000) {
        digitalWrite(LED_BUILTIN, LOW);
        showingComplete = true;
      }
    }
  }
}

void setup()
{
    Serial.begin(9600);
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
      pinMode(sensorPins[i], INPUT);
      sensorSamples[i] = 0;
      sensorHasMagnet[i] = false; // Initialize all sensors as having no magnet
    }
    
    calibrateSensors(); // Run calibration at startup
}

void sendSensorStatusReport() {
  Serial.print("<");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(sensorHasMagnet[i] ? "1" : "0");
    if (i < SENSOR_COUNT - 1) {
      Serial.print(",");
    }
  }
  Serial.print(">");
}

void loop()
{
  static unsigned long sensorTimer = millis();
  static unsigned long reportTimer = millis();

  bool sensorDataReady = false;
  int sensorData[SENSOR_COUNT];

  // Sample sensors
  if (millis() - sensorTimer >= SENSOR_SAMPLE_INTERVAL_MS) {
    sensorTimer = millis();

    if (sensorSampleCounter < SENSOR_SAMPLE_COUNT) {
      for (int i = 0; i < SENSOR_COUNT; i++) {
        sensorSamples[i] += analogRead(sensorPins[i]);
      }
      sensorSampleCounter += 1;
    } else {
      for (int i = 0; i < SENSOR_COUNT; i++) {
        sensorData[i] = sensorSamples[i] / SENSOR_SAMPLE_COUNT;
        sensorSamples[i] = 0;
      }
      sensorSampleCounter = 0;
      sensorDataReady = true;
    }
  }

  // Update magnet status
  if (sensorDataReady) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
      sensorHasMagnet[i] = abs(sensorBaselines[i] - sensorData[i]) > SENSOR_DETECTION_THRESHOLD;
    }
  }
  
  // Send periodic reports
  if (millis() - reportTimer >= REPORT_INTERVAL_MS) {
    reportTimer = millis();
    sendSensorStatusReport();
  }
}