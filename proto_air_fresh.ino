#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

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

int spraysLeft = 0;
void setup() {
  Serial.begin (9600);

  spraysLeft = readIntFromEEPROM(0);

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
  sensors.setResolution(8);
  sensors.begin();
  sensors.requestTemperatures();
}

/*  
Track device state:
  0 = "not in use"
  1 = "in use - type unknown"
  2 = "in use - number 1"
  3 = "in use - number 2"
  4 = "in use - cleaning"
  5 = "triggered"
  6 = "operator menu"
  7 = "exiting menu"
  8 = "sensor view"
  9 = "spray delay"
*/
const int notInUse = 0, inUseUnknown = 1, inUse1 = 2, inUse2 = 3, inUseCleaning = 4, stateTriggered = 5, stateMenu = 6, exitMenu = 7, sensorView = 8, delaySpray = 9;
int state = 0;

// timers
unsigned long lastTempCheck = 0, lastDistanceCheck = 0, lastLCDPrint = 0, lastLightCheck = 0, lastMagnetCheck = 0, lastMotionCheck = 0, lastMotion = -1UL, doorCloseTimer = 0, blinker = 0, sprayTimer = 0, exitTimer = 0, sprayDelay = 10000, useStart = 0;

// variables
const int numberOfMenuItems = 3, numberOfSensors = 5;
int toSpray = 0, light=0, distance = 0, selectedMenuItem = 0, selectedSensor = 0;
double temp = 0;

bool lightOn = false, motionDetected = false, sprayOn = false, magnetContact = false, bigFlush = false, doorClosed = false;

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLCDPrint > 100) { 
    lastLCDPrint = currentMillis;
    printLCD();
  }

  switch (state) {
    /* ===== not in use - keep checking light and motion to detect a user ===== */ 
    case notInUse:
      setLeds(0,0,0,0);

      checkLight(100);
      checkMotion(150);
      
      // if both the light is on and motion was detected, the toilet is in use
      if ((millis() - lastMotion < 10000) && lightOn) {
        state = inUseUnknown;
        useStart = millis();
      }

      // keep ambient temp up to date
      checkTemp(1500);

      break;


    /* ===== spray triggered ===== */    
    case stateTriggered: {// TODO - TEST
      // LET THE FRESHENING COMMENCE!!!!

      // blink the green light during the FRESHENING
      setLeds(1,0,1,0);

      // if freshener pin is not on yet, turn it on and start a timer for 18 seconds, then wait 2 seconds so the freshener can turn off fully before possibly doing the second spray
      if (!sprayOn && (currentMillis - sprayTimer > 2000)) {
        digitalWrite(sprayer, HIGH);
        sprayOn = true;
        sprayTimer = currentMillis;
      } else if (sprayOn && currentMillis - sprayTimer > 18000) { 
        digitalWrite(sprayer, LOW);
        sprayOn = false;
        sprayTimer = currentMillis;
        toSpray--;
        spraysLeft--;
        updateIntToEEPROM(0, spraysLeft);
      }

      // if no more sprays are needed, exit to state 0
      if (toSpray == 0) {
        // digitalWrite(ledRed, HIGH);
        state = notInUse;
      }
      break;
    }
    /* ===== operator menu ===== */
    case 6:
      setLeds(0,0,0,0);

      readSelectButton();

      break;

    /* ===== returning to state notInUse ===== */
    case 7:
      setLeds(0,0,0,0);

      if (currentMillis - exitTimer > 10000) {
        purge();
        state = notInUse;
      }

      break;

    /* ===== sensor view ===== */
    case 8:
      setLeds(0,0,0,0);

      readSelectButton();

      switch (selectedSensor) {
        case 1:
          checkLight(100);
          break;
        case 2:
          checkTemp(1000);
          break;
        case 3:
          checkDistance(150);
          break;
        case 4:
          checkMotion(150);
          break;
        case 5:
          checkMagnet(75);
          break;
      }
      
      break;

    /* ===== spray delay ===== */
    case 9:
      setLeds(1,0,1,0);

      if (currentMillis - exitTimer > sprayDelay) {
        sprayTimer = millis();
        sprayOn = false;
        state = stateTriggered;
      }

      break;

    /* ===== in use - monitor sensors to determine which type of use ===== */
    default: // TODO - implement
      
      switch (state) {
        case inUseUnknown: 
          setLeds(1,0,0,0); 
          break;
        case inUse1: 
          setLeds(1,1,0,0); 
          break;
        case inUse2: 
          setLeds(1,1,0,4); 
          break;
        case inUseCleaning: 
          setLeds(1,1,0,1); 
          break;
      }

      checkTemp(1000);
      checkLight(100);
      checkDistance(200);
      checkMagnet(75);

      if (currentMillis - useStart > 10000) {
        if (state == inUseUnknown && doorClosed) {
          state = inUse1;
          toSpray = 1;
        }

        if (bigFlush && doorClosed) {
          state = inUse2;
          toSpray = 2;
        }

        if (!doorClosed) {
          state = inUseCleaning;
          toSpray = 0;
        }
      }

      if (!lightOn) {
        // light is off, use of toilet ended

        if (useStart - currentMillis < 10000) {
          // use was too short, probably an error, no sprays, return to notInUse
          exitTimer = currentMillis;
          state = exitMenu;
        }

        switch (state) {
          case inUseUnknown:
            exitTimer = currentMillis;
            state = exitMenu;
            doorClosed = true; // REMOVE
            break;
          case inUse1:
            purge();
            toSpray = 1;
            exitTimer = millis();
            state = delaySpray;
            break;
          case inUse2:
            purge();
            toSpray = 2;
            exitTimer = currentMillis;
            state = delaySpray;
            break;
          case inUseCleaning:
            exitTimer = currentMillis;
            state = exitMenu;
            doorClosed = true; // REMOVE
            break;
        }
      }

      break;
  }
}


