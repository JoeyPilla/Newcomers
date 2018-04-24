#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "MPU6050.h"
#include "Wire.h"
 
#include <Adafruit_MotorShield.h>
#include <Wire.h>
 
// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
 
// Select which 'port' M1, M2, M3 or M4. In this case, M1
Adafruit_DCMotor *motor1 = AFMS.getMotor(4);
Adafruit_DCMotor *motor2 = AFMS.getMotor(2);
Adafruit_DCMotor *motor3 = AFMS.getMotor(3);
// Disposal motor
//Adafruit_DCMotor *motor4 = AFMS.getMotor(4);
 
MPU6050 mpu;
 
// MPU control/status variables
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
 
// Orientation/motion variables
Quaternion q;           // [w, x, y, z]         quaternion container
VectorFloat gravity;    // [x, y, z]            gravity vector
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container
 
// Interrupt detection routine:
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
  mpuInterrupt = true;
}

float Rangle, Pangle;
float PbalancePoint = 7.3, RbalancePoint = .2;       // An offset to couteract the misaligned center of gravity
 
void setup() {
  mpuSetup();
  motorSetup();
}
 
// ==========================================================================================
//                                       Main Loop
// ==========================================================================================
 
void loop() {
  mpuLoop();
 
  // Converting the angles into degrees (P for pitch, R for roll):
  Rangle = (ypr[1] * 180/M_PI) - RbalancePoint;
  Pangle = (ypr[2] * 180/M_PI) - PbalancePoint;
 
// ==========================================================================================
//                                 Print Statements
// ==========================================================================================

  Serial.print("Angles (P R):\t");
  Serial.print(Pangle);
  Serial.print("\t");
  Serial.print(Rangle);
  Serial.print("\t");
 
// ==========================================================================================
//                    Setting the speed and direction of the motors
// ==========================================================================================
 
double magnitude = sqrt(sq(Pangle) + sq(Rangle));
double dir       = -magnitude;
dir = map(abs(dir),0,12,140,255);
 
double theta = atan2((Rangle * M_PI / 180),(Pangle * M_PI / 180));
 
if(theta < 0){
  theta = (2 * M_PI) + theta;
}
 
double m1Speed   = (dir * sin((90*M_PI/180)  - theta));
double m2Speed   = (dir * sin((216*M_PI/180) - theta));
double m3Speed   = (dir * sin((340*M_PI/180) - theta));

/*if(magnitude < .5){
  motor1->run(RELEASE);
  motor2->run(RELEASE);
  motor3->run(RELEASE);
}*/
 
if(m1Speed > 0) {       motor1->run(FORWARD);}
else if(m1Speed == 0) { motor1->run(RELEASE);}
else {                  motor1->run(BACKWARD);}
if(m2Speed > 0) {       motor2->run(FORWARD);}
else if(m1Speed == 0) { motor2->run(RELEASE);}
else {                  motor2->run(BACKWARD);}
if(m3Speed > 0) {       motor3->run(FORWARD);}
else if(m1Speed == 0) { motor3->run(RELEASE);}
else {                  motor3->run(BACKWARD);}
 

if(m1Speed > 255){m1Speed = 255;}
if(m2Speed > 255){m2Speed = 255;}
if(m3Speed > 255){m3Speed = 255;}
 
motor1->setSpeed(abs(m1Speed));
motor2->setSpeed(abs(m2Speed));
motor3->setSpeed(abs(m3Speed));
 
 
  Serial.print("Theta:\t");
  Serial.print(theta * 180 / M_PI);
  Serial.print("\t");
  Serial.print("Magnitude:\t");
  Serial.print(magnitude);
  Serial.print("\t");
  Serial.print("M1 Speed:\t");
  Serial.print(m1Speed);
  Serial.print("\t");
  Serial.print("M2 Speed:\t");
  Serial.print(m2Speed);
  Serial.print("\t");
  Serial.print("M3 Speed:\t");
  Serial.print(m3Speed);
  Serial.print("\t");
  Serial.print("\n");
}
 
 
// ==========================================================================================
//                               Setup Functions and other functions
// ==========================================================================================
 
void motorSetup(){
  // Motor Shield: create with the default frequency 1.6KHz
  AFMS.begin();
 
  // Set the speed to start, from 0 (off) to 255 (max speed)
  motor1->setSpeed(255);
  motor2->setSpeed(255);
  motor3->setSpeed(255);
}
 
void mpuSetup(){
  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
  #endif
 
  // initialize serial communication
  Serial.begin(115200);
 
  // initialize device
  Serial.println(F("Initializing I2C devices..."));
  mpu.initialize();
 
  // verify connection
  Serial.println(F("Testing device connections..."));
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
 
  // load and configure the DMP
  Serial.println(F("Initializing DMP..."));
  devStatus = mpu.dmpInitialize();
 
  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXGyroOffset(85);
  mpu.setYGyroOffset(42);
  mpu.setZGyroOffset(-24);
  mpu.setXAccelOffset(353);
  mpu.setYAccelOffset(-4267);
  mpu.setZAccelOffset(1113);
 
  // turn on the DMP, now that it's ready
  Serial.println(F("Enabling DMP..."));
  mpu.setDMPEnabled(true);
 
  // enable Arduino interrupt detection
  Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
  attachInterrupt(0, dmpDataReady, RISING);
  mpuIntStatus = mpu.getIntStatus();
 
  // set our DMP Ready flag so the main loop() function knows it's okay to use it
  Serial.println(F("DMP ready! Waiting for first interrupt..."));
  dmpReady = true;
 
  // get expected DMP packet size for later comparison
  packetSize = mpu.dmpGetFIFOPacketSize();
}
 
void mpuLoop(){
  // if programming failed, don't try to do anything
  if (!dmpReady) return;
 
  // wait for MPU interrupt or extra packet(s) available
  while (!mpuInterrupt && fifoCount < packetSize) {}
 
  // reset interrupt flag and get INT_STATUS byte
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();
 
  // get current FIFO count
  fifoCount = mpu.getFIFOCount();
 
  // check for overflow (this should never happen unless our code is too inefficient)
  if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();
    Serial.println(F("FIFO overflow!"));
 
    // otherwise, check for DMP data ready interrupt (this should happen frequently)
  } else if (mpuIntStatus & 0x02) {
    // wait for correct available data length, should be a VERY short wait
    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
 
    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);
       
    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    fifoCount -= packetSize;
   
    // display Euler angles in degrees
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
  }
}
