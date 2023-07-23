/*
 *    Primarily uses the Elegoo Uno R3. Currently trying out the OSOYOO Pro Micro.
 *    The two servo motors are the HiTEC HS-785HB, which has a max PWM signal range of
 *    600-2400 microseconds (usec).
 *
 *    (Note: The Pro Micro requires a detachment of the PWM pins after every servo
 *           movement, or else it will emit a ton of audible noise. However, doing so 
 *           disables the servo's encoder, which means position may be lost with a 
 *           simple nudge. Best to just stick with the Uno board since it's more
 *           powerful and can better handle the power draw of 2 servos.)
 *
 *    It has been observed that 600-1500usec is approximately the range equivalent to 
 *    0-360 degrees for the driven gear, given a two-gear ratio of 3.8.
 *
 *    Given that this is a non-standard motor, use of read() is not advised, so position
 *    will be manually tracked. Note that 5 degrees (600usec) is the lowest allowed, as
 *    seen by read(), however this can be treated as 0 in the real world.
 */


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

// Serial print statements stored in flash memory instead of SRAM
const char status_setting_up[] PROGMEM = "\n\nSTATUS: setting up, please wait...\n";
const char status_origin_reached[] PROGMEM = "STATUS: origin initialized at 180 degrees\n";
const char status_setup_done[] PROGMEM = "STATUS: setup complete. Please enter a command.\n";
const char status_normalizing[] PROGMEM = "STATUS: normalizing PWM signal...\n";
const char status_normalize_cancelled[] PROGMEM = "STATUS: PWM normalization cancelled.\n";
const char prompt_normalize_pwm1[] PROGMEM = "\nPROMPT: Do you want to proceed with a normalized PWM? Servo may spin in the opposite direction.\n";
const char prompt_normalize_pwm2[] PROGMEM = "PROMPT: Press 'y', or any other key to cancel.\n\n";
const char warning_serial[] PROGMEM = "<Warning: Index has exceeded string length>\n";
const char warning_pwm_bounds[] PROGMEM = "<Warning: Destination exceeds acceptable PWM bounds of 600-1500 usec>\n";
const char error_invalid[] PROGMEM = "ERROR: please input a valid command.\n";
const char error_move_bounds[] PROGMEM = "ERROR: please choose a degrees value between -359 and 359.\n";
const char error_goto_bounds[] PROGMEM = "ERROR: please choose a degrees value between 0 and 360.\n";
const char error_unexpected_value[] PROGMEM = "ERROR: unexpected PWM value. Please review the bounds.\n";

// For reading Serial string data
const byte numChars = 32;
char charData[numChars] = {};
bool commandReady = false;

Servo servoX;
Servo servoY;
int currentServo;

// Manually track angular position in *real world* (zero at 180 degrees)
int curr_pos_x = 180;
int curr_pos_y = 180;

// Arbitrarily chosen PWM signal to represent 180 degrees in *real world*.
// Every 90-degree physical turn is consistently a 255usec difference.
int curr_pwm_x = PWM_180;
int curr_pwm_y = PWM_180;

// Affects and tracks control of motor
int runningSpeed = 2;
int tempSpeed = 0;                        // previous or new speed val to be used after a halt
bool runMode = false;
bool isHalted = false;
bool moveFlag = false;                    // dumb workaround for a blocking prompt
 

/* ==================================================================================================== */

void setup() {
  Serial.begin(9600);
  delay(2000);
  printFromFlash(status_setting_up);

  // Do not attach until a position or signal is given!
  servoX.writeMicroseconds(curr_pwm_x);
  servoY.writeMicroseconds(curr_pwm_y);
  servoX.attach(PWM_PIN_X, PWM_MIN, PWM_MAX);
  servoY.attach(PWM_PIN_Y, PWM_MIN, PWM_MAX);
  delay(5000);
  printFromFlash(status_origin_reached);

  // Hopefully temp solution
  currentServo = SERVO_X;
  Serial.print("[DEBUG] Set servo: ");
  if (currentServo == SERVO_X) Serial.println("X");
  else Serial.println("Y");

  printFromFlash(status_setup_done);
}

