#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include "PicoHM01B0.h"

/* ================= CAMERA ================= */
#define FRAME_W 160
#define FRAME_H 120
uint8_t frame[FRAME_W * FRAME_H];
PicoHM01B0 Cmra;

/* ================= L298N (DIRECT CONTROL) ================= */
// Adjust pins if needed
#define IN1 10
#define IN2 11
#define IN3 12
#define IN4 13
#define ENA 8
#define ENB 9

int dr = 0;

/* ================= SERVO ================= */
Servo camServo;
int servoPin = 15;

/* ================= MIC ================= */
#define MIC_PIN 26

/* ================= WIFI ================= */
const char* ap_ssid = "PicoRobot_AP";
const char* ap_pass = "12345678";
WebServer server(80);

/* ================= MOTOR CONTROL ================= */
void motorInit() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  motorStop();
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void motorSet(int d) {
  switch (d) {
    case 1: // forward
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      break;
    case 2: // backward
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      break;
    case 3: // left
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      break;
    case 4: // right
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      break;
    default:
      motorStop();
  }
}

/* ================= HANDLERS ================= */

void handleCmd() {
  dr = server.arg("d").toInt();
  motorSet(dr);
  server.send(200, "text/plain", "OK");
}

void handleServo() {
  int p = constrain(server.arg("p").toInt(), 0, 180);
  camServo.write(p);
  server.send(200, "text/plain", "OK");
}

void handleStream() {
  WiFiClient client = server.client();

  Cmra.wait_for_frame();
  Cmra.start_capture(frame);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/octet-stream");
  client.println("Connection: close");
  client.println();
  client.write(frame, sizeof(frame));
}

