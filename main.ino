#include <Servo.h>
#include <TinyStepper_28BYJ_48.h>
#include <Adafruit_SSD1306.h>

//SETTINGS
#define signalOutPin 2
#define signalInPin 3
#define lightSensorPin 23
#define servoStartPin 14
#define btnStartPin 4
#define lightSensorSensitivity 1.02 //UNUSED
#define lightIntensity 968
#define timeoutTime 40000 //ms
#define servoAngle 70 //degrees
//-----------------

Adafruit_SSD1306 display(128, 64, &Wire, -1);
TinyStepper_28BYJ_48 stepMotor;
Servo servoMotors[4];

bool active = false;
bool userControlled = false;
int elevatorCycle = 0; //0=down, 1=up, 2=down
u_int32_t timer = 0;
int initBrightness = 0; //UNUSED //Darkest brightness when no ball should be detected
u_int32_t lastSignalInState = 0;
int32_t btnDebounce[4] = {0,0,0,0};
int btnLastState[4] = {0,0,0,0};
u_int32_t highscore = timeoutTime*2000; //microsec, lower value = better score, as score is the time it took
bool reactivated = false;
bool ballDetected = false;

void setup() {
  //STEPMOTOR
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  
  //LIGHT SENSOR
  pinMode(lightSensorPin, INPUT);
  //SIGNAL IN
  pinMode(signalInPin, INPUT);
  //SIGNAL OUT
  pinMode(signalOutPin, OUTPUT);
  //LED

  //
  digitalWrite(signalOutPin, HIGH);
  Serial.begin(9600);

  //SETUP DISPLAY
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(2); // Pixel scale
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  //display.cp437(true);
  display.display();
 
  //SETUP STEPMOTOR
  stepMotor.connectToPins(9, 10, 11, 12);
  stepMotor.setSpeedInStepsPerSecond(1024);
  stepMotor.setAccelerationInStepsPerSecondPerSecond(512);
  stepMotor.disableMotor();
  stepMotor.setCurrentPositionInSteps(0);
  //Serial.println("Step motor configureation complete");

  //SERVOMOTORS + BUTTONS
  for (int i=0; i<4; i++) {
    pinMode(i+servoStartPin, OUTPUT);
    pinMode(i+btnStartPin, INPUT);
    servoMotors[i].attach(i+servoStartPin);
    servoMotors[i].write(0);
  }

  initBrightness = analogRead(lightSensorPin);
  Serial.print("Initial brightness: ");
  Serial.println(initBrightness);

  delay(2000);
  //Show that setup is complete
  display.clearDisplay();
  display.display();
}

void moveServo(int pin, bool up) {
  if (servoMotors[pin].read() != (int)up*servoAngle) {
    btnDebounce[pin] = (up ? 1 : -1)*millis();
    /*Serial.print("Servo ");
    Serial.print(pin+servoStartPin);
    Serial.print(" ");
    Serial.println((int)up*servoAngle);*/
    servoMotors[pin].write((int)up*servoAngle);
  }
}

void updateTimerDisplay(uint32_t time, String prefix) {
  uint32_t ms = floor((time-floor(time/1000000)*1000000)/10000);
  uint32_t sec = floor(time/1000000);

  String secStr = String(sec);
  if (2-secStr.length() >= 1) {
    for (uint8_t i=0; i<2-secStr.length(); i++) {
      secStr = "0" + secStr;
    }
  }
  String msStr = String(ms);
  if (2-msStr.length() >= 1) {
    for (uint8_t i=0; i<2-msStr.length(); i++) {
      msStr = "0" + msStr;
    }
  }
  String timerStr = prefix + secStr + " : " + msStr;

  display.clearDisplay();
  display.setCursor(20, 5);
  display.println(timerStr);
  display.display();
}

elapsedMillis intervallTimer;
void reset(bool timeout) {
  Serial.println("Resetting...");
  u_int32_t resetStart = millis();
  userControlled = false;

  digitalWrite(signalOutPin, LOW); //Send out signal
  display.clearDisplay();

  if (stepMotor.getCurrentPositionInSteps() != 0) { //Reset elevator
    Serial.println("Resetting elevator");
    Serial.println(stepMotor.getCurrentPositionInSteps());
    stepMotor.moveToPositionInSteps(0); //Motor disables at end of reset function
    stepMotor.setCurrentPositionInSteps(0);
  }


  if (timeout == true) { //Wait until the ball reaches the elevator
    display.setCursor(20, 5);
    display.println("You failed!");
    display.display();
    for (int i=0; i<4; i++) {
      moveServo(i, true);
    }
   
    Serial.println(initBrightness*lightSensorSensitivity);
    Serial.println(analogRead(lightSensorPin));
    while(analogRead(lightSensorPin) < lightIntensity) {//initBrightness*lightSensorSensitivity) {
      //Serial.println(analogRead(lightSensorPin));
      delay(100);
    }
    Serial.println(analogRead(lightSensorPin));
    Serial.println("Ball detected");
  }

  for (int i=0; i<4; i++) {
    moveServo(i, false);
  }

  if (timeout == false) { //Show time if user succeeded
    updateTimerDisplay(timer, "Your time: "); //Text is in english because åäö is wrongly desplayed (even with cp437)
    delay(6000);
    if (timer < highscore) {
      highscore = timer;
      display.clearDisplay();
      display.setCursor(0, 5);
      display.println("You bet the highscore!");
      display.display();
    } else {
      updateTimerDisplay(highscore, "Best time: ");
    }
  }
  
  if (millis()-resetStart < 1000) {
    delay(1000-(millis()-resetStart)); //Ensure the signal is given at least 1 s
  }
 

  digitalWrite(signalOutPin, HIGH);
  delay(4000);
  display.clearDisplay();
  display.display();

  for (int i=0; i<4; i++) {
    btnDebounce[i] = 0;
    btnLastState[i] = 0;
  }
  stepMotor.disableMotor();
  ballDetected = false;
  elevatorCycle = 0;
  timer = 0;
  active = false;
  Serial.println("Reset complete");
}


