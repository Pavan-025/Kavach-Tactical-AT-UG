#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// === PINS ===
#define CE_PIN    7
#define CSN_PIN   8

// L298N Motor Driver
#define IN1  3
#define IN2  4
#define IN3  5
#define IN4  6
#define ENA  9   // PWM - left motor
#define ENB  10  // PWM - right motor

// Servo
#define SERVO_PIN 2

// === DATA STRUCTURE (must match transmitter exactly) ===
struct Signal {
  byte throttle;   // 0–255
  byte pitch;      // spare
  byte roll;       // 0–255
  byte aux1;       // servo
  byte aux2;       // spare
} data;

// === OBJECTS ===
RF24 radio(CE_PIN, CSN_PIN);
Servo servo;
const uint64_t pipe = 0xE9E8F0F0E1LL;

void setup() {
  Serial.begin(115200);
  Serial.println("Receiver Starting...");

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Stop motors on startup
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  // Servo
  servo.attach(SERVO_PIN);
  servo.write(90);  // center position

  // Radio setup
  if (!radio.begin()) {
    Serial.println("Radio hardware failure! Check wiring/SPI.");
    while (1);
  }

  radio.setChannel(108);
  radio.setDataRate(RF24_250KBPS);
  radio.setCRCLength(RF24_CRC_8);
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(1, pipe);
  radio.startListening();

  radio.printDetails();
  Serial.println("Listening for packets...");
}

void loop() {
  if (radio.available()) {
    radio.read(&data, sizeof(data));

    // Center values: -127 to +128 approx
    int throttle = data.throttle - 127;
    int roll     = data.roll     - 127;

    // DEADZONE – motors stop completely near center throttle
    const int DEADZONE = 18;          // ← tune this (15–25 common)
    if (abs(throttle) < DEADZONE) {
      throttle = 0;
    }

    // Differential mixing (tank steering)
    int leftSpeed  = throttle + roll;
    int rightSpeed = throttle - roll;

    // Limit to valid PWM range
    leftSpeed  = constrain(leftSpeed,  -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);

    // Left motor control
    if (leftSpeed > 0) {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      analogWrite(ENA, leftSpeed);
    } 
    else if (leftSpeed < 0) {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      analogWrite(ENA, -leftSpeed);
    } 
    else {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      analogWrite(ENA, 0);
    }

    // Right motor control
    if (rightSpeed > 0) {
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      analogWrite(ENB, rightSpeed);
    } 
    else if (rightSpeed < 0) {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      analogWrite(ENB, -rightSpeed);
    } 
    else {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      analogWrite(ENB, 0);
    }

    // Servo control (0–180°)
    int servoAngle = map(data.aux1, 0, 255, 0, 180);
    servo.write(servoAngle);

    // Optional debug output
    Serial.print("RX ← Th:"); Serial.print(data.throttle);
    Serial.print(" ("); Serial.print(throttle); Serial.print(")");
    Serial.print(" Ro:");     Serial.print(data.roll);
    Serial.print("  Left:");  Serial.print(leftSpeed);
    Serial.print(" Right:");  Serial.println(rightSpeed);
  }

  // === Optional safety timeout / failsafe ===
  // Uncomment if you want motors to stop after signal loss
  /*
  static unsigned long lastPacket = 0;
  if (radio.available()) {
    lastPacket = millis();
  }s
  if (millis() - lastPacket > 500) {  // 500 ms no signal → stop
    analogWrite(ENA, 0);
    analogWrite(ENB, 0);
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    servo.write(90);
    // Serial.println("Signal lost → safety stop");
  }
  */
}