void handleAudio() {
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/octet-stream");
  client.println("Connection: close");
  client.println();

  uint8_t pcm[256];
  for (int i = 0; i < 256; i++) {
    pcm[i] = analogRead(MIC_PIN) >> 4;
    delayMicroseconds(125);
  }
  client.write(pcm, sizeof(pcm));
}
/* ================= WEB UI ================= */
void handleRoot() {
  server.send(200, "text/html", R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Pico W Command Center</title>
    <style>
        :root { --neon-orange: #ff8c00; --deep-purple: #120524; --panel: #25103f; --text: #f5f0ff; --stop: #ff3e3e; }
        body { background: var(--deep-purple); color: var(--text); font-family: sans-serif; margin: 0; display: flex; flex-direction: column; align-items: center; padding: 20px; overflow-x: hidden;}
        h3 { text-transform: uppercase; letter-spacing: 3px; color: var(--neon-orange); text-shadow: 0 0 15px rgba(255,140,0,0.5); margin: 10px 0; }
        .camera-container { border: 3px solid var(--neon-orange); box-shadow: 0 0 20px rgba(255,140,0,0.2); border-radius: 12px; overflow: hidden; background: #000; margin-bottom: 15px; line-height: 0; }
        canvas { display: block; image-rendering: pixelated; width: 320px; height: 240px; }
        .rec-controls { margin-bottom: 20px; display: flex; gap: 10px; }
        .btn-rec { background: transparent; border: 1px solid var(--neon-orange); color: var(--neon-orange); padding: 10px 20px; border-radius: 20px; cursor: pointer; font-weight: bold; transition: 0.3s; }
        .btn-rec.recording { background: var(--stop); border-color: var(--stop); color: white; animation: pulse 1.5s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
        .dashboard { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; max-width: 450px; width: 100%; }
        .panel { background: var(--panel); padding: 15px; border-radius: 12px; border: 1px solid rgba(255,140,0,0.1); }
        .d-pad { display: grid; grid-template-columns: repeat(3, 50px); gap: 10px; justify-content: center; }
        button.move { width: 50px; height: 50px; border: 2px solid var(--neon-orange); border-radius: 8px; background: transparent; color: var(--neon-orange); font-size: 20px; }
        button.move:active { background: var(--neon-orange); color: var(--deep-purple); }
        .btn-stop-m { border-color: var(--stop); color: var(--stop); }
        input[type="range"] { width: 100%; accent-color: var(--neon-orange); margin: 10px 0; }
        .status { margin-top: 20px; font-size: 11px; font-family: monospace; color: var(--neon-orange); }
    </style>
</head>
<body>
    <h3>Pico W Rover</h3>
    <div class="camera-container"><canvas id="cam" width="160" height="120"></canvas></div>
    <div class="rec-controls">
        <button id="recBtn" class="btn-rec" onclick="toggleRecording()">⏺ START RECORDING</button>
    </div>
    <div class="dashboard">
        <div class="panel">
            <div class="d-pad">
                <div></div><button class="move" onclick="cmd(1)">↑</button><div></div>
                <button class="move" onclick="cmd(3)">←</button><button class="move btn-stop-m" onclick="cmd(0)">■</button><button class="move" onclick="cmd(4)">→</button>
                <div></div><button class="move" onclick="cmd(2)">↓</button><div></div>
            </div>
        </div>
        <div class="panel">
            <label style="font-size:px">THRUST</label><input type="range" min="0" max="255" value="150" oninput="fetch('/speed?v='+this.value)">
            <label style="font-size:10px">GIMBAL</label><input type="range" min="0" max="180" value="90" oninput="fetch('/servo?p='+this.value)">
        </div>
    </div>
    <div class="status" id="stat">SYSTEM READY</div>

    <script>
        const canvas = document.getElementById("cam");
        const ctx = canvas.getContext("2d");
        let recorder, chunks = [], audioCtx, dest, isRecording = false;

        async function updateCam() {
            try {
                const r = await fetch("/stream?t=" + Date.now());
                if (r.ok) {
                    const buf = new Uint8Array(await r.arrayBuffer());
                    const img = ctx.createImageData(160, 120);
                    for(let i=0; i<buf.length; i++){
                        img.data[i*4+0] = img.data[i*4+1] = img.data[i*4+2] = buf[i];
                        img.data[i*4+3] = 255;
                    }
                    ctx.putImageData(img, 0, 0);
                }
            } catch (e) {}
            setTimeout(updateCam, 30);
        }

        /* ================= AUDIO & RECORDING ================= */
        async function toggleRecording() {
            if (!isRecording) {
                // Initialize Audio Context on user gesture
                if(!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)({sampleRate: 8000});
                dest = audioCtx.createMediaStreamDestination();
                
                const videoStream = canvas.captureStream(30);
                const combinedStream = new MediaStream([...videoStream.getTracks(), ...dest.stream.getTracks()]);
                
                recorder = new MediaRecorder(combinedStream, { mimeType: 'video/webm;codecs=vp8,opus' });
                chunks = [];
                recorder.ondataavailable = e => chunks.push(e.data);
                recorder.onstop = exportVideo;
                
                recorder.start();
                isRecording = true;
                document.getElementById('recBtn').innerText = "⏹ STOP & DOWNLOAD";
                document.getElementById('recBtn').classList.add('recording');
                startAudioPump();
            } else {
                recorder.stop();
                isRecording = false;
                document.getElementById('recBtn').innerText = "⏺ START RECORDING";
                document.getElementById('recBtn').classList.remove('recording');
            }
        }

        // Pulls audio from Pico and pushes to the recorder stream
        async function startAudioPump() {
            while(isRecording) {
                try {
                    const r = await fetch("/audio");
                    const buf = await r.arrayBuffer();
                    const data = new Uint8Array(buf);
                    const audioBuf = audioCtx.createBuffer(1, data.length, 8000);
                    const channel = audioBuf.getChannelData(0);
                    for(let i=0; i<data.length; i++) channel[i] = (data[i] - 128) / 128;
                    const source = audioCtx.createBufferSource();
                    source.buffer = audioBuf;
                    source.connect(dest);
                    source.start();
                } catch(e) {}
                await new Promise(r => setTimeout(r, 32)); // Match 8kHz buffer size timing
            }
        }

        function exportVideo() {
            const blob = new Blob(chunks, { type: "video/webm" });
            const url = URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            a.download = `PicoRover_${Date.now()}.webm`;
            a.click();
        }

        function cmd(d){ fetch('/cmd?d='+d); }
        updateCam();
    </script>
</body>
</html>
)HTML");
}

/* ================= SETUP / LOOP ================= */
void setup() {
  Serial.begin(115200);

  motorInit();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  PicoHM01B0_config cfg;
  cfg.i2c_dat_gpio = 4;
  cfg.i2c_clk_gpio = 5;
  cfg.vsync_gpio   = 16;
  cfg.pclk_gpio    = 14;
  cfg.mclk_gpio    = 3;
  cfg.d0_gpio      = 6;
  cfg.bus_4bit = false;
  cfg.flip_vertical = false;
  cfg.flip_horizontal = true;

  Cmra.begin(cfg);
  Cmra.start_streaming(15, false, false);
  Cmra.start_capture(frame);

  camServo.attach(servoPin);
  analogReadResolution(12);

  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/servo", handleServo);
  server.on("/stream", handleStream);
  server.on("/audio", handleAudio);
  server.begin();

  Serial.println("READY");

  motorSet(1);
  delay(2000);
}

void loop() {
  server.handleClient();
}
