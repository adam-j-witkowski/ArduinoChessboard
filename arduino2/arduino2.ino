#include <Wire.h>               // I2C communication
#include <TM1637Display.h>      // TM1637 4-digit 7-segment display library
#include <LiquidCrystal.h>      // HD44780 LCD library

// ------------------------
// Pin Definitions
// ------------------------
const uint8_t LED_P1     = 5;   // Player 1 turn indicator LED (cathode pin)
const uint8_t LED_P2     = 6;   // Player 2 turn indicator LED (cathode pin)

const uint8_t CLK1       = 3;   // TM1637 display #1 clock pin
const uint8_t DIO1       = 4;   // TM1637 display #1 data pin
const uint8_t CLK2       = A0;  // TM1637 display #2 clock pin (using analog pin as digital)
const uint8_t DIO2       = A1;  // TM1637 display #2 data pin

// LCD in 4-bit mode: (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

const uint8_t BUZZER_PIN = 13;  // Passive buzzer pin

// Manual control buttons (external 10K pull-downs required)
const uint8_t BTN_PAUSE  = 2;   // Pause/resume toggle button
const uint8_t BTN_RESET  = A2;  // Reset timer button (using analog pin as digital)

// ------------------------
// Display & LCD Objects
// ------------------------
TM1637Display display1(CLK1, DIO1);
TM1637Display display2(CLK2, DIO2);

// ------------------------
// Timer Configuration
// ------------------------
const unsigned long INITIAL_TIME = 5 * 60;  // Each clock starts at 5:00 (minutes * seconds)
volatile unsigned long time1 = INITIAL_TIME;  // Remaining seconds for Player 1
volatile unsigned long time2 = INITIAL_TIME;  // Remaining seconds for Player 2

// ------------------------
// State Variables
// ------------------------
volatile uint8_t  activePlayer = 1;  // Whose clock is ticking (1 or 2)
volatile bool     paused       = true;  // Whether countdown is paused
unsigned long     lastTick     = 0;     // Last timestamp when timer decremented
unsigned long     lastButton   = 0;     // Debounce timestamp for buttons

// ------------------------
// Function Prototypes
// ------------------------
void receiveEvent(int);         // I2C receive handler
void decrementTimer();          // Subtract one second from active clock
void updateDisplays();          // Refresh 7-segment and LCD output

void setup() {
  // --- I2C: receive turn-change commands from Arduino #3 at address 2 ---
  Wire.begin(2);
  Wire.onReceive(receiveEvent);

  // --- LEDs: setup turn indicators ---
  pinMode(LED_P1, OUTPUT);
  pinMode(LED_P2, OUTPUT);
  digitalWrite(LED_P1, LOW);
  digitalWrite(LED_P2, LOW);

  // --- Buzzer: initialize to off ---
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Buttons: inputs (external pull-downs) ---
  pinMode(BTN_PAUSE, INPUT);
  pinMode(BTN_RESET, INPUT);

  // --- 7-segment displays: full brightness ---
  display1.setBrightness(0x0f);
  display2.setBrightness(0x0f);

  // --- LCD: 16×2 initialization ---
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Smart Chess Clock");

  // --- Initial display update ---
  updateDisplays();
}

void loop() {
  unsigned long now = millis();

  // --- Pause/resume toggle ---
  if (digitalRead(BTN_PAUSE) == HIGH && now - lastButton > 200) {
    paused = !paused;
    lastButton = now;
    tone(BUZZER_PIN, 1000, 100);  // beep to confirm
  }

  // --- Reset both clocks ---
  if (digitalRead(BTN_RESET) == HIGH && now - lastButton > 200) {
    noInterrupts();
      time1 = time2 = INITIAL_TIME;
      activePlayer = 1;
      paused = true;
    interrupts();
    updateDisplays();
    tone(BUZZER_PIN, 2000, 200);  // longer beep on reset
    lastButton = now;
  }

  // --- Countdown logic: decrement once per second when running ---
  if (!paused && (now - lastTick >= 1000)) {
    lastTick = now;
    decrementTimer();
    updateDisplays();
  }
}

// -----------------------------------------------------------------
// I2C Receive Handler: called when Arduino #3 sends '1' or '2'
// -----------------------------------------------------------------
void receiveEvent(int) {
  char cmd = Wire.read();
  if (cmd == '1') {
    activePlayer = 1;            // Player 1's turn
    paused = false;              // start countdown
    digitalWrite(LED_P1, HIGH);  // light P1 LED
    digitalWrite(LED_P2, LOW);   // turn off P2 LED
    tone(BUZZER_PIN, 1500, 50);  // quick beep
  }
  else if (cmd == '2') {
    activePlayer = 2;            // Player 2's turn
    paused = false;
    digitalWrite(LED_P1, LOW);
    digitalWrite(LED_P2, HIGH);
    tone(BUZZER_PIN, 1500, 50);
  }
}

// -----------------------------------------------------------------
// Subtract one second from the active player's timer
// -----------------------------------------------------------------
void decrementTimer() {
  noInterrupts();
    if (activePlayer == 1 && time1 > 0) {
      --time1;
      if (time1 == 10) tone(BUZZER_PIN, 2000, 500);  // warning beeps at 10s
    }
    else if (activePlayer == 2 && time2 > 0) {
      --time2;
      if (time2 == 10) tone(BUZZER_PIN, 2000, 500);
    }
  interrupts();
}

// -----------------------------------------------------------------
// Refresh both 7-segment displays and update the bottom line of LCD
// -----------------------------------------------------------------
void updateDisplays() {
  // Format MMSS for display1 (Player 1)
  uint8_t m1 = time1 / 60;
  uint8_t s1 = time1 % 60;
  int val1   = m1 * 100 + s1;  
  display1.showNumberDecEx(val1, 0b01000000, true); // colon on

  // Format MMSS for display2 (Player 2)
  uint8_t m2 = time2 / 60;
  uint8_t s2 = time2 % 60;
  int val2   = m2 * 100 + s2;
  display2.showNumberDecEx(val2, 0b01000000, true);

  // Update LCD second row: "1 MM:SS  2 MM:SS"
  char buf[17];
  snprintf(buf, sizeof(buf), "1 %02u:%02u  2 %02u:%02u", m1, s1, m2, s2);
  lcd.setCursor(0, 1);
  lcd.print(buf);
}
