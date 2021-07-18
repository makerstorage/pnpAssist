

//    PNP Asist Firmware ver0.9
//    Designed for use on the Arduino UNO
//    Copyright 2021 2022 MakerStorage LLC.
//   
//    LICENSE INFORMATION
//
//    This work is licensed under the:
//    "Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License"
//
//    To view a copy of this license, visit:
//      http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
//
//    For updates and to download the lastest version, visit:
//      http://github.com/MakerStorage or
//      http://pnpassist.com
//
//    The above copyright notice and this permission notice shall be
//    included in all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
//    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <math.h>
#include <TimerOne.h>
#include "U8glib.h"
#include <SD.h>
#include <SPI.h>

// input pnp File
File pnpFile;

// we read the line from file to this var
String fileLine;
boolean SDfound;


// parsing vars
char fullMsg[ 120 ];
const uint8_t MAX_FIELD_COUNT = 6;
char *field[ MAX_FIELD_COUNT ];
uint8_t fieldCount;

// field[0] name of the component C1
// field[1] X coordinate
// field[2] Y coordinate
// field[3] Angle
// field[4] component value
// field[5] component package

//oled
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0);


// display buffer
char bufferX [20];
char bufferY [20];
char bufferZ [20];


//#define SPEED 2000
#define StepPerMM 800 //

// Stepper Motors Outputs
#define ENABLE 5 // same out for 2 motors
#define STEP   4 // same out for 2 motors
#define A_DIR    8
#define B_DIR    7

// Movement states
#define MOVING    0
#define ARRIVED   1


//length of y axis
#define MAX_Y_LENGTH 250

#define CENTER_Y_OFFSET 37800

#define SEARCH_HOME_SWITCH_SPEED 3800
#define SLOW_HOMING_SPEED 500

// homing trigered flag
volatile bool FindZero = false;

//SD CSPIN
#define CSPIN 6

//button 
#define NEXT_BUTTON_PIN A0

//button y home switch
const byte yHomePin = 2;



// Stepper speeds in RPM
int iDriveSpeed = 5000;

volatile long CurPos         = 0;
volatile long TargetPos      = 0;

// Motion state variables
volatile int Status     = ARRIVED;

int lastStepPulse   = LOW;


float mAngle = 0; //in degrees
float mRadius = 0; 
float eskiAngle = 90; // starting angle is 90
float eskiRadius = 0;

void waitForButton(){ 
  int val = digitalRead(NEXT_BUTTON_PIN);
  
  //Wait for the button
  
  while(val == LOW) {
    val = digitalRead(NEXT_BUTTON_PIN);
  }
  
  delay(10);  
}


void CalcAngleRadius(float x, float y){
  mAngle = atan2(y, x) * 180/PI; // use atan2 to get the quadrant
  Serial.print("CalcAngleRadius mAngle: ");
  Serial.println(mAngle);
  mRadius = sqrt((x * x) + (y * y));
  Serial.print("mRadius: ");
  Serial.println(mRadius);
  
}

void don(float angle){
  float polarXMove = round(angle * 0.28888 * StepPerMM);
  G1X(polarXMove);
  
  //String l="G1X"+String(polarXMove)+"F"+String(SPEED);
  //Serial.println(l);
}

void git(float y){

  float distance = round(y * StepPerMM);
  G1Y(distance);
 
  //String l="G1Y"+String(y)+"F"+String(SPEED);
  //Serial.println(l);
}

void gonder(){
  
      CalcAngleRadius(atof(field[1]),atof(field[2])); //x,y
      float mydon = eskiAngle - mAngle;
      don(mydon);
      float mygit = mRadius - eskiRadius;
      git(mygit);
      eskiAngle = mAngle;
      eskiRadius = mRadius;
}


void setup() {

  Timer1.initialize(150000/iDriveSpeed);
  // attach the motor intrupt service routine here
  Timer1.attachInterrupt( updateMotors ); 
  setMotorSpeed(iDriveSpeed);
  StepRelease();
 
  Serial.begin(115200);

  //next button as input
  pinMode(NEXT_BUTTON_PIN, INPUT);

  pinMode(yHomePin, INPUT);
  attachInterrupt(digitalPinToInterrupt(yHomePin), HomeSwitchReached, RISING);

              // homing
              sprintf(bufferX, "MakerStorage");
              sprintf(bufferY, "Homing......");
              sprintf(bufferZ, "PnPAssist");
              
              HomeY(-3000, SEARCH_HOME_SWITCH_SPEED);
              delay(100);

              // move back alittle
              git (10);
              delay(100);

              // seach home slowly
              HomeY(-110, SLOW_HOMING_SPEED);
              delay(100);

              
              // move back alittle for the rotary notch
              git (1.5);
              delay(100);
              
              // homing Rotary Axis 
              HomePolar(365, SEARCH_HOME_SWITCH_SPEED);
              delay(100);

              // turn back alittle
              don (-20);
              delay(100);

              // seach home slowly
              HomePolar(100, SLOW_HOMING_SPEED);
              delay(100);

            
              // Goto Center
              setMotorSpeed(iDriveSpeed);
              G1Y(CENTER_Y_OFFSET); // goto center

               // Set Angle to 0
              don (45);
              delay(100);

              // we are all at home :)

              

              
              
 
  sprintf(bufferX, "MakerStorage");
  sprintf(bufferY, "----------");
  sprintf(bufferZ, "PnPAssist");

  //logo display delay
  oledPrint();
  delay(1000);

  //Sd init
  if (SDfound == 0) {
    if (!SD.begin(CSPIN)) {
      Serial.print("The SD card cannot be found");
      while(1);
    }
  }
  SDfound = 1;
  pnpFile = SD.open("pnp.txt",FILE_READ);

  if (!pnpFile) {
    Serial.print("The text file cannot be opened");
    while(1);
  }

  sprintf(bufferX, "MakerStorage");
  sprintf(bufferY, "Ready!");
  sprintf(bufferZ, "PnPAssist");

  oledPrint();

/*
  //read first line to get the PCB_width
  fileLine = pnpFile.readStringUntil('\n');
  parseCommand(fileLine+" ");
  // field[0] width
  // field[1] value
  
  if(String(field[0]) == "width") {
    Pcb_width = atof(field[1]);
  }
*/

}//setup

