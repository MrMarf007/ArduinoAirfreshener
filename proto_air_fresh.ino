#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// digital pins
const int override = 2, 
          select = 3, scroll = 19,
          distEcho = 10, distTrigger = 11,
          thermo = 12,
          sprayer = 13,
          ledGreen = 14, ledRed = 15,
          magnet = 17,
          motion = 18;

// analog pins
const int lightSensor = 2;

// pins for display
const int rs = 9, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

OneWire oneWire(thermo);
DallasTemperature sensors(&oneWire);

// vars

/*
  Track device state:
    0 = "not in use"
    1 = "in use - type unknown"
    2 = "in use - number 1"
    3 = "in use - number 2"
    4 = "in use - cleaning"
    5 = "triggered"
    6 = "operator menu"
*/

void setup() {
  Serial.begin (9600);

  // setup all pins
  pinMode(override, INPUT);
  attachInterrupt(digitalPinToInterrupt(override), overrideInterrupt, FALLING);
  pinMode(select, INPUT);
  attachInterrupt(digitalPinToInterrupt(select), selectInterrupt, FALLING);
  pinMode(distEcho, INPUT);
  pinMode(distTrigger, OUTPUT);
  pinMode(thermo, INPUT);
  pinMode(sprayer, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(A2, INPUT_PULLUP);
  pinMode(magnet, INPUT);
  pinMode(motion, INPUT);
  pinMode(scroll, INPUT);

  // setup display
  lcd.begin(16, 2);

  // setup temp sensor
  sensors.setResolution(9);
  sensors.begin();
  sensors.requestTemperatures();
}

// timers
unsigned long lastTempCheck = 0, lastDistanceCheck = 0, lastLCDPrint = 0, lastLightCheck = 0, blinker = 0, sprayTimer = 0;

// variables
int state = 0, toSpray = 0, light=0;
double temp=0;

// lightOn will be updated after one full second of light/darkness, motionDetected is true if there has been motion in the last 5 seconds.
bool lightOn = false, motionDetected  = false, ledOn = false, sprayOn = false;

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLCDPrint > 100) { 
    lastLCDPrint = currentMillis;
    printLCD();
  }

  switch (state) {
    /* ===== not in use - keep checking light and motion to detect a user ===== */
    case 0:
      setLeds(0,0);

      if (currentMillis - lastLightCheck > 50) { 
        lastLightCheck = currentMillis;
        updateLightStatus();
      }
      
      // if both the light is on and motion was detected, the toilet is in use
      if (lightOn) {
        state = 1;
      }

      break;


    /* ===== spray triggered ===== */
    case 5:

      // if no more sprays are needed, exit to state 0
      if (toSpray == 0) {
        digitalWrite(ledGreen, LOW);
        state = 0;
      }

      // we need to FRESHEN!
      // if freshener pin is not on yet, turn it on and start a timer for 25 seconds
      if (!sprayOn && (currentMillis - sprayTimer > 2000)) {
        digitalWrite(sprayer, HIGH);
        sprayOn = true;
        sprayTimer = currentMillis;
      } else if (currentMillis - sprayTimer > 22000) { 
        digitalWrite(sprayer, LOW);
        toSpray--;
        sprayOn = false;
        sprayTimer = currentMillis;
      }

      // blink the green light during the FRESHENING
      if (currentMillis - blinker > 400) { 
        blinker = currentMillis;
        ledOn = !ledOn;
        digitalWrite(ledGreen, ledOn ? HIGH : LOW);
      }
      break;


    /* ===== operator menu ===== */
    case 6:
      if (currentMillis - blinker > 400) { 
        blinker = currentMillis;
        ledOn = !ledOn;
        digitalWrite(ledRed, ledOn ? HIGH : LOW);
      }
      if (currentMillis - sprayTimer > 2000) { 
        digitalWrite(ledRed, LOW);
        state = 1;
      }
      break;


    /* ===== in use - monitor sensors to determine which type of use ===== */
    default:
      setLeds(1,0);

      if (currentMillis - lastTempCheck > 1500) { 
        lastTempCheck = currentMillis;
        sensors.requestTemperatures();
        temp = sensors.getTempCByIndex(0);
      }

      if (currentMillis - lastDistanceCheck > 300) { 
        lastDistanceCheck = currentMillis;
        measureDistance();
      }
      break;
  }
}

void setLeds(bool g, bool r) {
  digitalWrite(ledGreen, g);
  digitalWrite(ledRed, r);
}

void printLCD() {
  lcd.clear();
  lcd.print(state);
  switch (state) {
    case 0:
      // NORMAL OPERATION
      lcd.setCursor(0,0);
      lcd.print("LightON: ");
      lcd.print(lightOn);
      lcd.setCursor(0,1);
      lcd.print(light);
      break;
    case 5: {
      // THE FRESHENING
      int time = 15 - ((millis() - sprayTimer) / 1000);
      if (time < 1) {time = 0;}
      lcd.setCursor(0,0);
      lcd.print("spraying ");
      lcd.print(toSpray);
      lcd.print("x");
      lcd.setCursor(0,1);
      lcd.print("t-");
      lcd.print(time);
      break;
    }
    case 6:
      // MENU
      lcd.setCursor(0,1);
      lcd.print("MENU");
      break;
    default:
      // NORMAL OPERATION
      lcd.setCursor(0,0);
      lcd.print("state: ");
      lcd.print(state);
      break;
  }
}



// light sensor readings higher than this value will be considered 'light on' 
int lightThreshold = 600;
// lightTimer records the last time the sensor had a 'dark' reading
unsigned long lastDarkTime = 0, lastLightTime  = 0;

void updateLightStatus() {
  light = analogRead(lightSensor);
  if (light > lightThreshold) {
    lastLightTime = millis();
    // if its been light for more than a second, the light is on
    if ((lastLightTime - lastDarkTime > 1000) && !lightOn) {
      lightOn = true;
    }
  } else {
    lastDarkTime = millis();
    // if its been dark for more than a second, the light is off
    if ((lastDarkTime - lastLightTime > 1000) && lightOn) {
      lightOn = false;
    }
  }
}

long distanceCM, echoTime;

void measureDistance() {
  digitalWrite(distTrigger, LOW);
  delayMicroseconds(5);
  digitalWrite(distTrigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(distTrigger, LOW);
 
  // get time between trigger and echo
  echoTime = pulseIn(distEcho, HIGH);
 
  // Convert the time into a distance
  distanceCM = (echoTime/2) / 29.1;
}


// INTERRUPT FUNCTIONS

void overrideInterrupt() {
  // debouncing with unsigned long as unsigned arithmatic is millis() rollover safe
  static unsigned long lastCallTime = 0;
  unsigned long thisCallTime = millis();
  if (thisCallTime - lastCallTime > 75) {
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, LOW);
    toSpray = 1;
    state = 5;
  }
  lastCallTime = thisCallTime;
}

void selectInterrupt() {
  // debouncing with unsigned long as unsigned arithmatic is millis() rollover safe
  static unsigned long lastCallTime = 0;
  unsigned long thisCallTime = millis();
  if (thisCallTime - lastCallTime > 75) {
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, LOW);
    sprayTimer = thisCallTime;
    state = 6;
  }
  lastCallTime = thisCallTime;
}