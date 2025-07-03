#include "secrets.h"

// *********** Blynk setup ***********
#define BLYNK_TEMPLATE_ID BLYNK_TEMPLATE_ID_SECRET
#define BLYNK_TEMPLATE_NAME BLYNK_TEMPLATE_NAME_SECRET
#define BLYNK_AUTH_TOKEN BLYNK_AUTH_TOKEN_SECRET
#define BLYNK_PRINT Serial
// V0 Temperature
#define TEMPERATURE_PIN V0
#define HUMIDITY_PIN V1
#define IRRIGATION_PIN V2
#define IRRIGATION_LOOPS_PIN V3
#define OPEN_PUMP_PIN V4

// *********** End Blynk setup ***********

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// *********** Constants ***********
#define PUMP_MOFSET_PIN 15  // Used to switch the power of the pump with the MOFSET
#define TANK_SENSOR_PIN 12
#define TEMP_HUM_PIN 0
#define DHTTYPE DHT11

const unsigned long oneMinuteMs = 60000;
const unsigned long oneHourMs = 3600000;
const unsigned long irrigationTimeMs = 10000;
const unsigned long sleepTimeMs = 24 * oneHourMs;
const int minIrrigationLoops = 1;
const float maxTemperature = 30.0;
// *********** End Constants ***********

// *********** Variables ***********

char ssid[] = WIFI_SSID_SECRET;
char pass[] = WIFI_PASSWORD_SECRET;

unsigned int waterTankCapacity = 0;
unsigned int irrigationLoops = 6;
unsigned int irrigationLoopsLeft = 0;
bool lowWaterReported = false;
int initializeTimer = -1;
bool test = false;  // Set to true to test the moisture

DHT dht(TEMP_HUM_PIN, DHTTYPE);
BlynkTimer timer; 

// *********** End Variables ***********

float readHumidity() {
  float h = 0;
  h = dht.readHumidity();
  byte retry = 0;
  while (isnan(h) && retry < 5) {
    delay(250);
    h = dht.readHumidity();
    retry++;
  }

  Serial.print("Humidity: ");
  Serial.println(h);
  Blynk.virtualWrite(HUMIDITY_PIN, h);

  return h;
}

float readTemperature() {
  float t = 0;
  t = dht.readTemperature();
  byte retry = 0;
  while (isnan(t) && retry < 5) {
    delay(250);
    t = dht.readTemperature();
    retry++;
  }

  Serial.print("Temperature: ");
  Serial.println(t);
  Blynk.virtualWrite(TEMPERATURE_PIN, t);

  return t;
}

void openPump() {
  Serial.println("Open pump");

  digitalWrite(PUMP_MOFSET_PIN, HIGH);
}

void closePump() {
  Serial.println("Close pump");

  digitalWrite(PUMP_MOFSET_PIN, LOW);
}

int calculateIrrigationLoops() {
  float temperature = min(maxTemperature, readTemperature());
  float seasonFactor = temperature / maxTemperature;
  int loops = max(minIrrigationLoops, (int)round(irrigationLoops * seasonFactor));

  Serial.println(String("Irrigation loops: ") + loops);
  Blynk.virtualWrite(IRRIGATION_PIN, loops);

  return loops;
}

void maybeReportLowWater() {
  if (!waterTankFull() && !lowWaterReported) {
    Blynk.logEvent("no_water", "Fill water tank");
    Serial.println("Event no_water sent");
    lowWaterReported = true;
  } else {
    lowWaterReported = false;
  }
}

bool waterTankFull() {
  bool full = digitalRead(TANK_SENSOR_PIN) == HIGH;
  Serial.print("Tank is full? ");
  Serial.println(full);
  return full;
}

void maybeTest() {
  if (test){
    test = false; // Set intervals only once.
    unsigned int interval = 2000;
    timer.setInterval(interval, readTemperature);
    timer.setInterval(interval + 100, readHumidity);
    timer.setInterval(interval + 200, waterTankFull);
    timer.setInterval(interval + 300, calculateIrrigationLoops);
    timer.setInterval(interval + 400, maybeReportLowWater);
  }
}

// *********** Main logic ***********

BLYNK_WRITE(IRRIGATION_LOOPS_PIN) {
  irrigationLoops = param.asInt();
  Serial.println(String("Irrigation loops updated: ") + irrigationLoops);

  // If the timer is set it means we are initializing.
  if (initializeTimer != -1) {
    timer.deleteTimer(initializeTimer);
    initializeTimer = -1;
    timer.setTimeout(1000, initIrrigation);
  }
}

BLYNK_WRITE(OPEN_PUMP_PIN) {
  bool open = (bool)param.asInt();
  Serial.println(String("Pump open: ") + open);
  if (open) {
    openPump();
  } else {
    closePump(); 
  }
}

void setup() {
  Serial.begin(19200);
  Serial.println("Starting!");
  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Initialize irrigation loops from the server
  Blynk.syncVirtual(IRRIGATION_LOOPS_PIN);
  // If after 10 seconds the value is not set, it stars offline with the default value.
  initializeTimer = timer.setTimeout(10000, initIrrigation);

  pinMode(PUMP_MOFSET_PIN, OUTPUT);
  pinMode(TANK_SENSOR_PIN, INPUT);
}

void loop() {
  Blynk.run();
  maybeTest();
  timer.run();
}

void initIrrigation(){
  irrigationLoopsLeft = calculateIrrigationLoops();
  timer.setTimeout(10, continueIrrigation);
}

void continueIrrigation() {
  openPump();
  timer.setTimeout(irrigationTimeMs, closePump);
  irrigationLoopsLeft--;
  if (irrigationLoopsLeft > 0) {
    // Let the soil to absorb the water so no drops fall when it is too wet.
    timer.setTimeout(oneMinuteMs * 5, continueIrrigation);
  } else {
    maybeReportLowWater();
    timer.setTimeout(sleepTimeMs, initIrrigation);
  }
}
