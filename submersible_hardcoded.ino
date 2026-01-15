/*************************************************************
 * ESP-01 + Blynk 2.0 (NO Edgent, NO Provisioning)
 * Relay  : GPIO0 (ACTIVE LOW)
 * LDR    : GPIO2
 *************************************************************/

#define BLYNK_PRINT Serial
#define BLYNK_USE_SSL false   // REQUIRED for ESP-01

/* -------- Blynk Credentials -------- */
#define BLYNK_TEMPLATE_ID   "TMPL3rppW-ObG"
#define BLYNK_TEMPLATE_NAME "google esp01 relay"
#define BLYNK_AUTH_TOKEN    "9uN_lfqrxWS1Fo0A7ZiK3J4npHFIPq8f"

/* -------- WiFi Credentials -------- */
char ssid[] = "IOT_AUTOMATION";
char pass[] = "Iot@1234";

/* -------- Pin Definitions -------- */
#define RELAY_PIN 0      // GPIO0
#define LDR_PIN   2      // GPIO2

/* -------- Blynk Virtual Pins ----- */
#define VPIN_RELAY      V1
#define VPIN_PUMP_STATE V2

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

BlynkTimer timer;


bool pumpState = false;
bool lastPumpState = false;

/* -------- Blynk Button -------- */
BLYNK_WRITE(VPIN_RELAY) {
  int value = param.asInt();
  digitalWrite(RELAY_PIN, value ? LOW : HIGH);
}

/* -------- Pump Feedback -------- */
void checkPumpStatus() {
  pumpState = (digitalRead(LDR_PIN) == LOW);

  if (pumpState != lastPumpState) {
    lastPumpState = pumpState;
    Blynk.virtualWrite(VPIN_PUMP_STATE, pumpState ? 1 : 0);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF at boot

  WiFi.mode(WIFI_STA);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(1000L, checkPumpStatus);
}

void loop() {
  Blynk.run();
  timer.run();
}
