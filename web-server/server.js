import express from "express";
import bodyParser from "body-parser";
import clipboard from "clipboardy";
import path from "path";
import { fileURLToPath } from "url";

import dns from "node:dns";
import os from "node:os";

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

global.boardState = new Array(64).fill(0);
global.binaryBoardState = new Array(64).fill('0');

// Pinged by Arduino #3 to update the board state
app.post("/update", (req, res) => {
  if (!req.body) {
    const error = "Bad request: missing body";
    console.error(error);
    res.status(400).send(error);
    return;
  }

  // Example boardState: RNBKQBNR,PPPPPPPP,        ,        ,        ,        ,pppppppp,rnbkqbnr
  global.boardState = req.body.boardState;
  console.log("Updated Board:", global.boardState);

  res.status(200).send("OK");
});

// Endpoint for binary board state updates
app.post("/update-binary", (req, res) => {
  if (!req.body || !req.body.binaryState) {
    const error = "Bad request: missing binary state data";
    console.error(error);
    res.status(400).send(error);
    return;
  }

  const binaryState = req.body.binaryState;
  
  // Validate that we got exactly 64 characters (8x8 board)
  if (binaryState.length !== 64) {
    const error = `Invalid binary state length: ${binaryState.length} (expected 64)`;
    console.error(error);
    res.status(400).send(error);
    return;
  }
  
  global.binaryBoardState = binaryState;
  console.log("Updated Binary Board:", global.binaryBoardState);

  res.status(200).send("OK");
});

// Allows polling the web server for traditional board state
app.get("/state", (req, res) => {
  res.json({ board: global.boardState });
});

// Endpoint for polling binary board state
app.get("/binary-state", (req, res) => {

  // Convert string to array of 0/1 values for better JSON formatting
  const binaryArray = Array.from(global.binaryBoardState).map(c => c === '1' ? 1 : 0);
  
  // Format the binary state as a 2D array for easier consumption
  const board2D = [];
  for (let i = 0; i < 8; i++) {
    const row = [];
    for (let j = 0; j < 8; j++) {
      const index = i * 8 + j;
      row.push(binaryArray[index]);
    }
    board2D.push(row);
  }
  
  res.json({
    binaryString: global.binaryBoardState,
    binaryArray: binaryArray,
    board2D: board2D
  });
});

// Serve the board visualizer page
app.get("/", (req, res) => {
  console.log("Serving index.html");
  res.sendFile(path.join(__dirname, "index.html"));
});

const port = 3000;
app.listen(port, '0.0.0.0', async () => {
  const ip = await getLocalIP();
  console.log(`Server running at http://${ip}:${port}`);
  console.log('IP copied to clipboard!');
  clipboard.writeSync(ip);
});
