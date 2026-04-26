#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// === PINS ===
#define CE_PIN    7
#define CSN_PIN   8

#define THROTTLE_PIN  A0   // Forward/Backward
#define ROLL_PIN      A3   // Left/Right
#define PITCH_PIN     A2   // Spare
#define AUX1_PIN      A6   // Servo
#define AUX2_PIN      A7   // Spare

// === DATA STRUCTURE ===
struct Signal {
  byte throttle;
  byte pitch;
  byte roll;
  byte aux1;
  byte aux2;
} data;

// === RADIO ===
RF24 radio(CE_PIN, CSN_PIN);
const uint64_t pipe = 0xE9E8F0F0E1LL;

void setup() {
  Serial.begin(115200);
  Serial.println("Transmitter Starting...");

  if (!radio.begin()) {
    Serial.println("Radio hardware failure! Check wiring.");
    while (1);
  }

  radio.setChannel(108);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_8);
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_LOW);      // LOW to prevent saturation
  radio.openWritingPipe(pipe);

  radio.printDetails();
  Serial.println("Ready to transmit...");
}

void loop() {
  // Read raw analog values (0–1023)
  int thrRaw = analogRead(THROTTLE_PIN);
  int pitRaw = analogRead(PITCH_PIN);
  int rolRaw = analogRead(ROLL_PIN);
  int aux1Raw = analogRead(AUX1_PIN);
  int aux2Raw = analogRead(AUX2_PIN);

  // Map to 0–255
  data.throttle = map(thrRaw, 0, 1023, 0, 255);
  data.pitch    = map(pitRaw, 0, 1023, 0, 255);
  data.roll     = map(rolRaw, 0, 1023, 0, 255);
  data.aux1     = map(aux1Raw, 0, 1023, 0, 255);
  data.aux2     = map(aux2Raw, 0, 1023, 0, 255);

  // Send
  bool ok = radio.write(&data, sizeof(data));

  // Debug output
  Serial.print("TX → Th:"); Serial.print(data.throttle);
  Serial.print(" Ro:");     Serial.print(data.roll);
  Serial.print(" A1:");     Serial.print(data.aux1);
  Serial.print("  OK:");    Serial.println(ok ? "YES" : "FAIL");

  delay(20);  // ~50 Hz
}