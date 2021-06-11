#include <LowPower.h>

#define PROBE_PIN 6
#define BATTERY_PIN 2
#define LED_PIN 17
#define PUMP_MOFSET_PIN 15 // Used to switch the power of the pump with the MOFSET

#define FIVE_MINUTES 300000
#define MAX_IRRIGATION_TIME FIVE_MINUTES
#define DRY_THRESHOLD 400 // In winter 330 - In summer 400
#define SLEEP_TIME_SECONDS 28800 // 8 hours
#define MIN_BATTERY_VOLTAGE 12

// Set to true to test the moisture
bool test = true;
bool noWater = false;

void setCPUAt8Mhz(){
  noInterrupts();
  CLKPR = 1<<CLKPCE;
  CLKPR = 1;
  interrupts();
}
  
void setup() {
  setCPUAt8Mhz();
  Serial.begin(19200);
  Serial.println("Starting!");
  pinMode(PROBE_PIN, INPUT);
  pinMode(BATTERY_PIN, INPUT);
  pinMode(PUMP_MOFSET_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}

int readMoisture(){
  int times = 10;
  long total = 0;
  for (int i = 0; i < times; ++i){
    total += analogRead(PROBE_PIN);
    delay(100);
  }
  
  return total/times;
}

void openPump(){
  Serial.println("Open pump");

  digitalWrite(PUMP_MOFSET_PIN, HIGH);
}

void closePump(){
  Serial.println("Close pump");

  digitalWrite(PUMP_MOFSET_PIN, LOW);
}

float readBatteryVoltage(){
  float batteryReferenceV = 12.37;
  float batteryReferenceInput= 819;
  float batteryInput = analogRead(BATTERY_PIN);
  
  return batteryInput * batteryReferenceV / batteryReferenceInput;
}

void maybeReportLowBattery(float voltage){
  if (voltage < MIN_BATTERY_VOLTAGE)  {
    Serial.println("Low battery!");
  
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW); 
  }
}

void loop() {
  while (test){
    digitalWrite(LED_PIN, HIGH);
    
    Serial.print("Moisture: ");
    Serial.println(readMoisture());
    
    Serial.print("Battery V: ");
    Serial.println(readBatteryVoltage());
    
    Serial.print("Battery bytes: ");
    Serial.println(analogRead(BATTERY_PIN));

    //maybeReportLowBattery(0);
    
    delay(1000);
  }
  Serial.flush();
  while (noWater) LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  
  int moisture = readMoisture();
  unsigned long elapsedTime = 0;
  Serial.println(moisture);
  
  if (moisture > DRY_THRESHOLD){
    while (moisture > DRY_THRESHOLD && elapsedTime < MAX_IRRIGATION_TIME){
      openPump();
      
      delay(10000);
      elapsedTime += 5000;
      moisture = readMoisture();
      Serial.println(moisture);
      Serial.println(elapsedTime);

      closePump();
      
      // Let the soil to absorb the water
      delay(FIVE_MINUTES * 2); // 5m
    }

    closePump();

    // Since the sleep period SLEEP_TIME is high this is more a problem 
    // than a solution to avoid draining the battery.
    /*if (elapsedTime >= MAX_IRRIGATION_TIME){
      Serial.println("No water");
      // We consider that there is no water in the tank
      noWater = true; 
    }
    else{
      elapsedTime = 0;
    }*/
  }
  else {
    Serial.println("Sleep");
    int times = SLEEP_TIME_SECONDS/8;
    Serial.flush();
    for (int i = 0; i < times; ++i) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      maybeReportLowBattery(readBatteryVoltage());
    }
  }
}
