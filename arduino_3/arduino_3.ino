#include <WiFiS3.h>
#include <WiFiClient.h>

const char *ssid = "Traman Net";
const char *password = "frogbones";

const char *serverIP = "10.41.183.81";
const int serverPort = 3000;

// Constants for serial communication with arduino #1
const char MSG_BEGIN = '<';
const char MSG_END = '>';
const int BOARD_SIZE = 8;

// Timer intervals
const unsigned long WIFI_CHECK_INTERVAL = 500;   // WiFi connection check interval in ms
const unsigned long PROCESS_INTERVAL = 100;      // Main processing interval in ms
unsigned long lastWifiCheckTime = 0;
unsigned long lastProcessTime = 0;

// Binary board state representation
bool binaryBoardState[8][8] = {};

// Character board state representation
static char boardState[8][8] = {
    {'R', 'N', 'B', 'K', 'Q', 'B', 'N', 'R'},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
    {'r', 'n', 'b', 'k', 'q', 'b', 'n', 'r'}};

char serialBuffer[70]; // Buffer to hold board state data (64 cells + markers)
int bufferIndex = 0;
bool messageStarted = false;
bool boardUpdated = false;
bool wifiConnected = false;

void setup()
{
  Serial.begin(9600); // For talking to the serial console
  Serial1.begin(9600); // For reading from arduino #1

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  lastWifiCheckTime = millis();
}

void loop()
{
  unsigned long currentMillis = millis();

  if (!wifiConnected) {
    if (currentMillis - lastWifiCheckTime >= WIFI_CHECK_INTERVAL) {
      lastWifiCheckTime = currentMillis;
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
      } else {
        Serial.print(".");
      }
    }
  }
  
  readBoardStateFromSerial();

  // Process board updates at regular intervals
  if (currentMillis - lastProcessTime >= PROCESS_INTERVAL) {
    lastProcessTime = currentMillis;
    
    // If we have just finished reading a new board state, send it to the server
    if (boardUpdated && wifiConnected) {
      sendBoardStateToServer();
      boardUpdated = false;
    }
  }
}

void readBoardStateFromSerial()
{
  while (Serial1.available() > 0)
  {
    char c = Serial1.read();

    if (c == MSG_BEGIN)
    {
      // We found the start of a new message
      messageStarted = true;
      bufferIndex = 0;
    }
    else if (c == MSG_END && messageStarted)
    {
      // We found the end of an existing message
      messageStarted = false;

      // Read through all the data we've accumulated so far
      if (bufferIndex == 64)
      {
        for (int row = 0; row < 8; row++)
        {
          for (int col = 0; col < 8; col++)
          {
            int index = row * 8 + col;
            // Convert '0'/'1' characters to boolean values
            binaryBoardState[row][col] = (serialBuffer[index] == '1');
          }
        }
        boardUpdated = true;
      }
    }
    else if (messageStarted && bufferIndex < 64)
    {
      // Middle of message, just accumulate the data
      serialBuffer[bufferIndex++] = c;
    }
  }
}

void sendBoardStateToServer()
{
  Serial.println("\nConnecting to server...");

  WiFiClient client;

  // Send the textual representation of the board state
  if (client.connect(serverIP, serverPort))
  {
    Serial.println("Connected to server");

    // Header
    String httpRequest = "POST /update HTTP/1.1\r\n";
    httpRequest += "Host: ";
    httpRequest += serverIP;
    httpRequest += "\r\n";
    httpRequest += "Content-Type: application/x-www-form-urlencoded\r\n";

    // Body
    String httpBody = "boardState=";
    for (int i = 0; i < 8; i++)
    {
      for (int j = 0; j < 8; j++)
      {
        httpBody += boardState[i][j];
      }
      if (i < 7)
      {
        httpBody += ",";
      }
    }

    // More headers
    httpRequest += "Content-Length: ";
    httpRequest += httpBody.length();
    httpRequest += "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";

    // Attach body
    httpRequest += httpBody;

    Serial.println("Sending POST request:");
    Serial.println(httpRequest);
    client.print(httpRequest);

    // Print the server's response
    Serial.println("\nReceiving response:");
    while (client.available())
    {
      String line = client.readStringUntil('\r\n');
      Serial.println(line);
    }

    client.stop();

    // Send the binary representation of the board state
    if (client.connect(serverIP, serverPort))
    {
      // Header
      String binaryRequest = "POST /update-binary HTTP/1.1\r\n";
      binaryRequest += "Host: ";
      binaryRequest += serverIP;
      binaryRequest += "\r\n";
      binaryRequest += "Content-Type: application/x-www-form-urlencoded\r\n";

      // Body
      String binaryBody = "binaryState=";
      for (int i = 0; i < 8; i++)
      {
        for (int j = 0; j < 8; j++)
        {
          binaryBody += binaryBoardState[i][j] ? '1' : '0';
        }
      }

      // More headers
      binaryRequest += "Content-Length: ";
      binaryRequest += binaryBody.length();
      binaryRequest += "\r\n";
      binaryRequest += "Connection: close\r\n";
      binaryRequest += "\r\n";

      // Attach body
      binaryRequest += binaryBody;

      Serial.println("Sending binary POST request:");
      Serial.println(binaryRequest);
      client.print(binaryRequest);

      // Read response
      while (client.available())
      {
        String line = client.readStringUntil('\r\n');
        Serial.println(line);
      }

      Serial.println("\nClosing connection.");
      client.stop();
    }
  }
  else
  {
    Serial.println("Failed to connect to server.");
  }
}
