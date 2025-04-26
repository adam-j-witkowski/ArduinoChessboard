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

// Allows polling the web server for board state
app.get("/state", (req, res) => {
  res.json({ board: global.boardState });
});

// Serve the HTML page on the root route
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
