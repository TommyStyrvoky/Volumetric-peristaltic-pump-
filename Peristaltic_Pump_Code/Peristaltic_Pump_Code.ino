//Peristaltic pump sketch by Tommy Styrvoky
#include <TimerOne.h>//https://github.com/arduino-libraries/LiquidCrystal
#include <Bounce2.h>// https://github.com/thomasfredericks/Bounce2
Bounce debouncer = Bounce();
#define BUTTON 4
#include<LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
#include <ClickEncoder.h>//https://github.com/0xPIT/encoder/tree/arduino
ClickEncoder encoder(6, 5, 4, 4);
int16_t oldEncPos, encPos;
const int STEP = 3;//Stepper controller step
const int DIR = 2;//Stepper controller direction
String Menu[] = {"Pump mode", "Flow", "Direction", "Volume", "Deliver", "Calibration vol.", "Blowout Volume"};
String Pumpmode[] = {"Off             ", "Continouous       ", "Volumetric        "};
String SerialData;
int MenuValue = 0, PumpMode = 0, i = 0;
boolean ButtonValue = LOW, Direction = LOW;
int StepsPermL = 2080;// internal value for calibration of the pump
int MinStepTime = 600;//minimum step duration
int ms = MinStepTime;// time in microseconds
int TimePerHStep = 0;//time in milliseconds per full step
double VolumeToDeliver;//volume to deliver in volumetric mode
int VolumeToDeliverInt;
int Blowoutint = 15;
double Blowout = Blowoutint / 100.00;
boolean Deliver = LOW;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  debouncer.attach(BUTTON);
  debouncer.interval(5);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder.setAccelerationEnabled(true);
  oldEncPos = -1;
}
void loop() {
  debouncer.update();
  int value = debouncer.read();
  encPos += encoder.getValue();
  if (Serial.available() > 0) {
    SerialInterface();
  }
  ButtonMode();
  while (encPos != oldEncPos || ButtonValue == HIGH) {
    delay(200);
    ButtonMode();
    oldEncPos = encPos;
    LCDMenu();
  }
  Pump();
}
void SerialInterface() { // Serial data interface with pump
  if (i > 5) {
    i = 0;
  }
  SerialData = Serial.readString();    // read the incoming data
  SerialData.remove((SerialData.length() - 1));
  switch (i) {//assign to pump setting
    case 0:
      Serial.println("PumpMode");
      PumpMode = SerialData.toInt();
      LCDMenu();
      break;
    case 1:
      Serial.println("ms");
      ms = SerialData.toInt();//ms per step
      LCDMenu();
      break;
    case 2:
      Serial.println("µs");
      TimePerHStep = SerialData.toInt(); //µs per step
      LCDMenu();
      break;
    case 3:
      Serial.println("Direction");
      Direction = SerialData.toInt();//0 or 1
      LCDMenu();
      break;
    case 4:
      Serial.println("VolumeToDeliver");
      VolumeToDeliver = SerialData.toDouble();//0.00-1000.00
      LCDMenu();
      break;
    case 5:
      Serial.println("Deliver");
      Deliver = SerialData.toInt();// 0 or 1
      LCDMenu();
      break;
  }
  i++;
}
void ButtonMode() {
  if (digitalRead(BUTTON) == LOW) {
    ButtonValue = ! ButtonValue;
    delay(250);
  }
}
void LCDMenu() {//LCD menu top line
  lcd.clear();
  MenuValue = constrain(MenuValue , 0 , 6);
  if (ButtonValue == LOW) {
    MenuValue = encPos;
  }
  if (MenuValue < 0) {
    MenuValue = 0;
    encPos = MenuValue;
  }
  if (MenuValue > 6) {
    MenuValue = 6;
    encPos = MenuValue;
  }
  lcd.setCursor(0, 0);
  lcd.print(Menu[MenuValue]);
  lcd.setCursor(0, 1);
  LCDbottom();
}
void LCDbottom() { //LCD menu bottom line
  ms = constrain(ms, 0, 1000);
  VolumeToDeliver = constrain(VolumeToDeliver, 0.00 , 1000.00);
  double Flow = ((60000.000 / 2) / (TimePerHStep + (ms / 1000.00)))  / StepsPermL; //flowrate of the pump in mL/min
  Flow = constrain(Flow, 0, 25);
  switch (MenuValue) {
    case 0:
      if (ButtonValue == HIGH) {
        PumpMode += encoder.getValue();
        if (PumpMode < 0) {
          PumpMode = 0;
          encPos = PumpMode;
        }
        else if (PumpMode > 2) {
          PumpMode = 2;
          encPos = PumpMode;
        }
      }
      lcd.print(Pumpmode[PumpMode]);
      break;
    case 1:
      if (ButtonValue == HIGH) {
        ms  -= encoder.getValue();
        if (ms <= 0) {
          TimePerHStep --;
          ms = 1000;
        }
        else if (ms >= 1000) {
          TimePerHStep ++;
          ms = 0;
        }
      }
      lcd.print(Flow);
      lcd.print("mL/min");
      break;
    case 2:
      if (Direction == LOW) {
        lcd.print("CW");
      }
      else {
        lcd.print("CCW");
      }
      if (ButtonValue == HIGH) {
        if ( encoder.getValue()  != 0) { //Direction controls
          Direction = !Direction;
          digitalWrite(DIR, Direction);
        }
        else {
          digitalWrite(DIR, Direction);
        }
        break;
      case 3:
        if (ButtonValue == HIGH) {//  volume to deliver
          VolumeToDeliverInt += 5 * encoder.getValue();
          VolumeToDeliver = VolumeToDeliverInt / 100.00;
          if (VolumeToDeliver < 0) {
            VolumeToDeliver = 0;
          }
          else if (VolumeToDeliver > 1000) {
            VolumeToDeliver = 1000;
          }
        }
        lcd.print(VolumeToDeliver);
        lcd.print("mL");
        break;
      case 4:
        if (ButtonValue == HIGH && PumpMode == 2) {
          Deliver = HIGH;
          ButtonValue = LOW;
        }
        if (Deliver == HIGH) {
          lcd.print("Delivering...   ");
        }
        else {
          lcd.print("Volume          ");
        }
        break;
      case 5:
        if (ButtonValue == HIGH) {//StepsPermL
          StepsPermL += encoder.getValue();
        }
        lcd.print(StepsPermL);
        lcd.print("Steps");
        break;
      case 6:
        if (ButtonValue == HIGH) {// Blowoutvolume
          Blowoutint += encoder.getValue();
          Blowout = Blowoutint / 100.00;
        }
        Serial.print(Blowout);
        lcd.print(Blowout);
        lcd.print("mL");
        break;
      }
  }
}
void Pump() {// Pump mode
  int BlowoutVolume = Blowout * StepsPermL; //Blowout volume
  long Totali = VolumeToDeliver * StepsPermL; // number of steps needed for pump to deliver specified volume
  if (PumpMode == 1) {// Continuous volume delivery mode
    Step();
  }
  if (PumpMode == 2) {
    if (Deliver == HIGH) {// Specific volume delivery mode
      for (int i = 0; i < (Totali + BlowoutVolume); i++) {//Volume delivery, and first part of blowout
        digitalWrite(STEP, HIGH);
        Step();
      }
      for (int i = 0; i < BlowoutVolume; i++) { //reverses directon to counter added blowout volume when done
        digitalWrite(DIR, !Direction);
        Step();
      }
      digitalWrite(DIR, Direction);
      Deliver = LOW;
      LCDMenu();
    }
  }
}
void Step() {//Step timing
  digitalWrite(STEP, HIGH);
  if (ms < 600 && TimePerHStep < 1) {
    ms = MinStepTime;
    delayMicroseconds(ms);
  }
  else {
    delayMicroseconds(ms);
    delay(TimePerHStep);
  }
  digitalWrite(STEP, LOW);
  if (ms < 600 && TimePerHStep < 1) {
    ms = MinStepTime;
    delayMicroseconds(ms);
  }
  else {
    delayMicroseconds(ms);
  }
}
void timerIsr() {
  encoder.service();
}