void HomeY(float distance, float Speed){
  FindZero = true;
  setMotorSpeed(Speed);
  git(distance);  
}

void HomePolar(float angle, float Speed){
  FindZero = true;
  setMotorSpeed(Speed);
  don(angle);  
}


void HomeSwitchReached(){ //Interrupt function
  if(FindZero) {
    FindZero = false;
    Status = ARRIVED;
    CurPos = 0;
    TargetPos = 0;
  }
}

void loop() {
  while (!pnpFile.available()) { // nothing to read
     pnpFile.close();
     sprintf(bufferZ, "FINISHED!!!"); // FINISHED 
     oledPrint();
     while(true){}//Program Ended!
  }
  
  waitForButton();
    
  fileLine = pnpFile.readStringUntil('\n');
  parseCommand(fileLine+" ");
  
  // display line contents
  sprintf(bufferX, field[0]); // component name ,
  sprintf(bufferY, "%s/%s",field[4],field[5]); // component value,package
  sprintf(bufferZ, "MOVING..."); // moving
  oledPrint();
  
  gonder(); // move the motors

  sprintf(bufferZ, "ARRIVED"); // ARRIVED
 
  oledPrint();

}//loop

void G1X(long pulseCount){
  //X rotation axis and must turn oposite directions
  if (pulseCount < 0) { //direction
    digitalWrite(A_DIR, HIGH);
    digitalWrite(B_DIR, LOW);
  }else {
    digitalWrite(A_DIR, LOW);
    digitalWrite(B_DIR, HIGH);   
  }

  TargetPos = abs(pulseCount); 
  Status = MOVING;

  while(Status == MOVING){
    sprintf(bufferZ, "MOVING..."); // ARRIVED
    oledPrint();
  }; // wait for move complate
  

  
  CurPos = 0;
  TargetPos = 0;
  
}

void G1Y(long pulseCount){
  //Y linear axis and must turn same directions
  if (pulseCount < 0) { //direction
    digitalWrite(A_DIR, HIGH);
    digitalWrite(B_DIR, HIGH);
  }else {
    digitalWrite(A_DIR, LOW);
    digitalWrite(B_DIR, LOW);   
  }

  TargetPos = abs(pulseCount); 
  Status = MOVING;

  while(Status == MOVING){
    sprintf(bufferZ, "MOVING..."); // ARRIVED
    oledPrint();
  }; // wait for move complate
  

  CurPos = 0;
  TargetPos = 0;
  
}




// Called 800 times per second when y speed is 120 rpm
// Called 66.7 times per second when y speed is 10 rpm
// Called 933.3 times per second when y speed is 140 rpm

void updateMotors()
{
  // For both motors
  if (TargetPos > CurPos) { // We need to move 
    Step();
  }
  
  if ((CurPos == TargetPos)&&(Status==MOVING))
  {
    // we are reached the destination
    StepRelease();
    Status = ARRIVED;
  }
  
}

///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//
// Step once, update position
//
void Step(){
  if(lastStepPulse == LOW){
    digitalWrite(ENABLE, LOW);
    digitalWrite(STEP, HIGH);
    lastStepPulse = HIGH;
  }
  else {
    digitalWrite(ENABLE, LOW);
    digitalWrite(STEP, LOW);
    lastStepPulse = LOW;
    CurPos++;
  }
}

///////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//
// Release the Stepper Motors
//
void StepRelease(){
  digitalWrite(ENABLE, HIGH);
  digitalWrite(A_DIR, LOW);
  digitalWrite(B_DIR, LOW);
  digitalWrite(STEP, LOW);
  lastStepPulse = LOW;
}

///////////////////////////////////////////////////////

// Reset the timer for stepper motor updates to
// control rpm
void setMotorSpeed(int iRPM){
  Timer1.setPeriod(150000/iRPM);
}
