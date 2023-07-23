#include <pyduino_bridge.h>

// Create the Bridge_ino object for communication with Python
Bridge_ino ardBridge(Serial);

void setup() {
  delay(2000);
  Serial.begin(115200);

  // Required verbatim to begin Python interface
  Serial.println("<Arduino is ready>");

}

void loop() {
  ardBridge.curMillis = millis(); 
  ardBridge.read();

  const char* commandInput = ardBridge.headerOfMsg;
  int argInputs[] = {ardBridge.intsRecvd[0]};

  // Echos back received data
  ardBridge.write_HeaderAndTwoArrays(commandInput, argInputs, 1, ardBridge.floatsRecvd, 0);

  // Prevents Arduino from continuously executing the last command received
  strcpy(ardBridge.headerOfMsg, "xyz");
}
