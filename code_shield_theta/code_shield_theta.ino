/*
  Theta Locomotion Control Algorithm (ESP)
  Developed by Felipe Machado and Mateus Santos da Silva
  Last modification: 07/09/2023
  Version: 0.5

- usable to read speed from hall efect sensor (speedLeftWheel and speedRightWheel)
- control joystick by typing the bits values (writeJoystickManually)

TODO:
[ ] do the feedback loop control to asset desired speed
*/

struct Hall {
  int pin;
  volatile bool endPulse;
  volatile unsigned long timeStart;
  volatile unsigned long timeEnd;
  unsigned long currentTime;
  unsigned long previousTime;
};

Hall ARightHall = { 5, true, 0, 0, 0, 0 };   // white - orange
Hall BRightHall = { 18, true, 0, 0, 0, 0 };  // brown - blue
Hall ALeftHall = { 19, true, 0, 0, 0, 0 };   // gray - orange
Hall BLeftHall = { 21, true, 0, 0, 0, 0 };   // blue - blue


// variables - change radius and stoppedTime accordingly

int stoppedTime = 1000000;  // 1s = 1 000 000
float radiusWheel = 0.165;  // in meters


struct wheel {
  float velLinear;
};

wheel leftWheel;
wheel rightWheel;

int Xchannel = 25;
int Ychannel = 26;
int X_joy;  // = 105;
int Y_joy;  // = 105;


struct robot {
  float trackWidth = 0.5;
  float velLinear;
  float velAngular;
};

// Variables - Digital filter
#define sampleSize 100 // Number of samples that the filter will filter.
#define samplingInterval 1 // Sets the sampling interval in (ms).

unsigned long timer1 = 0; // 


void IRAM_ATTR rightMotorISR(Hall* hall) {  // IRAM_ATTR to run on RAM

  ARightHall.timeStart = BRightHall.timeEnd;
  BRightHall.timeEnd = ARightHall.timeEnd;
  ARightHall.timeEnd = xTaskGetTickCountFromISR();  // 1 tick = 1ms by default

  hall->endPulse = !hall->endPulse;
}

void IRAM_ATTR leftMotorISR(Hall* hall) {

  ALeftHall.timeStart = BLeftHall.timeEnd;
  BLeftHall.timeEnd = ALeftHall.timeEnd;
  ALeftHall.timeEnd = xTaskGetTickCountFromISR();

  hall->endPulse = !hall->endPulse;
}


void setup() {
  Serial.begin(115200);

  pinMode(Xchannel, OUTPUT);
  pinMode(Ychannel, OUTPUT);

  pinMode(ARightHall.pin, INPUT_PULLUP);
  pinMode(BRightHall.pin, INPUT_PULLUP);
  pinMode(ALeftHall.pin, INPUT_PULLUP);
  pinMode(BLeftHall.pin, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(ARightHall.pin), [] {
      rightMotorISR(&ARightHall);
    },
    FALLING);
  attachInterrupt(
    digitalPinToInterrupt(BRightHall.pin), [] {
      rightMotorISR(&BRightHall);
    },
    FALLING);

  attachInterrupt(
    digitalPinToInterrupt(ALeftHall.pin), [] {
      leftMotorISR(&ALeftHall);
    },
    FALLING);
  attachInterrupt(
    digitalPinToInterrupt(BLeftHall.pin), [] {
      leftMotorISR(&BLeftHall);
    },
    FALLING);
}

void loop() {

  // writeJoystickManually();
  // Serial.println("- - - - - - - - - - - - - - - - - - - - - - - - - - - -");

  leftWheel = speedLeftWheel(&ALeftHall, &BLeftHall);
  rightWheel = speedRightWheel(&ARightHall, &BRightHall);

  wheel leftWheelFilter;
  wheel rightWheelFilter;
  
  // Obtains the filtered value for the left wheel (side = 0).
  leftWheelFilter.velLinear = movingAverageFilter(0, 0);
  // Obtains the filtered value for the right wheel (side = 1).
  rightWheelFilter.velLinear = movingAverageFilter(0, 1);
  
  Serial.println("LEFT = " + String(leftWheelFilter.velLinear));
  Serial.println("RIGHT = " + String(rightWheelFilter.velLinear));

  robot theta = wheelsVelocity2robotVelocity(leftWheel.velLinear, rightWheel.velLinear);
  // Serial.println("vLIN = " + String(theta.velLinear));
  // Serial.println("vANG = " + String(theta.velAngular));

  // Serial.println("- - - - - - - - - - - - - - - - - - - - - - - - - - - -");

  float contrVelLin = controle_vel_linear(0.0, theta.velLinear);
  robotVelocity2joystick(contrVelLin, 0.0);
  // writeJoystickManually();
  // Serial.println("- - - - - - - - - - - - - - - - - - - - - - - - - - - -");
}