void loop() {
  u_int32_t startTime = micros();
  //Serial.println(analogRead(lightSensorPin));
  //Kolla in signal
  if (digitalRead(signalInPin) == HIGH || reactivated == true) {
    if (active == true && reactivated == false && millis()-lastSignalInState >= 5000) {
      //Save state
      //Serial.println("Active state re-recived");
      reactivated = true;
    } else if (active == false) {
      Serial.println("Activating...");
      lastSignalInState = millis();
      reactivated = false;
      active = true; //Activate
      elevatorCycle = 0;
      stepMotor.setCurrentPositionInSteps(0);
      stepMotor.setupMoveInSteps(1024*25);
      display.clearDisplay();
      display.setCursor(20, 5);
      display.println("Get ready...");
      display.display();
    }
  }

  //Elevator movement
  if (active == true) {
    if(!stepMotor.motionComplete()) {
      //Serial.println(stepMotor.getCurrentPositionInSteps());
      stepMotor.processMovement();
      if (stepMotor.getCurrentPositionInSteps() <= 2048*4 && elevatorCycle == 0) {
        userControlled = true;
      }
    } else if (elevatorCycle < 2) {
      Serial.println(analogRead(lightSensorPin));
      //Serial.println(initBrightness*lightSensorSensitivity);
      if (analogRead(lightSensorPin) >= lightIntensity) {ballDetected = true;}
      if (elevatorCycle == 0 && analogRead(lightSensorPin) < lightIntensity-20 && ballDetected == true) {//initBrightness*lightSensorSensitivity) { //Elevator is up
        elevatorCycle++;
        Serial.println("Move elevator down");
        stepMotor.setupMoveInSteps(0);
        display.clearDisplay();
        display.setCursor(40, 22);
        display.println("GO!");
        display.display();
      } else if (elevatorCycle == 1) { //Elevator is down
        elevatorCycle++;
        Serial.println("Elevator finished");
        stepMotor.setCurrentPositionInSteps(0);
        stepMotor.disableMotor();
      }
    }
  }

  //Check buttons
  if (active == true && userControlled == true) {
    for (int i=0; i<4; i++) {
      int btnState = digitalRead(i+btnStartPin);
      if (btnState == HIGH && btnLastState[i] == LOW && btnDebounce[i] == 0 && (i != 3 || elevatorCycle >= 2)) {
        Serial.print("Btn down ");
        Serial.println(i);
        moveServo(i, true);
      } else if (btnState == LOW && btnLastState[i] == HIGH && btnDebounce[i] > 0) {
        Serial.print("Btn up ");
        Serial.println(i);
        moveServo(i, false);
      }
      if (btnState == HIGH && btnDebounce[i] > 0 && millis()-btnDebounce[i] >= 3000) { //Timeout
        Serial.print("Btn timeout ");
        Serial.println(i);
        moveServo(i, false);
      }
      if (btnDebounce[i] < 0 && millis()+btnDebounce[i] >= 500) { //Debounce
        btnDebounce[i] = 0;
      }
      btnLastState[i] = btnState;
    }
  }
  //Read light signal
  if (active == true && elevatorCycle >= 2 && analogRead(lightSensorPin) >= lightIntensity) {//initBrightness*(2-lightSensorSensitivity)) { //Användaren har fått kulan till hissen
    //Serial.println(initBrightness*(2-lightSensorSensitivity));
    Serial.println(analogRead(lightSensorPin));
    reset(false);
  }

  //Check timer
  if (timer >= timeoutTime*1000) { //Ball has not reached the elevator within the timelimit
    Serial.println("Timer timeout");
    reset(true);
  }
  if (active == true && elevatorCycle >= 1) {
    /*updateTimerDisplay();
    intervallTimer = 0;*/
    timer += (micros()-startTime);
  } else if (active == false && intervallTimer >= 1000) { //Maintenance functions
    //initBrightness = analogRead(lightSensorPin);
    if (digitalRead(btnStartPin) == HIGH && digitalRead(btnStartPin+1) == HIGH && digitalRead(btnStartPin+2) == HIGH && digitalRead(btnStartPin+3) == HIGH) {
      for (int i=0; i<4; i++) {
        servoMotors[i].write(servoAngle);
      }
      delay(5000);
      for (int i=0; i<4; i++) {
        servoMotors[i].write(0);
      }
    } else if (digitalRead(btnStartPin) == HIGH && digitalRead(btnStartPin+1) == HIGH) {
      stepMotor.moveRelativeInSteps(2048);
      stepMotor.setCurrentPositionInSteps(0);
      stepMotor.disableMotor();
    } else if (digitalRead(btnStartPin+2) == HIGH && digitalRead(btnStartPin+3) == HIGH) {
      stepMotor.moveRelativeInSteps(-2048);
      stepMotor.setCurrentPositionInSteps(0);
      stepMotor.disableMotor();
    }
    intervallTimer = 0;
  }
}
