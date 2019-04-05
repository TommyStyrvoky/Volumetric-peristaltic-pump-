//Peristaltic pump sketch by Tommy Styrvoky
#include <TimerOne.h>//https://github.com/arduino-libraries/LiquidCrystal
#include <Bounce2.h>// https://github.com/thomasfredericks/Bounce2
Bounce debouncer = Bounce();
#define BUTTON 4
#include <EEPROM.h>
#include<LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
#include <ClickEncoder.h>//https://github.com/0xPIT/encoder/tree/arduino
ClickEncoder encoder(6, 5, 4, 4);
int16_t oldEncPos, encPos;
const int STEP = 3;//Stepper controller step
const int DIR = 2;//Stepper controller direction
String Menu[] = {"Pump mode", "Flow", "Direction", "Volume", "Deliver", "Calibration vol.", "Blowout volume"};
String Pumpmode[] = {"Off             ", "Continouous       ", "Volumetric        "};
String SerialData;
int MenuValue = 0, PumpMode = 0;
boolean ButtonValue = LOW, Direction = LOW;
int StepsPermL = 2080;// internal value for calibration of the pump
int MinStepTime = 400;//minimum step duration
int ms = MinStepTime;// time in microseconds
int TimePerHStep = 0;//time in milliseconds per full step
double VolumeToDeliver;//volume to deliver in pump mode 3
int VolumeToDeliverInt;
int Blowoutint = 15;
double Blowout = 0.15;
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
    LCDMenu();
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
void SerialInterface() {
  SerialData = Serial.readString();    // read the incoming data
  PumpMode = SerialData.substring(0, 1).toInt();
  ms = SerialData.substring(1, 4).toInt();
  TimePerHStep = SerialData.substring(4, 6).toInt();
  Direction = SerialData.substring(6, 7).toInt();
  digitalWrite(DIR, Direction);
  VolumeToDeliver = SerialData.substring(7, 14).toDouble();
  Deliver = SerialData.substring(14, 15).toInt();
}
void ButtonMode() {
  if (digitalRead(BUTTON) == LOW) {
    ButtonValue = ! ButtonValue;
    delay(250);
  }
}
void LCDMenu() {//menu
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
void LCDbottom() {
  ms = constrain(ms, 0, 1000);
  VolumeToDeliver = constrain(VolumeToDeliver, 0.00 , 1000.00);
  double Flow = ((60000.00/2) / (TimePerHStep + (ms / 1000.00)))  / StepsPermL; //flowrate of the pump in mL/min
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
      if (Direction == LOW) { // Direction
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
        if (ButtonValue == HIGH) {// volume to deliver
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
        if (ButtonValue == HIGH) {// StepsPermL
          StepsPermL += encoder.getValue();
        }
        //EEPROM.write(StepsPermL, 1);
        lcd.print(StepsPermL);
        lcd.print("mL");
        break;
      case 6:
        if (ButtonValue == HIGH) {// Blowoutvolume
          Blowoutint += encoder.getValue();
          //EEPROM.write(Blowoutint, 2);
          Blowout = Blowoutint / 100.00;
        }
        lcd.print(Blowout);
        lcd.print("mL");
        break;
      }

  }
}
void Pump() {// Pump settings
  int BlowoutVolume = Blowout * StepsPermL; //Blowout volume
  double Flow = (60000.00 / (TimePerHStep + (ms / 1000.00)))  / StepsPermL; //flowrate of the pump in mL/min
  long Totali = VolumeToDeliver * StepsPermL; // number of steps needed for pump to deliver specified volume
  if (PumpMode == 1) {// Continuous volume delivery mode
    Step();
  }
  if (PumpMode == 2 && Deliver == HIGH) { // Specific volume delivery mode
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
void Step() {//Step timing
  digitalWrite(STEP, HIGH);
  if (ms < MinStepTime && TimePerHStep < 1) {
    ms = MinStepTime;
    delayMicroseconds(ms);
  }
  else {
    delayMicroseconds(ms);
    delay(TimePerHStep);
  }
  digitalWrite(STEP, LOW);
  if (ms < MinStepTime && TimePerHStep < 1) {
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
