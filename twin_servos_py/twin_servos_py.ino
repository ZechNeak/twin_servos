#include <pyduino_bridge.h>
#include <Servo.h>

// Define digital PWM pins and PWM signal range for both servo motors
// (Note: These use the yellow signal wires as the standard.)
#define PWM_PIN_X 6
#define PWM_PIN_Y 3
#define SERVO_X 0
#define SERVO_Y 1

#define PWM_MIN 600
#define PWM_180 1050
#define PWM_FULL_REV 1500
#define PWM_MAX 2400

#define GEAR_RATIO 3.8

Bridge_ino ardBridge(Serial);

Servo servoX;
Servo servoY;
int currentServo;
bool isHalted = false;

// Manually track angular position in *real world* (zero at 180 degrees)
int curr_pos_x = 180;
int curr_pos_y = 180;

// I know this global var is accruing technical debt but I'm in a rush
int target_pos = 180;

// Closest PWM signal that represents 180 degrees in *real world*.
// Every 90-degree real-world turn is consistently a 255usec difference.
int curr_pwm_x = PWM_180;
int curr_pwm_y = PWM_180;


/* ==================================================================================================== */


void setup() {
  delay(2000);
  Serial.begin(115200);

  // Do not attach until a position or signal is given!
  servoX.writeMicroseconds(curr_pwm_x);
  servoY.writeMicroseconds(curr_pwm_y);
  servoX.attach(PWM_PIN_X, PWM_MIN, PWM_MAX);
  servoY.attach(PWM_PIN_Y, PWM_MIN, PWM_MAX);

  // Wait for both servos to reposition
  // delay(5000);
  Serial.println("<Origin initialized at 180 degrees>");

  // Required verbatim to begin Python interface
  Serial.println("<Arduino is ready>");

}

void loop() {
  ardBridge.curMillis = millis();
  ardBridge.read();

  const char* commandInput = ardBridge.headerOfMsg;
  int argInputs[] = {ardBridge.intsRecvd[0]};

  // Match command and execute it
  // TODO: Create an alphabetically sorted array of string-function pairs and use binary search instead
  if (strcmp(commandInput, "pos") == 0) showCurrentPositions();
  else if (strcmp(commandInput, "servo") == 0) switchServo(argInputs[0]);
  else if (strcmp(commandInput, "pwm") == 0) sendPwmSignal(argInputs[0]);
  else if (strcmp(commandInput, "move") == 0) displace(false, argInputs[0]);
  else if (strcmp(commandInput, "goto") == 0) displace(true, argInputs[0]);
  else if (strcmp(commandInput, "sweep") == 0) sweep_axis(argInputs[0]);
  else if (strcmp(commandInput, "fullsweep") == 0) sweep_both(argInputs[0]);

  // Prevents Arduino from continuously executing the last command received
  strcpy(ardBridge.headerOfMsg, "xyz");
}


/* ==================================================================================================== */


/* ----------------------------------------------------------------------------------
 * Functions to be called by Serial commands, either via Serial Monitor or a script
 * ---------------------------------------------------------------------------------- */

/* 'pos'
 *
 * Show current angle positions of both servos.
*/
void showCurrentPositions() {
  Serial.print("<Angles (X, Y): ");
  Serial.print(curr_pos_x);
  Serial.print(", ");
  Serial.print(curr_pos_y);
  Serial.println(">");
}


/*  'servo'
 *
 *  Specify which of the two motors is to be configured or commanded.
 *  (Hopefully a temporary solution.)
*/
void switchServo(int servo) {
  if ((servo != SERVO_X) && (servo != SERVO_Y)) {
    Serial.println("<ERROR -- Invalid servo. (0 or 1)>");
  }
  else {
    currentServo = servo;
    Serial.print("<STATUS -- Set servo: ");
    if (currentServo == SERVO_X) Serial.println("X>");
    else Serial.println("Y>");
  }
}


/*  If target_pwm is below 600usec, or exceeds 1500usec, then convert it to another
 *  value that retains the same position in degrees. This is achieved by wrapping the
 *  value around the range 600-1500. Negative numbers, or much larger numbers, may
 *  require multiple wrapping operations.
 *
 *  Ex: Both 1650 and 750usec roughly correspond to the 60-degree angle, but 750 usec
 *      retains more physical precision. The alternative can be off by a few degrees.
 */
int normalize_pwm(int target_pwm) {
  while (target_pwm < PWM_MIN) {
    target_pwm = PWM_FULL_REV - abs(PWM_MIN - target_pwm);
  }
  while (PWM_FULL_REV < target_pwm) {
    target_pwm = PWM_MIN + abs(target_pwm - PWM_FULL_REV);
  }

  if ((PWM_MIN <= target_pwm) && (target_pwm <= PWM_FULL_REV)) return target_pwm;

  // TODO: else {} halt for unexpected value
}


/* 'pwm'
 *
 *  Turns servo towards angle position mapped to the given PWM period value
 */
