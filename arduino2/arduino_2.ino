#include <TM1637Display.h>
#include <LiquidCrystal.h>

// Pin Definitions
const uint8_t LED_Black = 7;
const uint8_t LED_White = 6;

const uint8_t CLK1 = A4;
const uint8_t DIO1 = A3;
const uint8_t CLK2 = A2;
const uint8_t DIO2 = A1;

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const uint8_t BUZZER_PIN = A0;

// Display Objects
TM1637Display display1(CLK1, DIO1);
TM1637Display display2(CLK2, DIO2);

// Timer Config
const unsigned long INITIAL_TIME = 5 * 60;
volatile unsigned long time1 = INITIAL_TIME;
volatile unsigned long time2 = INITIAL_TIME;

// State Variables
volatile uint8_t activePlayer = 1;
volatile bool paused = true;
unsigned long lastTick = 0;

// Beep/victory tune timing variables
bool beeping = false;
int beepCount = 0;
unsigned long lastBeepTime = 0;

bool playingVictory = false;
int victoryStep = 0;
unsigned long lastVictoryTime = 0;

void setup() {
  Serial.begin(9600);

  pinMode(LED_Black, OUTPUT);
  pinMode(LED_White, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_Black, LOW);
  digitalWrite(LED_White, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  display1.setBrightness(0x0f);
  display2.setBrightness(0x0f);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);

  updateDisplays();
}

void loop() {
  unsigned long now = millis();

  processSerial();

  if (!paused && (now - lastTick >= 1000)) {
    lastTick = now;
    decrementTimer();
    updateDisplays();
  }

  if (beeping) {
    updateQuickBeep();
  }

  if (playingVictory) {
    updateVictoryTune();
  }
}

void processSerial() {
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 'S') { // Start Game
      activePlayer = 1;
      paused = false;
      digitalWrite(LED_Black, HIGH);
      digitalWrite(LED_White, LOW);
      lcd.setCursor(0, 0);
      lcd.print("  Black's Turn   ");
      tone(BUZZER_PIN, 1500, 100);
    }
    else if (cmd == 'M') { // Move has been made, swap players
      if (activePlayer == 1) {
        activePlayer = 2;
        digitalWrite(LED_Black, LOW);
        digitalWrite(LED_White, HIGH);
        lcd.setCursor(0, 0);
        lcd.print("  White's Turn   ");
      } else {
        activePlayer = 1;
        digitalWrite(LED_Black, HIGH);
        digitalWrite(LED_White, LOW);
        lcd.setCursor(0, 0);
        lcd.print("  Black's Turn   ");
      }
      tone(BUZZER_PIN, 1500, 100);
    }
    else if (cmd == 'W') { // White Wins
      paused = true;
      lcd.setCursor(0, 0);
      lcd.print("  White Wins!   ");
      digitalWrite(LED_Black, LOW);
      digitalWrite(LED_White, HIGH);
      startVictoryTune();
    }
    else if (cmd == 'B') { // Black Wins
      paused = true;
      lcd.setCursor(0, 0);
      lcd.print("  Black Wins!   ");
      digitalWrite(LED_Black, HIGH);
      digitalWrite(LED_White, LOW);
      startVictoryTune();
    }
  }
}

void decrementTimer() {
  noInterrupts();
  if (activePlayer == 1 && time1 > 0) {
    --time1;
    if (time1 == 59) startQuickBeep(); // Black low time 
    if (time1 == 0) { // Black out of time
      paused = true;
      lcd.setCursor(0, 0);
      lcd.print("  White Wins!    ");
      digitalWrite(LED_Black, LOW);
      digitalWrite(LED_White, HIGH);
      startVictoryTune();
    }
  }
  else if (activePlayer == 2 && time2 > 0) {
    --time2;
    if (time2 == 59) startQuickBeep(); // White low time
    if (time2 == 0) { // White out of time
      paused = true;
      lcd.setCursor(0, 0);
      lcd.print("  Black Wins!    ");
      digitalWrite(LED_Black, HIGH);
      digitalWrite(LED_White, LOW);
      startVictoryTune();
    }
  }
  interrupts();
}

void updateDisplays() {
  uint8_t m1 = time1 / 60;
  uint8_t s1 = time1 % 60;
  int val1 = m1 * 100 + s1;
  display1.showNumberDecEx(val1, 0b01000000, true);

  uint8_t m2 = time2 / 60;
  uint8_t s2 = time2 % 60;
  int val2 = m2 * 100 + s2;
  display2.showNumberDecEx(val2, 0b01000000, true);
}

// Low timer beeps
void startQuickBeep() {
  beeping = true;
  beepCount = 0;
  lastBeepTime = millis();
}

void updateQuickBeep() {
  static bool beepState = false;
  unsigned long now = millis();

  if (beepState == false && now - lastBeepTime >= 150) {
    tone(BUZZER_PIN, 2000, 100);
    beepState = true;
    lastBeepTime = now;
  }
  else if (beepState == true && now - lastBeepTime >= 100) {
    beepState = false;
    lastBeepTime = now;
    beepCount++;
    if (beepCount >= 3) {
      beeping = false;
    }
  }
}

// Playing victory tune with buzzer
void startVictoryTune() {
  playingVictory = true;
  victoryStep = 0;
  lastVictoryTime = millis();
}
// Handling the timing using millis()
void updateVictoryTune() {
  unsigned long now = millis();

  if (victoryStep == 0 && now - lastVictoryTime >= 0) {
    tone(BUZZER_PIN, 1000, 200);
    lastVictoryTime = now;
    victoryStep = 1;
  }
  else if (victoryStep == 1 && now - lastVictoryTime >= 250) {
    tone(BUZZER_PIN, 1500, 200);
    lastVictoryTime = now;
    victoryStep = 2;
  }
  else if (victoryStep == 2 && now - lastVictoryTime >= 250) {
    tone(BUZZER_PIN, 2000, 400);
    lastVictoryTime = now;
    victoryStep = 3;
  }
  else if (victoryStep == 3 && now - lastVictoryTime >= 500) {
    playingVictory = false;
  }
}
