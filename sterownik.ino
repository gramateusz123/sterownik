#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);

#define SCL A5
#define SDA A4
#define R1 4
#define R2 2
#define R3 6

#define ENTER 12
#define CANCEL 9
#define LEFT 11
#define RIGHT 10

#define R1_BACK_DURATION 0
#define R2_BACK_DELAY 4
#define R2_BACK_DURATION 6
#define R3_BACK_DELAY 10
#define R3_BACK_DURATION 12

#define R1_FORWARD_DURATION 2
#define R2_FORWARD_DELAY 8
#define R3_FORWARD_DELAY 14
#define R3_FORWARD_DURATION 16

#define TRESHOLD_VALUE 18
#define acs712 A0
long lastSample = 0;
long sampleSum = 0;
int sampleCount = 0;
float vpc = 4.8828125;
// TODO: transform menu to object [{ name: "", address: R1_FORWARD_ETC }]
String menu[] = {
  "Prad graniczny",
  "R1_BACK_DURATION",
  "R1_FORWARD_DURATION",
  "R2_BACK_DELAY",
  "R2_BACK_DURATION",
  "R2_FORWARD_DELAY",
  "R3_BACK_DELAY",
  "R3_BACK_DURATION",
  "R3_FORWARD_DELAY",
  "R3_FORWARD_DURATION"
};

void setup() {

  EEPROM.put(R1_BACK_DURATION, 00);
  EEPROM.put(R2_BACK_DELAY, 100);
  EEPROM.put(R2_BACK_DURATION, 5000);
  EEPROM.put(R3_BACK_DELAY, 200);
  EEPROM.put(R3_BACK_DURATION, 5000);
  EEPROM.put(R1_FORWARD_DURATION, 300);
  EEPROM.put(R2_FORWARD_DELAY, 100);
  EEPROM.put(R3_FORWARD_DELAY, 200);
  EEPROM.put(R3_FORWARD_DURATION, 10000);
  EEPROM.put(TRESHOLD_VALUE, 0.9f);

  Serial.begin(9600);
  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);

  pinMode(ENTER, INPUT);
  pinMode(CANCEL, INPUT);
  pinMode(LEFT, INPUT);
  pinMode(RIGHT, INPUT);

  digitalWrite(R1, HIGH);
  digitalWrite(R2, HIGH);
  digitalWrite(R3, HIGH);

  lcd.init();
  lcd.backlight();
}
float currentApmerage = 0.0f;
bool error = false;
int enter = false;
void loop() {

  enter = digitalRead(ENTER);
  if (enter == HIGH) {
    showMenu();
  }

  //float currentApmerage = measureAmperage();
  float treshold = 0.9f;
  EEPROM.get(TRESHOLD_VALUE, treshold);
  Serial.println(currentApmerage);

  lcd.setCursor(0, 0);
  lcd.print("Aktualny prad:");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(currentApmerage);
  lcd.print(" A");

  if (currentApmerage >= treshold) {// blokada
    emergencyMovement();
    currentApmerage = 0.0f;
  }

  if (error) {
    while (true) {
      lcd.clear();
      lcd.print("Blad! Uruchom");
      lcd.setCursor(0, 1);
      lcd.print("program ponownie");
      delay(20000);
    }
  }

  //  currentApmerage += 0.1f;
  //  delay(1000);
}

void emergencyMovement() {
  lcd.clear();
  lcd.print("Blokada slimaka!");
  bool isBlocked = true;
  for (int i = 0; i < 3; i++) {
    if (!isBlocked) {
      break;
    }
    moveBack();
    isBlocked = testMovement();
  }
  if (isBlocked) {
    digitalWrite(R1, LOW); //r1 on
    error = true;
  }
}

void moveBack() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Trwa cofanie...");
  Serial.println("moveBACK");
  int R2delayTime = 0;
  int R3delayTime = 0;
  int R1durationTime = 0;
  int R2durationTime = 0;
  int R3durationTime = 0;
  EEPROM.get(R2_BACK_DELAY, R2delayTime);
  EEPROM.get(R3_BACK_DELAY, R3delayTime);
  EEPROM.get(R1_BACK_DURATION, R1durationTime);
  EEPROM.get(R2_BACK_DURATION, R2durationTime);
  EEPROM.get(R3_BACK_DURATION, R3durationTime);

  digitalWrite(R1, LOW); // r1 on
  delay(R2delayTime);
  digitalWrite(R2, LOW); // r2 on
  delay(R3delayTime - R2delayTime);
  digitalWrite(R3, LOW); // r3 on
  digitalWrite(R1, HIGH); // r1 off
  delay(R2durationTime - (R3delayTime - R2delayTime));
  digitalWrite(R2, HIGH); // r2 off
  delay(R3durationTime - (R3durationTime - (R3delayTime - R2delayTime)));
  digitalWrite(R3, HIGH); // r3 off
}