void loop() {
  if ((Serial.available() > 0) && moveFlag == false) {
    serialCommandEvent();
    Serial.println("Yes");
  }
  Serial.println("No Command");
  delay(1000);
}

/* ==================================================================================================== */


/* -------------------------------------------------------------------------------------
 * Parse Serial commands and run them
 *    Adapted from: 
 * https://forum.arduino.cc/t/serial-input-basics/278284/2#parsing-the-received-data-3
 * ------------------------------------------------------------------------------------- */
void processCommand() {
  char *token;
  char commandMessage[numChars] = {};
  int commandArg = -1;
  bool argFound = false;
  
  if (commandReady == true) {
    token = strtok(charData, " ");                            // isolate command
    strcpy(commandMessage, token);                  
    token = strtok(NULL, "\n");                               // isolate argument

    if (token != NULL) {
      argFound = true;
      if (token[0] == '-') commandArg = 0 - atoi(token+1);    // account for negative val
      else commandArg = atoi(token);
    }
    //else Serial.println("ERROR: Invalid input.\n  Try: [command] [arg]\n");

    // Echo serial input to terminal
    Serial.print(commandMessage);
    Serial.print(" ");
    if (argFound) {
      Serial.println(commandArg);
    }
    else Serial.println();

    // Match command and execute it
    // TODO: Create an alphabetically sorted array of string-function pairs and use binary search instead
    if (strcmp(commandMessage, "pos") == 0) showCurrentPositions();
    else if (strcmp(commandMessage, "origin") == 0) updateOrigin();
    // else if (strcmp(commandMessage, "speed") == 0) updateSpeed(commandArg);
    else if (strcmp(commandMessage, "pwm") == 0) sendPwmSignal(commandArg);
    else if (strcmp(commandMessage, "move") == 0) displace(false, commandArg);
    else if (strcmp(commandMessage, "goto") == 0) displace(true, commandArg);
    else if (strcmp(commandMessage, "sweep") == 0) sweep(commandArg);
    else if (strcmp(commandMessage, "fullsweep") == 0) sweep_all();
    else if (strcmp(commandMessage, "halt") == 0) stopMotor();
    else if (strcmp(commandMessage, "test") == 0) testSpeed(commandArg);
    else if (strcmp(commandMessage, "servo") == 0) switchServo(commandArg);
    else printFromFlash(error_invalid);
    
    commandReady = false;
  }
}


/* ------------------------------------------------------------------------
 * Reads Serial inputs as they appear
 * Taken from: https://forum.arduino.cc/t/serial-input-basics/278284
 *
 * NOTE: Requires 'New Line' ending to be selected in the Serial Monitor.
 * ------------------------------------------------------------------------ */
void serialCommandEvent() {
  static byte i = 0;
  char newChar;

  while (Serial.available() > 0 && commandReady == false) {
    newChar = Serial.read();

    if (newChar != '\n') {
      charData[i] = newChar;
      i++;

      if (i >= numChars) {
        printFromFlash(warning_serial);
        i = numChars - 1;
      }
    }
    else {
      charData[i] = '\0';   // terminate string
      i = 0;
      commandReady = true;
    }
  }
  processCommand();
}


/* -------------------------------------------------------------------
 * Reads Serial print statements stored in flash memory via PROGMEM,
 * and variable values if provided
 * ------------------------------------------------------------------- */
void printFromFlashAndMore(const char* statement1, int val, const char* statement2) {
  printFromFlash(statement1);
  Serial.print(String(val));
  printFromFlash(statement2);
  Serial.println();
}
 
