/**************************************************************
 * ESP-01 pump controller with feedback – git:-Abhishekiot
 **************************************************************/

#define BLYNK_PRINT Serial
#define BLYNK_USE_SSL false

/* ---------------- BLYNK ---------------- */
#define BLYNK_TEMPLATE_ID   "TMPL3rppW-ObG"
#define BLYNK_TEMPLATE_NAME "google esp01 relay"
#define BLYNK_AUTH_TOKEN    "9uN_lfqrxWS1Fo0A7ZiK3J4npHFIPq8f"

/* ---------------- PINS ---------------- */
#define RELAY_PIN 0   // GPIO0 (ACTIVE LOW)
#define LDR_PIN   2   // GPIO2

#define VPIN_RELAY      V1
#define VPIN_PUMP_STATE V2

/* ---------------- SYSTEM ---------------- */
#define AP_SSID "Abhishekiot pumpControl"
#define WIFI_RETRY_INTERVAL 5000UL
#define WIFI_TEST_TIMEOUT   15000UL

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>

/* ---------------- STRUCT ---------------- */
struct WiFiCred {
  char ssid[32];
  char pass[32];
};

WiFiCred activeCred;
WiFiCred backupCred;
WiFiCred pendingCred;

/* ---------------- GLOBALS ---------------- */
ESP8266WebServer server(80);
BlynkTimer timer;

bool apRunning = false;
bool testingNewWiFi = false;
unsigned long wifiTestStart = 0;
unsigned long lastWiFiAttempt = 0;

bool pumpState = false, lastPumpState = false;

/* ---------------- EEPROM ---------------- */
void loadCreds() {
  EEPROM.begin(256);
  EEPROM.get(0, activeCred);
  EEPROM.get(64, backupCred);

  if (activeCred.ssid[0] == 0xFF) {
    memset(&activeCred, 0, sizeof(WiFiCred));
    memset(&backupCred, 0, sizeof(WiFiCred));
  }
}

void saveActiveCred() {
  EEPROM.put(0, activeCred);
  EEPROM.put(64, backupCred);
  EEPROM.commit();
}

/* ---------------- WIFI ---------------- */
void connectActiveWiFi() {
  WiFi.begin(activeCred.ssid, activeCred.pass);
}

void startAP() {
  if (apRunning) return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);

  server.on("/", []() {
    String page =
      "<h3>WiFi Setup</h3>"
      "<p>Previous WiFi: <b>" + String(activeCred.ssid) + "</b></p>"
      "<form action='/save'>"
      "New SSID:<br><input name='s'><br>"
      "Password:<br><input name='p' type='password'><br><br>"
      "<input type='submit' value='Save & Test'>"
      "</form>";
    server.send(200, "text/html", page);
  });

  server.on("/save", []() {
    if (!server.hasArg("s") || !server.hasArg("p")) {
      server.send(400, "text/plain", "Invalid input");
      return;
    }

    strncpy(pendingCred.ssid, server.arg("s").c_str(), 31);
    strncpy(pendingCred.pass, server.arg("p").c_str(), 31);

    server.send(200, "text/html", "<h3>Testing new WiFi…</h3>");

    testingNewWiFi = true;
    wifiTestStart = millis();
    WiFi.begin(pendingCred.ssid, pendingCred.pass);
  });

  server.begin();
  apRunning = true;

  digitalWrite(RELAY_PIN, HIGH); // FAIL SAFE
}

void stopAP() {
  if (!apRunning) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  apRunning = false;
}

/* ---------------- BLYNK ---------------- */
BLYNK_WRITE(VPIN_RELAY) {
  int state = param.asInt();
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);
}

/* ---------------- PUMP FEEDBACK -------- */
void checkPump() {
  pumpState = (digitalRead(LDR_PIN) == LOW);
  if (pumpState != lastPumpState) {
    lastPumpState = pumpState;
    Blynk.virtualWrite(VPIN_PUMP_STATE, pumpState ? 1 : 0);
  }
}

/* ---------------- WIFI MANAGER TASK ---- */
void wifiManagerTask() {
  wl_status_t st = WiFi.status();

  /* New WiFi testing */
  if (testingNewWiFi) {
    if (st == WL_CONNECTED) {
      // PROMOTE
      backupCred = activeCred;
      activeCred = pendingCred;
      saveActiveCred();

      testingNewWiFi = false;
      stopAP();
      return;
    }

    if (millis() - wifiTestStart > WIFI_TEST_TIMEOUT) {
      // FAILED → rollback
      testingNewWiFi = false;
      connectActiveWiFi();
      return;
    }
  }

  /* Normal retry logic */
  if (st != WL_CONNECTED) {
    startAP();
    if (millis() - lastWiFiAttempt > WIFI_RETRY_INTERVAL) {
      lastWiFiAttempt = millis();
      connectActiveWiFi();
    }
  } else {
    stopAP();
  }
}

/* ---------------- SETUP ---------------- */
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF

  loadCreds();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  if (activeCred.ssid[0]) {
    connectActiveWiFi();
  } else {
    startAP();
  }

  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(1000L, checkPump);
  timer.setInterval(2000L, wifiManagerTask);
}

/* ---------------- LOOP ----------------- */
void loop() {
  if (apRunning) server.handleClient();
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  timer.run();
}
