/**
 * Group 61 - Smart Chessboard - Arduino #3
 * - Seth Traman (stram3, stram3@uic.edu)
 * - Adam Witkowski (awitk, awitk@uic.edu)
 * - Paul Quinto (pquin6, pquin6@uic.edu)
 * 
 * Arduino #3 is responsible for connecting to the Wi-Fi network and receiving sensor data
 * from Arduino #1 over serial.  It then forwards that data to a web server on the same Wi-Fi
 * network using HTTP POST requests, attempting to use HTTP keep-alive to maintain a persistent
 * connection to the server.
 *
 * For serial communication with Arduino #1, see arduino_1.ino for details.
 * 
 * Reference: https://docs.arduino.cc/libraries/wifi/
 * Reference: https://docs.arduino.cc/language-reference/en/functions/communication/serial/
 * Reference: https://stackoverflow.com/questions/20763999/explain-http-keep-alive-mechanism
 * Reference: https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Methods/POST
 * Reference: https://docs.arduino.cc/built-in-examples/digital/Button/
 */

#include <WiFiS3.h>
#include <WiFiClient.h>

const char *ssid = "Traman Net";       // Wi-FI network name
const char *password = "frogbones";    // Wi-Fi network password
const char *serverIP = "10.41.183.81"; // Web server IP address (should be auto-copied on 'npm start')
const int serverPort = 3000;           // Web server port (should be constant)

const int MAX_SENSORS = 32; // Most amount of sensors that could ever be connected to Arduino #1
const int BOARD_SIZE = 4;   // 4x4 Reversi board

// Reversi board states
const int EMPTY = 0;
const int BLACK = 1;  // Black always goes first
const int WHITE = 2;

const unsigned long WIFI_CHECK_INTERVAL = 500; // WiFi connection check interval in ms
const unsigned long PROCESS_INTERVAL = 50;     // Main processing interval in ms (now much faster)
const unsigned long RECONNECT_INTERVAL = 5000; // How often to check the server connection
const unsigned long BOARD_UPDATE_INTERVAL = 1000; // How often to send board state to server

int sensorCount = 0;                             // Will be determined from Arduino #1's messages
bool sensorState[MAX_SENSORS] = {false};         // Array of booleans for magnet present/absent
bool previousSensorState[MAX_SENSORS] = {false}; // To track changes

const int BUTTON_PIN = 2;

// Reversi game state
int board[BOARD_SIZE][BOARD_SIZE] = {0}; // Board representation (0=empty, 1=black, 2=white)
int currentPlayer = BLACK;               // Black goes first
bool gameOver = false;                   // Flag for game over state
int winner = EMPTY;                      // Winner of the game (BLACK, WHITE, or EMPTY for a tie)

WiFiClient client;
bool dataUpdated = false;
bool wifiConnected = false;
bool serverConnected = false;

void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  initializeReversiBoard();
}

// Try and connect to the server, returning 'true' on success and 'false' on failure
bool ensureServerConnection() {

  // Edge case: we think we're connected, but the client says otherwise
  if (serverConnected && !client.connected()) {
    Serial.println("Connection lost, reconnecting...");
    serverConnected = false;
    client.stop();
  }
  
  // Try and connect, since we aren't connected
  if (!serverConnected) {
    Serial.println("Connecting to server...");
    if (client.connect(serverIP, serverPort)) {
      Serial.println("Connected to server");
      serverConnected = true;
      
      // The connection: keep-alive is supposed to allow us to send faster
      // messages to the server by avoiding the need to constantly re-create
      // an HTTP connection for every message we send.  But, it will time out
      // and die without activity, so I'm using a /ping endpoint to make sure
      // there's always some traffic that keeps the connection alive.
      client.println("GET /ping HTTP/1.1");
      client.println("Host: " + String(serverIP));
      client.println("Connection: keep-alive");
      client.println();
      
      // I don't care what the server says in response, just read it into the void xD
      while (client.available()) {
        client.read();
      }
    } else {
      // Oopsies
      Serial.println("Failed to connect to server");
      return false;
    }
  }
  
  return serverConnected;
}