void printFromFlash(const char* statement) {
  char currChar;
  
  for (byte i = 0; i < strlen_P(statement); i++) {
    currChar = pgm_read_byte_near(statement + i);
    Serial.print(currChar);
  }
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
  Serial.print("read angles (X, Y): ");
  Serial.print(String(servoX.read()));
  Serial.print(", ");
  Serial.println(String(servoY.read()));
  Serial.print("REAL angles (X, Y): ");
  Serial.print(String(curr_pos_x));
  Serial.print(", ");
  Serial.println(String(curr_pos_y));
  Serial.println("------------------------------------");
}


/*  'servo' 
 *  
 *  Specify which of the two motors is to be configured or commanded.
 *  (Hopefully a temporary solution.)
*/
void switchServo(int servo) {
  if ((servo != SERVO_X) && (servo != SERVO_Y)) {
    Serial.println("ERROR: invalid servo selection. Please enter either 0 or 1 (for X or Y).");
  }
  currentServo = servo;
  Serial.print("[DEBUG] Set servo: ");
  if (currentServo == SERVO_X) Serial.println("X");
  else Serial.println("Y");
}


/* 'origin' 
 * 
 * Sets current position as the new starting point of 180 degrees
*/
void updateOrigin() {
  Serial.println("updateOrigin() is called.");
}


/*  'speed' 
 * 
 *  Sets speed for the servo, which doesn't directly affect the servo operation,
 *  but is used to calculate the time duration to reach a certain destination.
*/
// void updateSpeed(int rev_per_sec) {
//   runningSpeed = rev_per_sec;
//   printFromFlashAndMore(speed_set1, runningSpeed, revs_per_sec);
// }


/* 'pwm'
 *
 *  Turns servo towards angle position mapped to the given PWM period value
 */
void sendPwmSignal(int target_pwm) {
  if (target_pwm < PWM_MIN || PWM_FULL_REV < target_pwm) {
    char answer;
    moveFlag = true;

    printFromFlash(warning_pwm_bounds);
    printFromFlash(prompt_normalize_pwm1);
    printFromFlash(prompt_normalize_pwm2);

    // Block until input
    while (Serial.available() == 0) {}
    answer = Serial.read();

    // Echo serial input to terminal
    Serial.println(answer);

    // Intercepts the newline added by the Serial Monitor
    // (Otherwise, the loop() function will get it first!)
    while (Serial.available() == 0) {}
    Serial.read();

    moveFlag = false;

    if ((answer == 'y') || (answer == 'Y')) {
      printFromFlash(status_normalizing);
      target_pwm = normalize_pwm(target_pwm);
    }
    else {
      printFromFlash(status_normalize_cancelled);
      return;
    }
  }

  // Convert PWM period to degrees
  int target_pos = map(target_pwm, PWM_MIN, PWM_FULL_REV, 0, 360);

  if (currentServo == 0) Serial.print("Servo X");
  else if (currentServo == 1) Serial.print("Servo Y");
  Serial.print(" turning to ");
  Serial.print(String(target_pos));
  Serial.println(" degrees");

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
      sig += 1;
    }
  }
  else { //turn backwards
    while (sig >= target_pwm) {
      if (currentServo == SERVO_X) servoX.writeMicroseconds(sig);
      else servoY.writeMicroseconds(sig);
      // Serial.println(sig);
      delay(15);
      sig -= 1;
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
  // servoX.detach();
}


void testSpeed(int target_pwm) {
  servoX.attach(PWM_PIN_X, PWM_MIN, PWM_MAX);

  // Convert PWM period to degrees
  int target_pos = map(target_pwm, PWM_MIN, PWM_FULL_REV, 0, 360);

  Serial.print("Turning to ");
  Serial.print(String(target_pos));
  Serial.println(" degrees");

  // Magnitude of travel distance
  int travel = target_pwm - curr_pwm_x;
  travel = abs(travel);                     //cannot do math inside abs()
  Serial.print("Travel: ");
  Serial.println(travel);

  unsigned long MOVING_TIME = 3000; // moving time is 3 seconds
  // unsigned long MOVING_TIME = 1000 * (travel / runningSpeed);
  Serial.print("Duration: ");
  Serial.println(String(MOVING_TIME));
  unsigned long moveStartTime;
  // int startAngle = curr_pos_x;
  // int stopAngle  = target_pos;
  int startPWM = curr_pwm_x;
  int stopPWM  = target_pwm;

  moveStartTime = millis(); // start moving

  unsigned long progress = millis() - moveStartTime;

  while ((progress <= MOVING_TIME) && (curr_pwm_x != target_pwm)) {
    // long angle = map(progress, 0, MOVING_TIME, startAngle, stopAngle);
    long signal = map(progress, 0, MOVING_TIME, startPWM, stopPWM);
    // servoX.write(angle);
    servoX.writeMicroseconds(signal);
    // Serial.println(String(angle));
    // Serial.println(String(signal));
    Serial.println(String(progress));
    progress = millis() - moveStartTime;
  }
  Serial.println("Timer done.");
  // servoX.writeMicroseconds(target_pwm);   //final push to ensure accuracy

  curr_pwm_x = target_pwm;
  curr_pos_x = target_pos;
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
      printFromFlash(error_move_bounds);
      return;
    }
    if (currentServo == SERVO_X) {
      target_pwm = map((curr_pos_x + degrees), 0, 360, PWM_MIN, PWM_FULL_REV);
    }
    else {
      target_pwm = map((curr_pos_y + degrees), 0, 360, PWM_MIN, PWM_FULL_REV);
    }
  }

  else {
    if ((degrees < 0) || (360 < degrees)) {
      printFromFlash(error_goto_bounds);
      return;
    } 
    target_pwm = map(degrees, 0, 360, PWM_MIN, PWM_FULL_REV);
  }
  sendPwmSignal(target_pwm);
}


