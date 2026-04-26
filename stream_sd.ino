#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"                // File system
#include "SD_MMC.h"            // SD card (MMC interface)
#include <Preferences.h>       // Persistent storage for picture counter

/* WiFi credentials */
#define WIFI_SSID "Realme 8 5G"
#define WIFI_PASS "0987654321"

/* Web server on port 80 */
WebServer server(80);

/* Flash LED pin */
#define FLASH_PIN 4

/* Camera pins - AI Thinker ESP32-CAM */
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* Global variables */
Preferences prefs;
unsigned long pictureNumber = 0;  // Counter for saved photos

/* ================= HTML UI ================= */
String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{margin:0;font-family:'Segoe UI',sans-serif;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:white;text-align:center;}
.card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);margin:15px;padding:15px;border-radius:15px;box-shadow:0 0 20px rgba(0,0,0,0.3);}
img{width:100%;max-width:380px;border-radius:12px;border:2px solid #00ffc8;}
button{padding:14px 25px;font-size:18px;background:linear-gradient(45deg,#00ffc8,#00aaff);color:black;border:none;border-radius:30px;cursor:pointer;transition:0.3s;font-weight:bold;}
button:hover{transform:scale(1.05);box-shadow:0 0 15px #00ffc8;}
h2{color:#00ffc8;font-size:28px;margin:15px 0;}
#status{margin-top:10px;color:#00ffcc;}
</style>
</head>

<body>

<h2>SURVEILLANCE VIEW</h2>

<div class="card">
  <h3>Live Stream</h3>
  <img src="/stream">
</div>

<div class="card">
  <button onclick="capture()">Capture & Save Image</button>
  <p id="status">Ready</p>
</div>

<div class="card">
  <h3>Captured Image</h3>
  <img id="cap" src="">
</div>

<script>
function capture(){
  document.getElementById("status").innerHTML = "Capturing & Saving...";
  
  const ts = Date.now();
  document.getElementById("cap").src = "/capture?t=" + ts;
  
  setTimeout(() => {
    document.getElementById("status").innerHTML = "Image Captured & Saved";
  }, 800);
}
</script>

</body>
</html>
)rawliteral";
}

/* ================= MJPEG STREAM (single client - blocking) ================= */
void handleStream() {
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Stream frame capture failed");
      continue;
    }

    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(fb->len));
    client.println();
    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);
    delay(30);  // Adjust for smoother/faster stream (20-100 ms)
  }
  Serial.println("Stream client disconnected");
}

/* ================= CAPTURE + FLASH + SAVE TO SD ================= */
void handleCapture() {
  digitalWrite(FLASH_PIN, HIGH);
  delay(120);
  digitalWrite(FLASH_PIN, LOW);

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed - fb NULL");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  Serial.printf("Captured frame - size: %u bytes\n", fb->len);

  // ------------------- SAVE TO SD -------------------
  if (SD_MMC.cardType() != CARD_NONE) {
    pictureNumber++;
    String path = "/pic_" + String(pictureNumber) + ".jpg";

    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file on SD for writing");
    } else {
      size_t written = file.write(fb->buf, fb->len);
      Serial.printf("Saved to SD: %s  |  Bytes written: %u / %u\n", path.c_str(), written, fb->len);
      file.close();

      // Persist counter
      prefs.putULong("picNum", pictureNumber);
    }
  } else {
    Serial.println("No SD card detected - photo NOT saved");
  }
  // ------------------- END SAVE -------------------

  // Send fresh image to browser
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

/* ================= ROOT PAGE ================= */
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

/* ================= CAMERA INIT ================= */
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Start small for stability - change to FRAMESIZE_VGA or SVGA if PSRAM present
  config.frame_size   = FRAMESIZE_QVGA;   // 320x240 good balance
  config.jpeg_quality = 20;               // 10-63 (lower = better quality, bigger size)
  config.fb_count     = 2;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    while (true) delay(1000);  // Halt
  }

  // Optional: sensor tweaks (brightness, contrast, etc.)
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_whitebal(s, 1);       // auto white balance
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nESP32-CAM Starting...");

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: http://");
  Serial.println(WiFi.localIP());

  // SD card
  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed");
  } else {
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
    } else {
      Serial.print("SD Card Type: ");
      if (cardType == CARD_MMC)      Serial.println("MMC");
      else if (cardType == CARD_SD)   Serial.println("SDSC");
      else if (cardType == CARD_SDHC) Serial.println("SDHC");
      else                            Serial.println("UNKNOWN");
      uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %llu MB\n", cardSize);
    }
  }

  // Load picture counter from NVS
  prefs.begin("cam", false);
  pictureNumber = prefs.getULong("picNum", 0);
  Serial.printf("Next picture number: %lu\n", pictureNumber + 1);

  // Camera
  startCamera();

  // Server routes
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);

  server.begin();
  Serial.println("Web server started");
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();
  delay(1);  // Allow background tasks
}