void loop()
{
  static unsigned long lastWifiCheckTime = millis();
  static unsigned long lastProcessTime = millis();
  static unsigned long lastConnectionCheckTime = millis();
  static unsigned long lastBoardUpdateTime = millis();
  static bool lastButtonState = HIGH; // Using pull-up resistor, so HIGH is unpressed

  // Detect button activity by comparing previous state with current state
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    Serial.println("Reset button pressed - reinitializing board");
    initializeReversiBoard();
    
    // We want the server to become aware immediately
    if (wifiConnected && (serverConnected || ensureServerConnection())) {
      sendBoardStateToServer();
    }
  }
  // Remember the current state for next time
  lastButtonState = currentButtonState;

  // Re-connect to the internet if necessary
  if (!wifiConnected && millis() - lastWifiCheckTime >= WIFI_CHECK_INTERVAL) {
    lastWifiCheckTime = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      // We managed to connect, track that in our global state and print some debug info
      wifiConnected = true;
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      // Just print something so I know the board isn't idle
      Serial.print(".");
    }
  }
  
  readSensorDataFromSerial();

  // I'm trying to use HTTP keepalive to ensure the connection stays open and we don't
  // have to close and re-open the connection everytime.  This condition basically checks
  // in on the server connection every RECONNECT_INTERVAL ms just in case the keepalive
  // connection got killed and we need to re-establish the connection
  if (wifiConnected && millis() - lastConnectionCheckTime >= RECONNECT_INTERVAL) {
    lastConnectionCheckTime = millis();
    ensureServerConnection();
  }

  // This is our kind of 'core processing loop' where we check if a full data frame has
  // been received from Arduino #1 and attempt to forward that data to the web server.
  if (millis() - lastProcessTime >= PROCESS_INTERVAL) {
    lastProcessTime = millis();
    
    // If data is ready, AND the wifi is connected,
    if (dataUpdated && wifiConnected) {
      // Process game board based on sensor data
      processSensorData();

      dataUpdated = false;
    }
  }
  
  // Periodically send board state to server
  if (wifiConnected && millis() - lastBoardUpdateTime >= BOARD_UPDATE_INTERVAL) {
    lastBoardUpdateTime = millis();
    
    if (serverConnected || ensureServerConnection()) {
      sendBoardStateToServer();
    }
  }
}

void readSensorDataFromSerial()
{
  static char serialBuffer[64];
  static int bufferIndex = 0;
  static bool receivingMessage = false;

  // We want to repeatedly read bytes until we can't anymore
  while (Serial1.available() > 0)
  {
    char c = Serial1.read();

    /// New line indicates end of message
    if (c == '\n')
    {
      if (bufferIndex > 0) {

        // bufferIndex conveniently stores how many sensors we've read the states from so far
        serialBuffer[bufferIndex] = '\0';
        sensorCount = bufferIndex;
        
        if (sensorCount > 0 && sensorCount <= MAX_SENSORS) {
          // Copy the data we've accumulated from this message so far into our sensorState array
          for (int i = 0; i < sensorCount; i++) {
            sensorState[i] = (serialBuffer[i] == '1');
          }
          
          dataUpdated = true;
        }
        
        // since we reached the end, reset tracking vars for next incoming message
        bufferIndex = 0;
        receivingMessage = false;
      }
    }
    else if (c == '1' || c == '0')
    {
      // we got a sensor state in the middle of the message, write it into our message buffer
      if (bufferIndex < sizeof(serialBuffer) - 1) {
        serialBuffer[bufferIndex++] = c;
        receivingMessage = true;
      }
    }
  }
}

// This sets/resets the board state to a healthy initial condition, both when the
// chip powers up and also when the 'reset' button is pressed
void initializeReversiBoard() {
  // Clear the board
  for (int row = 0; row < BOARD_SIZE; row++) {
    for (int col = 0; col < BOARD_SIZE; col++) {
      board[row][col] = EMPTY;
    }
  }
  
  // Set up the initial four pieces in the center
  int center = BOARD_SIZE / 2;
  board[center-1][center-1] = WHITE;
  board[center-1][center] = BLACK;
  board[center][center-1] = BLACK;
  board[center][center] = WHITE;
  
  currentPlayer = BLACK;
  gameOver = false;
  winner = EMPTY;
  
  // Mark the board as updated so it will be sent to the server
  dataUpdated = true;
  
  Serial1.write('S');
  Serial.println("Reversi board initialized");
}

// Process sensor data to update the game board
void processSensorData() {
  // Skip if we don't have enough sensor data
  if (sensorCount != BOARD_SIZE * BOARD_SIZE) {
    return;
  }
  
  // Don't process moves if the game is over
  if (gameOver) {
    // Just update previous state for next comparison
    for (int i = 0; i < sensorCount; i++) {
      previousSensorState[i] = sensorState[i];
    }
    return;
  }
  
  // Check for any new pieces that have been added
  for (int i = 0; i < sensorCount; i++) {
    int row = i / BOARD_SIZE;
    int col = i % BOARD_SIZE;
    
    // A new piece was added (sensor now active but wasn't before)
    if (sensorState[i] && !previousSensorState[i]) {
      // Only process the move if it's a valid empty square
      if (board[row][col] == EMPTY) {
        // Place the piece of the current player
        board[row][col] = currentPlayer;
        Serial.print("New piece at row ");
        Serial.print(row);
        Serial.print(", col ");
        Serial.print(col);
        Serial.print(" for player ");
        Serial.println(currentPlayer == BLACK ? "BLACK" : "WHITE");

        Serial1.write('M');
        
        // Flip opponent's pieces
        flipPieces(row, col);
        
        // Check if the game is over after this move
        checkGameOver();
        
        // If game not over, switch to the other player
        if (!gameOver) {
          currentPlayer = (currentPlayer == BLACK) ? WHITE : BLACK;
        }
      }
    }
    
    // Update previous state for next comparison
    previousSensorState[i] = sensorState[i];
  }
}

