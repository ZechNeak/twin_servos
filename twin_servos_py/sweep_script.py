# This is meant to interface with an Arduino Uno over a serial port.
# Heavily depends on the PyDuinoBridge library.
#
# Instead of allowing for command input, this is simply a script 
# that begins a full sweep of both servos immediately after the
# servos and Arduino are ready.
#
# Companion file: twin_servos_py.ino

from pyduinobridge import Bridge_py

serPort = '/dev/ttyACM0'
baudRate = 115200

ardBridge = Bridge_py()
ardBridge.begin(serPort, baudRate, numIntValues_FromPy=1, numFloatValues_FromPy=0)
# ardBridge.setVerbosity(0)
ardBridge.setSleepTime(2)   # Set this to like 7sec later

x_pos = 180
x_step = 2
forward_flag = True
x_sweep = 80
y_sweep = 80
x_bound_min = 180 - (x_sweep/2)
x_bound_max = 180 + (x_sweep/2)

# Initialize both servos back to their origins
ardResponse = ardBridge.writeAndRead_Strings(["<servo,0>"])
# ardBridge.setSleepTime(1)
ardResponse = ardBridge.writeAndRead_Strings(["<goto,180>"])
# ardBridge.setSleepTime(1)
ardResponse = ardBridge.writeAndRead_Strings(["<servo,1>"])
# ardBridge.setSleepTime(1)
ardResponse = ardBridge.writeAndRead_Strings(["<goto,180>"])
# ardBridge.setSleepTime(3)
print("++++++++++++++++++++++++++++++++++++++")

# Start by sweeping Y from the origin
ardResponse = ardBridge.writeAndRead_Strings(["<servo,1>"])
ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(y_sweep/2) + ">"])
ardBridge.setSleepTime(2)
ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(-(y_sweep)) + ">"])
ardBridge.setSleepTime(2)
print("++++++++++++++++++++++++++++++++++++++")

# Repeat sweeping
while True:

    # Move Servo X by a step in either direction
    if (x_pos != x_bound_max) and (forward_flag):
        ardResponse = ardBridge.writeAndRead_Strings(["<servo,0>"])
        ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(x_step) + ">"])
        # ardBridge.setSleepTime(2)
        x_pos += x_step
        if (x_pos >= x_bound_max):
            forward_flag = False

    elif (x_pos != x_bound_min) and (not forward_flag):
        ardResponse = ardBridge.writeAndRead_Strings(["<servo,0>"])
        ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(-(x_step)) + ">"])
        # ardBridge.setSleepTime(2)
        x_pos -= x_step
        if (x_pos <= x_bound_min):
            forward_flag = True

    # Move Servo Y up and then down
    ardResponse = ardBridge.writeAndRead_Strings(["<servo,1>"])
    ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(y_sweep) + ">"])
    ardBridge.setSleepTime(2)
    ardResponse = ardBridge.writeAndRead_Strings(["<move," + str(-(y_sweep)) + ">"])
    ardBridge.setSleepTime(2)
    print("++++++++++++++++++++++++++++++++++++++")

ardBridge.close()