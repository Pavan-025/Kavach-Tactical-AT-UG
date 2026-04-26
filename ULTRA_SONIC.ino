#define BLYNK_TEMPLATE_ID "TMPL3miTlYN34"
#define BLYNK_TEMPLATE_NAME "LIVE STREAMING"
#define BLYNK_AUTH_TOKEN "lIKFT0Sn63E3gGXtE5xxpfvE7hZxqbgI"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

/* WIFI */
char ssid[] = "Realme 8 5G";
char pass[] = "0987654321";

/* ULTRASONIC PINS */
#define TRIG_PIN 5
#define ECHO_PIN 18

BlynkTimer timer;

/* NUMBER OF SAMPLES FOR FILTER */
#define SAMPLES 7

/* ---------- SINGLE MEASUREMENT ---------- */
float measureOnce()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 35000); // ~6m timeout

  if(duration == 0) return -1;

  float distance = duration * 0.0343 / 2.0;
  return distance;
}

/* ---------- MEDIAN FILTER ---------- */
float getDistanceFiltered()
{
  float values[SAMPLES];

  for(int i=0;i<SAMPLES;i++)
  {
    values[i] = measureOnce();
    delay(30);
  }

  /* SORT VALUES */
  for(int i=0;i<SAMPLES-1;i++)
  {
    for(int j=i+1;j<SAMPLES;j++)
    {
      if(values[i] > values[j])
      {
        float temp = values[i];
        values[i] = values[j];
        values[j] = temp;
      }
    }
  }

  /* MEDIAN VALUE */
  float median = values[SAMPLES/2];

  return median;
}

/* ---------- SEND TO BLYNK ---------- */
void sendUltrasonic()
{
  float distance = getDistanceFiltered();

  if(distance > 2 && distance < 600)
  {
    Blynk.virtualWrite(V0, distance);

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }
  else
  {
    Serial.println("Invalid reading");
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(700L, sendUltrasonic);
}

void loop()
{
  Blynk.run();
  timer.run();
}