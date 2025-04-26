#include <WiFiS3.h>
#include <WiFiClient.h>

const char* ssid = "Traman Net";
const char* password = "frogbones";

const char* serverIP = "10.41.183.81";
const int serverPort = 3000;

static char boardState[8][8] = {
  {'R', 'N', 'B', 'K', 'Q', 'B', 'N', 'R'},
  {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
  {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
  {'r', 'n', 'b', 'k', 'q', 'b', 'n', 'r'}
};

void setup() {
  Serial.begin(9600);
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  Serial.println("\nConnecting to server...");

  WiFiClient client;
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Connected to server");

    // Construct the POST request
    String httpRequest = "POST /update HTTP/1.1\r\n";
    httpRequest += "Host: ";
    httpRequest += serverIP;
    httpRequest += "\r\n";
    httpRequest += "Content-Type: application/x-www-form-urlencoded\r\n"; // Indicate the body format

    // Construct the body of the POST request
    String httpBody = "boardState=";
    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
        httpBody += boardState[i][j];
      }
      if (i < 7) {
        httpBody += ","; // Add a separator between rows if needed on the server-side
      }
    }

    httpRequest += "Content-Length: ";
    httpRequest += httpBody.length();
    httpRequest += "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";
    httpRequest += httpBody; // Add the body to the request

    Serial.println("Sending POST request:");
    Serial.println(httpRequest);
    client.print(httpRequest);

    // Read the server's response
    Serial.println("\nReceiving response:");
    while (client.available()) {
      String line = client.readStringUntil('\r\n');
      Serial.println(line);
    }

    Serial.println("\nClosing connection.");
    client.stop();
  } else {
    Serial.println("Failed to connect to server.");
  }

  // Wait a few seconds before sending another request (optional)
  delay(5000);
}