// This updates the Reversi game state in response to a new piece being added to the board
void flipPieces(int row, int col) {
  
  int opponent = (currentPlayer == BLACK) ? WHITE : BLACK;
  
  // dRow and dCol are relative offsets to the current row and col,
  // so we can explore in all 8 directions
  for (int dRow = -1; dRow <= 1; dRow++) {
    for (int dCol = -1; dCol <= 1; dCol++) {

      // this is our piece (no offset)
      if (dRow == 0 && dCol == 0) continue;
      
      // Check if there's a valid line to flip
      if (canFlip(row, col, dRow, dCol)) {
        flipInDirection(row, col, dRow, dCol);
      }
    }
  }
}

// This starts at row and col and progressively increments by dRow and dCol
// basically 'scanning' from a starting position towards a certain direction
// until it finds a piece of our color, which means YES we are eligible to flip
// some pieces in this direction, otherwise NO because we don't enclose any
// cells in this direction.
bool canFlip(int row, int col, int dRow, int dCol) {
  int opponent = (currentPlayer == BLACK) ? WHITE : BLACK;
  int r = row + dRow;
  int c = col + dCol;
  
  // Stop if we immediately hit a boundary or hit our own piece
  if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE || board[r][c] != opponent) {
    return false;
  }
  
  // Move forward again, and then...
  r += dRow;
  c += dCol;
  
  // Keep going until we hit a boundary or an empty space
  while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
    if (board[r][c] == EMPTY) return false;
    if (board[r][c] == currentPlayer) return true;
    
    r += dRow;
    c += dCol;
  }
  
  return false;
}

// This is supposed to be used after 'canFlip' which tells us whether
// we will eventually find a piece of our own color by starting at row
// and col and continuing to 'walk' in a certain direction.  It will
// flip all the pieces of our oponent's color until we reach our own
// color.
void flipInDirection(int row, int col, int dRow, int dCol) {
  int r = row + dRow;
  int c = col + dCol;
  
  while (board[r][c] != currentPlayer) {
    board[r][c] = currentPlayer;
    r += dRow;
    c += dCol;
  }
}

// Check if the game is over
void checkGameOver() {
  // Game is over if the board is full
  bool boardFull = true;
  
  for (int row = 0; row < BOARD_SIZE; row++) {
    for (int col = 0; col < BOARD_SIZE; col++) {
      if (board[row][col] == EMPTY) {
        boardFull = false;
        break;
      }
    }
    if (!boardFull) break;
  }
  
  if (boardFull) {
    gameOver = true;
    Serial.println("Game over - board is full");
    determineWinner();
  }
}

// Determine the winner based on piece count
void determineWinner() {
  int blackCount = 0;
  int whiteCount = 0;
  
  // Count pieces for each player
  for (int row = 0; row < BOARD_SIZE; row++) {
    for (int col = 0; col < BOARD_SIZE; col++) {
      if (board[row][col] == BLACK) {
        blackCount++;
      } else if (board[row][col] == WHITE) {
        whiteCount++;
      }
    }
  }
  
  Serial.print("Game over - Black: ");
  Serial.print(blackCount);
  Serial.print(", White: ");
  Serial.println(whiteCount);
  
  // Set the winner
  if (blackCount > whiteCount) {
    winner = BLACK;
    Serial.println("Black wins!");
    Serial1.write('B');
  } else if (whiteCount > blackCount) {
    winner = WHITE;
    Serial.println("White wins!");
    Serial1.write('W');
  } else {
    winner = EMPTY;
    Serial.println("It's a tie!");
  }
}

// Send the current board state to the web server
void sendBoardStateToServer() {
  if (!serverConnected) {
    return;
  }
  
  // Construct the HTTP body with board state
  char httpBody[512];
  strcpy(httpBody, "boardState=");
  int offset = strlen(httpBody);
  
  for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
    int row = i / BOARD_SIZE;
    int col = i % BOARD_SIZE;
    
    httpBody[offset++] = '0' + board[row][col]; // Convert 0,1,2 to '0','1','2'
    
    if (i < (BOARD_SIZE * BOARD_SIZE - 1)) {
      httpBody[offset++] = ',';
    }
  }
  
  // Add current player
  offset += sprintf(httpBody + offset, "&currentPlayer=%d", currentPlayer);
  
  // Add game status data
  offset += sprintf(httpBody + offset, "&gameOver=%d&winner=%d", gameOver ? 1 : 0, winner);
  
  httpBody[offset] = '\0';
  
  // Send the HTTP request
  client.print("POST /update-board HTTP/1.1\r\nHost: ");
  client.print(serverIP);
  client.print("\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ");
  client.print(offset);
  client.print("\r\nConnection: keep-alive\r\n\r\n");
  client.print(httpBody);
  client.flush();
  
  // Ignore the response
  while (client.available()) {
    client.read();
  }
  
  Serial.println("Board state sent to server");
}
