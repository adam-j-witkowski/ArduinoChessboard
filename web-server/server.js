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

global.sensorState = new Array(16).fill('0'); // 4x4 board
global.boardState = new Array(16).fill('0'); // 0=empty, 1=black, 2=white
global.currentPlayer = 1; // 1=black, 2=white

function initializeReversiBoard() {
  global.boardState = new Array(16).fill('0');
  
  // const centerOffsetA = 1; // one away from left or top
  // const centerOffsetB = 2; // two away from left or top // TODO should probably calculate this based on boardSize
  // const boardSize = 4;
  // global.boardState[centerOffsetA * boardSize + centerOffsetA] = '2';
  // global.boardState[centerOffsetA * boardSize + centerOffsetB] = '1';
  // global.boardState[centerOffsetB * boardSize + centerOffsetA] = '1'; 
  // global.boardState[centerOffsetB * boardSize + centerOffsetB] = '2';
  
  global.currentPlayer = 1;
  
  console.log("Reversi board initialized");
}

// Initialize the board at startup
initializeReversiBoard();

// Endpoint for Arduino #3 to update board state
app.post("/update-board", (req, res) => {
  if (!req.body || !req.body.boardState) {
    const error = "Bad request: missing board state data";
    console.error(error);
    res.status(400).send(error);
    return;
  }

  const boardValues = req.body.boardState.split(',');
  
  if (boardValues.length !== 16) {
    const error = `Invalid board state length: ${boardValues.length} (expected 16)`;
    console.error(error);
    res.status(400).send(error);
    return;
  }
  
  global.boardState = boardValues;
  
  // Update current player if provided
  if (req.body.currentPlayer) {
    global.currentPlayer = parseInt(req.body.currentPlayer);
  }
  
  console.log("Updated Board State");
  console.log("Current Player:", global.currentPlayer === 1 ? "BLACK" : "WHITE");

  res.status(200).send("OK");
});

// Endpoint for web page to get board state
app.get("/board-state", (req, res) => {
  const boardArray = global.boardState.map(v => parseInt(v));
  
  res.json({
    boardState: global.boardState,
    boardArray: boardArray,
    currentPlayer: global.currentPlayer
  });
});

// Serve the board visualizer page
app.get("/", (req, res) => {
  console.log("Serving index.html");
  res.sendFile(path.join(__dirname, "index.html"));
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