void sendPwmSignal(int target_pwm) {

  // Adjust the PWM signal while retaining the same destination position
  if (target_pwm < PWM_MIN || PWM_FULL_REV < target_pwm) {
    target_pwm = normalize_pwm(target_pwm);
  }

  if (currentServo == 0) Serial.print("<Servo X");
  else if (currentServo == 1) Serial.print("<Servo Y");
  Serial.print(" turning to ");
  Serial.print(String(target_pos));
  Serial.println(" degrees>");

  // Travel towards the target angle in steps of 5
  int curr_pwm;
  int curr_pos;
  // TODO: Declare Servo pointer here, instead of using all the conditionals below.

  if (currentServo == SERVO_X) curr_pwm = curr_pwm_x;
  else curr_pwm = curr_pwm_y;

  int sig = curr_pwm;

  if (curr_pwm <= target_pwm) {
    while (sig <= target_pwm) {
      if (currentServo == SERVO_X) servoX.writeMicroseconds(sig);
      else servoY.writeMicroseconds(sig);
      // Serial.println(sig);
      delay(15);
      sig += 5;
    }
  }
  else { //turn backwards
    while (sig >= target_pwm) {
      if (currentServo == SERVO_X) servoX.writeMicroseconds(sig);
      else servoY.writeMicroseconds(sig);
      // Serial.println(sig);
      delay(15);
      sig -= 5;
    }
  }
  // Final push to ensure accuracy
  if (currentServo == SERVO_X) {
    servoX.writeMicroseconds(target_pwm);
    curr_pwm_x = target_pwm;
    curr_pos_x = target_pos;
  }
  else {
    servoY.writeMicroseconds(target_pwm);
    curr_pwm_y = target_pwm;
    curr_pos_y = target_pos;
  }
}


/*  'move [# degrees]' or 'goto [degrees from origin]'
 *
 *  If 'isAbsolute' is false, then displace the motor by the specified # of degrees from
 *  the current position.
 *
 *  Otherwise, displace the motor towards the absolute position between 0 and 360 degrees.
*/
void displace(bool isAbsolute, int degrees) {
  int target_pwm;

  if (!isAbsolute) {
    if ((degrees < -359) || (359 < degrees)) {
      Serial.println("<ERROR -- Choose between -359 and 359>");
      return;
    }
    if (currentServo == SERVO_X) {
      target_pos = (curr_pos_x + degrees);
    }
    else {
      target_pos = (curr_pos_y + degrees);
    }
    target_pwm = map(target_pos, 0, 360, PWM_MIN, PWM_FULL_REV);
  }

  else {
    if ((degrees < 0) || (360 < degrees)) {
      Serial.println("<ERROR -- Choose between 0 and 360>");
      return;
    }
    target_pos = degrees;
    target_pwm = map(degrees, 0, 360, PWM_MIN, PWM_FULL_REV);
  }
  sendPwmSignal(target_pwm);
}


/*  'sweep'
 *
 *  Continuously turns the servo back and forth at a specified angle from rest.
 *
 *  Ex: degrees = 60 and start = 180:
 *      Servo oscillates between 120 and 240 degrees.
 */
void sweep_axis(int degrees) {

  // Initial turn
  if (!isHalted) displace(false, degrees);
  delay(2000);

  // TODO: Vary delay time depending on travel distance.
  //       May overlap with future speed control implementation.
  while (!isHalted) {
    displace(false, -(degrees*2));
    delay(2000);
    displace(false, degrees*2);
    delay(2000);
  }
}


/*  Servo Y is currently hardcoded to sweep up and down 40 degrees,
 *  while Servo X moves left and right at a user-defined increment.
 */
// void sweep_step(int x_pos, int x_bound, int x_degrees) {
  
// }


/*  'fullsweep'
 *
 *  Continuously turns both servos back and forth for specified ranges.
 */
void sweep_both(int x_degrees) {
  int x_pos = 180;
  bool forward_flag = true;
  isHalted = false;

  // Hardcoded for now because I can't take more than 1 argument for my Serial commands...
  int x_sweep = 80;
  int y_sweep = 80;
  int x_bound_min = 180 - (x_sweep/2);
  int x_bound_max = 180 + (x_sweep/2);

  // Initialize both servos back to their origins
  currentServo = SERVO_X;
  displace(true, 180);
  currentServo = SERVO_Y;
  displace(true, 180);
  delay(3000);
  // Serial.println("<++++++++++++++++++++++++++++++++++++++>");

  // Start sweeping Y, at X's 180 position
  currentServo = SERVO_Y;
  displace(false, (y_sweep/2));
  delay(2000);
  displace(false, -(y_sweep));
  delay(2000);
  // Serial.println("<++++++++++++++++++++++++++++++++++++++>");

  while(!isHalted) {
    if ((x_pos != x_bound_max) && (forward_flag)) {
      currentServo = SERVO_X;
      displace(false, x_degrees);
      delay(2000);
      x_pos += x_degrees;
      if (x_pos >= x_bound_max) forward_flag = false;
    }
    else if ((x_pos != x_bound_min) && (!forward_flag)) {
      currentServo = SERVO_X;
      displace(false, -(x_degrees));
      delay(2000);
      x_pos -= x_degrees;
      if (x_pos <= x_bound_min) forward_flag = true;
    }
    currentServo = SERVO_Y;
    displace(false, y_sweep);
    delay(2000);
    displace(false, -(y_sweep));
    delay(2000);
    // Serial.println("<++++++++++++++++++++++++++++++++++++++>");

    // Check for a "stop" command to halt sweeping
    ardBridge.curMillis = millis();
    ardBridge.read();
    const char* commandInput = ardBridge.headerOfMsg;
    int argInputs[] = {ardBridge.intsRecvd[0]};

    if (strcmp(commandInput, "stop") == 0) isHalted = true;
    strcpy(ardBridge.headerOfMsg, "xyz");
  }
}