/*
 *  If target_pwm is below 600usec, or exceeds 1500usec, then convert it to another
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

  else {
    printFromFlash(error_unexpected_value);
    // TODO: halt operation
  }
}


/*  'sweep'
 *
 *  Continuously turns the servo back and forth at a specified angle from rest.
 *
 *  Ex: degrees = 60 and start = 180: 
 *      Servo oscillates between 120 and 240 degrees.
 */
void sweep(int degrees) {

  // Initial turn
  if (!isHalted) displace(false, degrees);
  delay(2000);

  // TODO: Vary delay time depending on travel distance. 
  //       May overlap with speed control implementation.
  while (!isHalted) {
    displace(false, -(degrees*2));
    delay(2000);
    displace(false, degrees*2);
    delay(2000);
  }
}

void sweep_all() {
  int x_pos = 180;
  bool forward_flag = true;

  // currentServo = SERVO_X;
  // displace(false, )

  // Start sweeping Y, at X's 180 position
  currentServo = SERVO_Y;
  Serial.println("[DEBUG] servo is now: Y");
  displace(false, 40);
  delay(2000);
  displace(false, -80);
  delay(2000);

  while(!isHalted) {
    if ((x_pos != 240) && (forward_flag)) {
      currentServo = SERVO_X;
      Serial.println("[DEBUG] servo is now: X");
      displace(false, 2);
      delay(2000);
      x_pos += 2;
      if (x_pos >= 240) forward_flag = false;
    }
    else if ((x_pos != 120) && (!forward_flag)) {
      currentServo = SERVO_X;
      Serial.println("[DEBUG] servo is now: X");
      displace(false, -2);
      delay(2000);
      x_pos -= 2;
      if (x_pos <= 120) forward_flag = true;
    }
    currentServo = SERVO_Y;
    Serial.println("[DEBUG] servo is now: Y");
    displace(false, 80);
    delay(2000);
    displace(false, -80);
    delay(2000);
  }

}


/*  'halt' 
 *  
 *  Immediately freezes the motor in place without finishing the revolution.
*/
void stopMotor() {
  Serial.println("stopMotor() is called.");
}
