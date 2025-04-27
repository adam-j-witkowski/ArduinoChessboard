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
 */

#include <WiFiS3.h>
#include <WiFiClient.h>

const char *ssid = "Traman Net";       // Wi-FI network name
const char *password = "frogbones";    // Wi-Fi network password
const char *serverIP = "10.41.183.81"; // Web server IP address (should be auto-copied on 'npm start')
const int serverPort = 3000;           // Web server port (should be constant)

const int MAX_SENSORS = 32; // Most amount of sensors that could ever be connected to Arduino #1

const unsigned long WIFI_CHECK_INTERVAL = 500; // WiFi connection check interval in ms
const unsigned long PROCESS_INTERVAL = 50;     // Main processing interval in ms (now much faster)
const unsigned long RECONNECT_INTERVAL = 5000; // How often to check the server connection

int sensorCount = 0;                             // Will be determined from Arduino #1's messages
bool sensorState[MAX_SENSORS] = {false};         // Array of booleans for magnet present/absent
bool previousSensorState[MAX_SENSORS] = {false}; // To track changes

WiFiClient client;
bool dataUpdated = false;
bool wifiConnected = false;
bool serverConnected = false;

void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
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

      // AND we are already connected, OR we are able to hotfix our connection on-the-spot....
      if (serverConnected || ensureServerConnection()) {
        // ...then send the data.
        sendSensorDataToServer();
      }
      dataUpdated = false;
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

    // Process each character based on arduino_1's new protocol
    // Arduino #1 now sends '1'/'0' for each sensor followed by '\n'
    if (c == '\n')
    {
      // End of message, process the collected data
      if (bufferIndex > 0) {
        // Add null terminator to make it a proper string
        serialBuffer[bufferIndex] = '\0';
        
        // Set the number of sensors based on the message length
        sensorCount = bufferIndex;
        
        if (sensorCount > 0 && sensorCount <= MAX_SENSORS) {
          // Parse each character in the buffer ('1' or '0') into our sensor state array
          for (int i = 0; i < sensorCount; i++) {
            sensorState[i] = (serialBuffer[i] == '1');
          }
          
          dataUpdated = true;
        }
        
        // Reset the buffer for the next message
        bufferIndex = 0;
        receivingMessage = false;
      }
    }
    else if (c == '1' || c == '0')
    {
      // If it's a valid sensor state character, add it to our buffer
      if (bufferIndex < sizeof(serialBuffer) - 1) {
        serialBuffer[bufferIndex++] = c;
        receivingMessage = true;
      }
    }
    // Ignore any other characters that aren't '0', '1', or '\n'
  }
}

void sendSensorDataToServer()
{
  // skip obvious failure cases
  if (!serverConnected || sensorCount == 0) {
    return;
  }

  // Construct our body first, since we need its length to make the headers
  char httpBody[128];
  strcpy(httpBody, "sensorState=");
  int offset = strlen(httpBody);
  for (int i = 0; i < sensorCount; i++) {
    httpBody[offset++] = sensorState[i] ? '1' : '0';
    if (i < sensorCount - 1) {
      httpBody[offset++] = ',';
    }
  }
  httpBody[offset] = '\0';
  
  client.print("POST /update-sensors HTTP/1.1\r\nHost: ");
  client.print(serverIP);
  client.print("\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ");
  client.print(offset);
  client.print("\r\nConnection: keep-alive\r\n\r\n");
  client.print(httpBody);
  client.flush();
  
  // Ignore the response, we don't really care lol
  while (client.available()) {
    client.read();
  }
}