// params: green on/off, red on/off, green blinkmode (0 = no blink, 1 = slow blink, 2 = fast blink)
void setLeds(bool g, bool r, int gMode, int rMode) {
  static bool gOn = false, rOn = false;
  static unsigned long gBlinker = 0, rBlinker = 0;
  unsigned long currentMillis = millis();

  if (!g) {
    digitalWrite(ledGreen, LOW);
    gOn = false;
  } else if (gMode == 0) {
    digitalWrite(ledGreen, HIGH);
    gOn = true;
  } else if (currentMillis - gBlinker > (500 / gMode)) { 
    gBlinker = currentMillis;
    gOn = !gOn;
    digitalWrite(ledGreen, gOn ? HIGH : LOW);
  }

  if (!r) {
    digitalWrite(ledRed, LOW);
    rOn = false;
  } else if (rMode == 0) {
    digitalWrite(ledRed, HIGH);
    rOn = true;
  } else if (currentMillis - rBlinker > (500 / rMode)) { 
    rBlinker = currentMillis;
    rOn = !rOn;
    digitalWrite(ledRed, rOn ? HIGH : LOW);
  }
}

void printLCD() {
  unsigned long currentMillis = millis();
  lcd.clear();
  lcd.print(state);
  switch (state) {
    case 5: {
      // THE FRESHENING
      int time = 15 - ((currentMillis - sprayTimer) / 1000);
      if (time < 0) {time = 0;}
      lcd.setCursor(0,0);
      lcd.print("spraying ");
      lcd.print(toSpray);
      lcd.print("x");
      lcd.setCursor(0,1);
      if (sprayOn) {
        lcd.print("t-");
        lcd.print(time);
      }
      break;
    }
    case 6: {
      // MENU
      lcd.setCursor(0,0);
      lcd.print("    MENU    ");
      lcd.print(selectedMenuItem);
      lcd.setCursor(0,1);
      switch (selectedMenuItem) {
        case 0:
          lcd.print("EXIT");
          break;
        case 1:
          lcd.print("sensor view");
          break;
        case 2: {
          lcd.print("delay: ");
          int secs = sprayDelay / 1000;
          lcd.print(secs);
          lcd.print("s");
          break;
        }
        case 3:
          lcd.print("#sprays: ");
          lcd.print(spraysLeft);
          break;
      }
      break;
    }
    case 7: {
      lcd.setCursor(0,0);
      lcd.print("Device restarts");
      lcd.setCursor(0,1);
      lcd.print("   in ");
      int val = 10000 - (currentMillis - exitTimer);
      lcd.print(val / 1000);
      lcd.setCursor(9,1);
      lcd.print("sec");
      break;
    }
    case 8: {
      // SENSOR VIEW
      lcd.setCursor(0,0);
      lcd.print("SENSOR: ");
      lcd.print(selectedSensor);
      lcd.setCursor(0,1);
      switch (selectedSensor) {
        case 0:
          lcd.print("BACK");
          break;
        case 1: // light
          lcd.print("light: ");
          lcd.print(lightOn ? "ON" : "OF");
          lcd.print(" - ");
          lcd.print(light);
          break;
        case 2: // temp
          lcd.print("temp: ");
          lcd.print(temp);
          lcd.print("C");
          break;
        case 3: // dist
          lcd.print("dist: ");
          lcd.print(distance);
          lcd.print("cm");
          break;
        case 4: { // motion
          lcd.print("Move: ");
          String txt;
          int ago = (currentMillis - lastMotion) / 1000;
          if (ago < 1000) {
            txt = String(ago);
          } else {
            txt = ">999";
          }
          lcd.print(txt);
          lcd.print("s ago");
          break;
        }
        case 5: // magnetic contact
          lcd.print(magnetContact);
          break;
      }
      break;
    }
    case 9: {
      lcd.setCursor(0,0);
      lcd.print("waiting to spray");
      int time = (sprayDelay - (currentMillis - exitTimer)) / 1000;
      lcd.setCursor(0,1);
      lcd.print(time);
      lcd.print("s to go");
      lcd.setCursor(15,1);
      lcd.print(toSpray);
      break;
    }
    default: {
      // NORMAL OPERATION
      lcd.setCursor(0,0);
      lcd.print("#sprays: ");
      lcd.print(spraysLeft);
      lcd.setCursor(15,0);
      lcd.print(state);
      lcd.setCursor(0,1);
      lcd.print("temp: ");
      lcd.print(temp);
      lcd.print(" C");
      break;
    }
  }
}