float controle_vel_linear(float vel_desejada, float vel_medida) {

  float vel_controlada;  // Control output
  float kp = 0.01;             // Proportional gain
  float ki = 0.0001;              // Integral gain
  float kd = 0.001;             // Derivative gain

  float prevError = 0.0;
  float integral = 0.0;
  long prevTime = 0;
  unsigned long sampleTime = 500;  //Sample time in milliseconds

  unsigned long tempoAtual = millis();
  if (tempoAtual - prevTime >= sampleTime) {
    double error = vel_desejada - vel_medida;
    integral += error * (tempoAtual - prevTime);

    double derivative = (error - prevError) / (tempoAtual - prevTime);

    prevError = error;
    prevTime = tempoAtual;

    vel_controlada = kp * error + ki * integral + kd * derivative;

    if (vel_controlada > 0.5) {
      vel_controlada = 0.5;
    } else if (vel_controlada < -0.6) {
      vel_controlada = -0.6;
    }

    Serial.print("vel_desejada: " + String(vel_desejada));
    Serial.print("   vel_medida: " + String(vel_medida));
    Serial.println("   vel_controlada: " + String(vel_controlada));

    return vel_controlada;
  }
}


robot wheelsVelocity2robotVelocity(float leftWheel_velLinear, float rightWheel_velLinear) {
  // https://www.roboticsbook.org/S52_diffdrive_actions.html

  robot localRobot;

  localRobot.velLinear = (rightWheel_velLinear + leftWheel_velLinear) / 2;
  localRobot.velAngular = (rightWheel_velLinear - leftWheel_velLinear) / localRobot.trackWidth;
  // velAngular is negative clockwise on ROS

  return localRobot;
}


void robotVelocity2joystick(float velLinear, float velAngular) {
  float velLinearMAX = 0.5;   // (m/s) going forward
  float velLinearMIN = -0.6;  // (m/s) going reverse

  float velAngularMAX = 1.5;  // (rad/s) 1 rad = 60°
  float velAngularMIN = -1.5;

  Y_joy = 255 * (velLinear - velLinearMIN) / (velLinearMAX - velLinearMIN);
  X_joy = 255 * (velAngular - velAngularMIN) / (velAngularMAX - velAngularMIN);

  dacWrite(Xchannel, X_joy);
  dacWrite(Ychannel, Y_joy);

  // Serial.print("X_joy: " + String(X_joy));
  // Serial.println("      Y_joy: " + String(Y_joy));
}


wheel speedLeftWheel(Hall* Ahall, Hall* Bhall) {
  static wheel localWheel;
  Ahall->currentTime = micros();

  // If the wheels have stopped for stoppedTime, reset hall values
  if (Ahall->currentTime - Ahall->previousTime >= stoppedTime) {

    Ahall->endPulse = true;
    Ahall->timeStart = 0;
    Bhall->timeEnd = 0;
    Ahall->timeEnd = 0;
    Ahall->previousTime = Ahall->currentTime;

    localWheel.velLinear = 0;
  }

  // If there is a complete wheel turn from A Hall sensor
  if (Ahall->timeStart && Ahall->endPulse) {
    int deltaTime = (Ahall->timeEnd - Ahall->timeStart);  // in miliseconds
    float freq = 1 / (deltaTime / 1000.0);                // divide by float to save whole number
    float rpmWheel = freq * (60 / 32.0);
    float angularFrequency = ((rpmWheel * (2 * PI / 60.0)));  // (rad/s)
    localWheel.velLinear = angularFrequency * radiusWheel;    // (m/s)

    if ((Ahall->timeEnd - Bhall->timeEnd) > (Bhall->timeEnd - Ahall->timeStart)) {
      localWheel.velLinear = localWheel.velLinear * (-1);
    }

    // Reset the Hall sensor values and update the previousTime variable
    Ahall->timeStart = 0;
    Bhall->timeEnd = 0;
    Ahall->timeEnd = 0;
    Ahall->previousTime = Ahall->currentTime;
  }

  return localWheel;
}

