# This is meant to interface with an Arduino Uno over a serial port.
# Heavily depends on the PyDuinoBridge library. 
#
# Companion file: twin_servos_py.ino

from pyduinobridge import Bridge_py

serPort = '/dev/ttyACM0'
baudRate = 115200

ardBridge = Bridge_py()
ardBridge.begin(serPort, baudRate, numIntValues_FromPy=1, numFloatValues_FromPy=0)
# ardBridge.setVerbosity(0)
ardBridge.setSleepTime(2)   # Set this to like 7sec later

userInput = []
singleCommands = ["pos", "stop", "exit"]
# userInput = (input("Command for servo: ")).split()

while True:
    userInput = (input("Command for servo: ")).split()

    if (len(userInput) != 2) and (userInput[0] not in singleCommands):
        print("ERROR: Format is [command] [#]")

    elif userInput[0] == "exit":
        break

    else:
        command = userInput[0]

        if command not in singleCommands:
            value = int(userInput[1])
        else:
            value = -1

        fullMessage = ["<" + command + "," + str(value) + ">"]
        ardResponse = ardBridge.writeAndRead_Strings(fullMessage)
        # print(ardResponse)

        # Prints positions while sweeping
        # (Note: Blocks user input.)
        # while command == "fullsweep":
        #     print(ardBridge.read())


ardBridge.close()