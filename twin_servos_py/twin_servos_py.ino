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

// Manually track angular position in *real world* (zero at 180 degrees)
int curr_pos_x = 180;
int curr_pos_y = 180;

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

  // else Serial.println("<ERROR: Please input a valid command.>");

  // Echos back received data
  // (Note: Any print via serial is required for the Python script to continue)
  ardBridge.write_HeaderAndTwoArrays(commandInput, argInputs, 1, ardBridge.floatsRecvd, 0);

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
