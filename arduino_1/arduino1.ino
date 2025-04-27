/**
 * Group 61 - Smart Chessboard - Arduino #1
 * - Seth Traman (stram3, stram3@uic.edu)
 * - Adam Witkowski (awitk, awitk@uic.edu)
 * - Paul Quinto (pquin6, pquin6@uic.edu)
 * 
 * Arduino #1 is responsible for reading the state of the hall-effect sensors
 * and sending that data to Arduino #3 over serial. 
 * 
 * We send 16 '1' or '0' characters depending on if a magnetic piece is on a square or not
 * The characters are sent in the order:
 * 1  2  3  4 
 * 5  6  7  8 
 * 9  10 11 12
 * 13 14 15 16
 *
 * Followed by '\n' to indicate the end of a message.
 *
 * Reference: https://electronoobs.com/eng_arduino_tut82.php
 * Reference: https://docs.arduino.cc/language-reference/en/functions/communication/serial/
 * Reference: https://learn.sparkfun.com/tutorials/multiplexer-breakout-hookup-guide/74hc4051-breakout-overview
 * Reference: https://forum.arduino.cc/t/coding-74hc4051-multiplexing/298069
 */

// millis() variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 500;

// 2D array representing the squares each input pin is reading from
const int pinGrid[4][4] = {
  {13, 9, 5, A5},
  {12, 8, 4, A4},
  {11, 7, 3, A3},
  {10, 6, 2, A2}
};

// Set each pin used as input
void setup() {
  Serial.begin(9600);
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      pinMode(pinGrid[row][col], INPUT);
    }
  }
}

void loop() {
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    
    // Sending board data as single characters
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
        int value = digitalRead(pinGrid[row][col]);
        
        if (value == 0) {
          Serial.write('1'); // Magnet detected
        } else {
          Serial.write('0'); // No magnet
        }
      }
    }

    Serial.write('\n'); // Sending newline after full board
  }
}