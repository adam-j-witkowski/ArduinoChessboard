const unsigned long SEND_INTERVAL = 1000; // Time between transmissions in milliseconds
unsigned long lastSendTime = 0;
const char MSG_BEGIN = '<';
const char MSG_END = '>';

// 8x8 array of booleans
bool chessboardState[8][8] = {
  {1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1}
};

void setup() {
  Serial.begin(9600); // Initialize the serial port
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check if it's time to send data
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = currentTime;
    sendChessboardState();
  }
}

void sendChessboardState() {
  // Send message begin marker
  Serial.write(MSG_BEGIN);
  
  // Send board state
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      Serial.write(chessboardState[row][col] ? '1' : '0');
    }
  }
  
  // Send message end marker
  Serial.write(MSG_END);
}