bool testMovement() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Usuwanie blokady");
  Serial.println("testMovement");
  int R1durationTime = 0;
  int R3durationTime = 0;
  int R3delayTime = 0;
  float treshold = 0.0f;

  EEPROM.get(R1_FORWARD_DURATION, R1durationTime);
  EEPROM.get(R3_FORWARD_DURATION, R3durationTime);
  EEPROM.get(R3_FORWARD_DELAY, R3delayTime);
  EEPROM.get(TRESHOLD_VALUE, treshold);

  digitalWrite(R1, LOW); // r1 on
  delay(R3delayTime);
  digitalWrite(R3, LOW); // r3 on
  delay(R1durationTime - R3delayTime);
  digitalWrite(R1, HIGH); // r1 off

  unsigned long start = millis();
  bool shouldRun = true;
  bool retryEmergencyMovement = false;
  // 10 sekund obserwacji
  while (shouldRun) {
    retryEmergencyMovement = watch(shouldRun, R3durationTime, treshold, start);
  }
  digitalWrite(R3, HIGH); // r3 off
  return retryEmergencyMovement;
}

bool watch(bool& shouldRun, int& duration, float treshold, unsigned long start) {
  // sprawdz prad
  if (millis() - start >= (duration / 2)) {
    Serial.println("blokada!");
    shouldRun = false;
    return true;
  }

  //  if (measureAmperage() >= treshold) {
  //    shouldRun = false;
  //    return true;
  //  }
  // wylacz while po czasie duration
  if (millis() - start >= duration) {
    shouldRun = false;
  }
  return false;
}

void printToLCD(int row, String text) {
  lcd.setCursor(0, row);
  lcd.print("                ");
  lcd.setCursor(0, row);
  lcd.print(text);
}

void showSubmenu(String submenu) {
  bool showMenu = true;
  int enter = 0;
  int left = 0;
  int right = 0;
  int cancel = 0;
  int value;
  float currentValue;
  bool isTimeValue = submenu == menu[0];
  printToLCD(0, submenu);
  if (isTimeValue) {
    value = digitalRead(submenu);
    printToLCD(0, value + " ms");
  } else {
    currentValue = digitalRead(submenu);
    printToLCD(0, currentValue + " A");
  }
  while (showMenu) {
    enter = digitalRead(ENTER);
    left = digitalRead(LEFT);
    right = digitalRead(RIGHT);
    cancel = digitalRead(CANCEL);
    if (enter == HIGH) {
      Serial.println("ENTER");
    }
    if (left == HIGH) {
      if (cnt != 0) {
        cnt--;
      } else {
        cnt = 9;
      }
      printToLCD(1, menu[cnt]);
      Serial.println("LEFT");
    }
    if (right == HIGH) {
      if (cnt < 10) {
        cnt++;
      } else {
        cnt = 0;
      }
      printToLCD(1, menu[cnt]);
      Serial.println("RIGHT");
    }
    if (cancel == HIGH) {
      Serial.println("CANCEL");
      showMenu = false;
    }
  }
}

void showMenu() {
  bool showMenu = true;
  int cnt = 0;
  int enter = 0;
  int left = 0;
  int right = 0;
  int cancel = 0;
  printToLCD(0, "MENU");
  printToLCD(1, menu[cnt]);
  while (showMenu) {
    enter = digitalRead(ENTER);
    left = digitalRead(LEFT);
    right = digitalRead(RIGHT);
    cancel = digitalRead(CANCEL);
    if (enter == HIGH) {
      Serial.println("ENTER");
    }
    if (left == HIGH) {
      if (cnt != 0) {
        cnt--;
      } else {
        cnt = 9;
      }
      printToLCD(1, menu[cnt]);
      Serial.println("LEFT");
    }
    if (right == HIGH) {
      if (cnt < 10) {
        cnt++;
      } else {
        cnt = 0;
      }
      printToLCD(1, menu[cnt]);
      Serial.println("RIGHT");
    }
    if (cancel == HIGH) {
      Serial.println("CANCEL");
      showMenu = false;
    }
  }
}

float measureAmperage() {
  if (millis() > lastSample + 1) {
    sampleSum += sq(analogRead(acs712) - 512);
    sampleCount++;
    lastSample = millis();
  }

  if (sampleCount == 1000) {
    float mean = sampleSum / sampleCount;
    float value = sqrt(mean);
    float finalVoltage = (value * vpc); //mV
    float finalAmperage = finalVoltage / 100;
    sampleSum = 0;
    sampleCount = 0;
    return finalAmperage;
  }
}
