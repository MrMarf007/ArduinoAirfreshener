// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int led_green = 13, led_red = 12, temp = 11, sprayer = 10, motion_trig = 9, motion_echo = 8;

// analog ports
const int aButtons = 0, aMagCon = 1, aLight = 2;

// ports for display
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
int a=0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  pinMode(led_green, OUTPUT);
  pinMode(led_red, OUTPUT);
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
}

void loop() {
  a = analogRead(aLight);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("  analogRead() ");
  lcd.setCursor(0,1);
  lcd.print("  value is :");
  lcd.print(a);
  delay(250);

  // digitalWrite(led_green, HIGH);  // turn the LED on (HIGH is the voltage level)
  // digitalWrite(led_red, LOW);
  // delay(1000);                      // wait for a second
  // digitalWrite(led_green, LOW);
  // digitalWrite(led_red, HIGH);   // turn the LED off by making the voltage LOW
  // delay(1000);                      // wait for a second
}