wheel speedRightWheel(Hall* Ahall, Hall* Bhall) {
  static wheel localWheel;
  Ahall->currentTime = micros();

  // If the wheels have stopped for stoppedTime, reset hall values
  if (Ahall->currentTime - Ahall->previousTime >= stoppedTime) {

    Ahall->endPulse = true;
    Ahall->timeStart = 0;
    Bhall->timeEnd = 0;
    Ahall->timeEnd = 0;
    Ahall->previousTime = Ahall->currentTime;

    localWheel.velLinear = 0;
  }

  // If there is a complete wheel turn from A Hall sensor
  if (Ahall->timeStart && Ahall->endPulse) {
    int deltaTime = (Ahall->timeEnd - Ahall->timeStart);  // in miliseconds
    float freq = 1 / (deltaTime / 1000.0);                // divide by float to save whole number
    float rpmWheel = freq * (60 / 32.0);
    float angularFrequency = ((rpmWheel * (2 * PI / 60.0)));  // (rad/s)
    localWheel.velLinear = angularFrequency * radiusWheel;    // (m/s)

    if ((Ahall->timeEnd - Bhall->timeEnd) > (Bhall->timeEnd - Ahall->timeStart)) {
      localWheel.velLinear = localWheel.velLinear * (-1);
    }

    // Reset the Hall sensor values and update the previousTime variable
    Ahall->timeStart = 0;
    Bhall->timeEnd = 0;
    Ahall->timeEnd = 0;
    Ahall->previousTime = Ahall->currentTime;
  }

  return localWheel;
}


void writeJoystickManually() {
  if (Serial.available() > 0) {  // No Line Ending

    int X_joy = Serial.parseFloat();
    int Y_joy = Serial.parseFloat();

    Serial.println("X: " + String(X_joy));
    Serial.println("Y: " + String(Y_joy));

    dacWrite(Xchannel, X_joy);
    dacWrite(Ychannel, Y_joy);
  }
}

void sampling() {
  // Checks the sampling time.
  if(millis() - timer1 > samplingInterval) {
    // If the sampling time has occurred, it sends one to the moving average filter function in order to update the output value to a new filtered value.
    movingAverageFilter(1, 0);
    movingAverageFilter(1, 1);

    timer1 = millis(); // Stores the last count.
  }
}

// Moving average function (uses the static variable, meaning it saves the content without losing it on the next function execution).
float movingAverageFilter(bool updateOutput, bool side){
  // Wheel right
  static float previousReadingsRight[sampleSize]; // Circular Buffer.
  static int positionRight = 0; // Current reading position, which should be saved.
  static long amountRight = 0; // Total sum of the circular buffer.
  static float averageRight = 0; // Stores the result of the moving average filter.

  // Wheel left
  static float previousReadingsLeft[sampleSize];
  static int positionLeft = 0;
  static long amountLeft = 0;
  static float averageLeft = 0;

  static bool clearVector = 1; // Enables or disables vector clearing.

  // Clears the vector the first time the function is executed.
  if(clearVector) {
    for(int i = 0; i <= sampleSize; i++){
      previousReadingsRight[i] = 0;
      previousReadingsLeft[i] = 0;
    }

    clearVector = 0;
  }

  if(side){
    // Wheel right (side = 1)
    if(updateOutput == 0)
      // If the parameter received in the function is zero, it returns only the previously calculated mean value.
      return ((double)averageRight);
    else{
      // If it's equal to one, it means it's at the sampling time, and then it updates the average variable.
      amountRight = rightWheel.velLinear - previousReadingsRight[positionRight % sampleSize];
      previousReadingsRight[positionRight % sampleSize] = rightWheel.velLinear;
      averageRight = (float)amountRight / (float)sampleSize;
      positionRight = (positionRight + 1) % sampleSize; 

      return((double)averageRight);
    }
  }else{
    // Wheel left (side = 0)
    if(updateOutput == 0)
      return ((double)averageLeft);
    else{
      amountLeft = leftWheel.velLinear - previousReadingsLeft[positionLeft % sampleSize];
      previousReadingsLeft[positionLeft % sampleSize] = leftWheel.velLinear;
      averageLeft = (float)amountLeft / (float)sampleSize;
      positionLeft = (positionLeft + 1) % sampleSize;

      return((double)averageLeft);
    }
  }
}
