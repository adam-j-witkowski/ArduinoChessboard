/**
 * Group 61 - Smart Chessboard - Web Server
 * - Seth Traman (stram3, stram3@uic.edu)
 * - Adam Witkowski (awitk, awitk@uic.edu)
 * - Paul Quinto (pquin6, pquin6@uic.edu)
 * 
 * This web server runs on a laptop and it must be on the same Wi-Fi network as the Arduino boards.
 * The Arduino #3 board is going to send the board state to this server, and then this server will
 * forward that data to an interactive web page.
 * 
 * Reference: https://stackoverflow.com/questions/3653065/get-local-ip-address-in-node-js
 * Reference: https://expressjs.com/en/5x/api.html
 * Reference: https://expressjs.com/en/resources/middleware/body-parser.html
 */

import express from "express";
import bodyParser from "body-parser";
import clipboard from "clipboardy";
import path from "path";
import { fileURLToPath } from "url";

import dns from "node:dns";
import os from "node:os";

// Reference: https://stackoverflow.com/questions/3653065/get-local-ip-address-in-node-js
async function getLocalIP() {
  return new Promise((resolve, reject) => {
    dns.lookup(os.hostname(), { family: 4 }, (err, addr) => {
      if (err) {
        reject(err);
      } else {
        resolve(addr);
      }
    });
  });
}

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.urlencoded({ extended: true }));
app.use(express.json());

global.sensorState = new Array(6).fill('0');

// Endpoint for Arduino #3 to update sensor state
app.post("/update-sensors", (req, res) => {
  if (!req.body || !req.body.sensorState) {
    const error = "Bad request: missing sensor state data";
    console.error(error);
    res.status(400).send(error);
    return;
  }

  const sensorValues = req.body.sensorState.split(',');
  
  if (sensorValues.length !== 6) {
    const error = `Invalid sensor state length: ${sensorValues.length} (expected 6)`;
    console.error(error);
    res.status(400).send(error);
    return;
  }
  
  global.sensorState = sensorValues;
  console.log("Updated Sensor State:", global.sensorState);

  res.status(200).send("OK");
});

// Endpoint for web page to get sensor state
app.get("/sensor-state", (req, res) => {

  const sensorArray = global.sensorState.map(v => v === '1' ? 1 : 0);
  
  res.json({
    sensorState: global.sensorState,
    sensorArray: sensorArray
  });
});

// Serve the board visualizer page
app.get("/", (req, res) => {
  console.log("Serving index.html");
  res.sendFile(path.join(__dirname, "index.html"));
});

// Serve the sensor visualizer page
app.get("/sensors", (req, res) => {
  console.log("Serving sensors.html");
  res.sendFile(path.join(__dirname, "sensors.html"));
});

// This is for Arduino #3's keep-alive connection but I'm not sure if it works
app.get("/ping", (req, res) => {
  res.status(200).send("pong");
});

// Start the web server
const port = 3000;
app.listen(port, '0.0.0.0', async () => {
  const ip = await getLocalIP();
  console.log(`Server running at http://${ip}:${port}`);
  console.log('IP copied to clipboard!');
  clipboard.writeSync(ip);
});