void checkLight(int t) {
  if (millis() - lastLightCheck > t) { 
    lastLightCheck = millis();

    // lightTimer records the last time the sensor had a 'dark' reading
    static unsigned long lastDarkTime = 0, lastLightTime  = 0;
    // light sensor readings higher than this value will be considered 'light on' 
    int lightThreshold = 600;

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
}

void checkTemp(int t) {
  if (millis() - lastTempCheck > t) { 
    lastTempCheck = millis();
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
  }
}
int doorDistance = 86;
void checkDistance(int t) {
  if (millis() - lastDistanceCheck > t) { 
    lastDistanceCheck = millis();

    digitalWrite(distTrigger, LOW);
    delayMicroseconds(2);
    digitalWrite(distTrigger, HIGH);
    delayMicroseconds(10);
    digitalWrite(distTrigger, LOW);
  
    unsigned long echoTime = -1UL;
    // get time between trigger and echo
    echoTime = pulseIn(distEcho, HIGH, 22000);
  
    // Convert the time into a distance
    distance = ((echoTime/2) / 29.1);


    // if a distance very close to the door distance is registered, increase the timer
    if (distance > doorDistance - 2 && distance < doorDistance + 2) {
      doorCloseTimer += t;
    }

    // if a distance larger than the distance to the door is registered shrink the timer. (not a full reset as )
    if (distance >= 88) {
      doorCloseTimer /= 2;
    }

    // if the door seems closed for long enough, register it as closed
    if (doorCloseTimer > 8000) {
      doorClosed = true;
    } else {
      // doorClosed = false;
    }
  }
}

void checkMagnet(int t) {
  if (millis() - lastMagnetCheck > t) { 
    lastMagnetCheck = millis();
    magnetContact = digitalRead(magnet);
    if (magnetContact == HIGH) {
      bigFlush = true;
    }
  }
}

void checkMotion(int t) {
  if (millis() - lastMotionCheck > t) { 
    lastMotionCheck = millis();
    motionDetected = digitalRead(motion);
    if (motionDetected == HIGH) {
      lastMotion = millis();
    }
  }
}

void purge() {
  setLeds(0,0,0,0);
  digitalWrite(sprayer, LOW);
  digitalWrite(distTrigger, LOW);
  doorCloseTimer = 0;
  toSpray = 0;
  light=0;
  distance = 0;
  selectedMenuItem = 0;
  temp = 0;
  lightOn = false;
  motionDetected  = false;
  sprayOn = false;
  bigFlush = false;
}

void updateIntToEEPROM(int address, int number) {
  uint8_t b1 = (number >> 8) & 0xFF;
  uint8_t b2 = number & 0xFF;

  EEPROM.update(address, b1);
  EEPROM.update(address+1, b2);
}

int readIntFromEEPROM(int address) {
  uint8_t b1 = EEPROM.read(address);
  uint8_t b2 = EEPROM.read(address+1);

  return ((b1 << 8) + b2);
}

unsigned long sprayDelays[8] = {0,5000,10000,30000,60000,120000,180000,360000};
void activateCurrentMenuItem() {
  if (state == stateMenu) {
    switch (selectedMenuItem) {
      case 0:
        purge();
        exitTimer = millis();
        state = exitMenu;
        break;
      case 1:
        state = sensorView;
        selectedSensor = 1;
        break;
      case 2:
        static unsigned int i = 0;
        i++;
        if (i == 8) {i = 0;}
        sprayDelay = sprayDelays[i];
        break;
      case 3:
        spraysLeft = 2400;
        updateIntToEEPROM(0, spraysLeft);
        break;
    }
  } else if (state == sensorView) {
    if (selectedSensor == 0) {
      purge();
      state = stateMenu;
    }
  }
}

int lastButtonState = LOW, buttonState = LOW;
unsigned long lastDBTime = 0;
void readSelectButton() {
  int r = digitalRead(19);
  if (r != lastButtonState) {
    lastDBTime = millis();
  }
    
  if ((millis() - lastDBTime) > 75) {
    if (r != buttonState) {
      buttonState = r;
    
      if (buttonState == LOW) {
        if (state == stateMenu) {
          selectedMenuItem++;
          if (selectedMenuItem > numberOfMenuItems) {
            selectedMenuItem = 0;
          }
        } else if (state == sensorView) {
          selectedSensor++;
          if (selectedSensor > numberOfSensors) {
            selectedSensor = 0;
          }
        }
      }
    }
  }
  lastButtonState = r;
}

// INTERRUPT FUNCTIONS

void overrideInterrupt() {
  // debouncing with unsigned long as unsigned arithmatic is millis() rollover safe
  static unsigned long lastCallTime = 0;
  unsigned long thisCallTime = millis();
  if (thisCallTime - lastCallTime > 75) {
    // DO STUFF HERE
    if(digitalRead(override == HIGH)) {
      purge();
      toSpray = 1;
      exitTimer = thisCallTime;
      state = delaySpray;
    }
  }
  lastCallTime = thisCallTime;
}

void selectInterrupt() {
  // debouncing with unsigned long as unsigned arithmatic is millis() rollover safe
  static unsigned long lastCallTime = 0;
  unsigned long thisCallTime = millis();
  if (thisCallTime - lastCallTime > 75) {
    // DO STUFF HERE
    if (digitalRead(select) == LOW) {
      if (state == stateMenu || state == sensorView) {
        activateCurrentMenuItem();
      } else {
        purge();
        state = stateMenu;
        selectedMenuItem = 0;
      }
    }
  }
  lastCallTime = thisCallTime;
}