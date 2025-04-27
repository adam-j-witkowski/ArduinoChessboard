// 2D array representing the squares each input pin is reading from
const int pinGrid[4][4] = {
  {13, 9, 5, A5},
  {12, 8, 4, A4},
  {11, 7, 3, A3},
  {10, 6, 2, A2}
};

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 1500;

void setup() {
  Serial.begin(9600);
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      pinMode(pinGrid[row][col], INPUT);
    }
  }
}

void loop() {
  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
    // Printing visual representation of board
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
        int value = digitalRead(pinGrid[row][col]);

        Serial.print("| ");
        if (value == 0) {
          Serial.print("X ");
        } else {
          Serial.print("  ");
        }
      }
      Serial.println("|");
    }

    Serial.println(); // Extra line between full board prints
  }
}
