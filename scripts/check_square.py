import numpy as np
import serial

# Ground truth numpy array
ground_truth = np.arange(-32768, 32768, 1, dtype=np.int32)
ground_truth = ground_truth ** 2

# Choose COM port and baud rate
ser = serial.Serial("COM8", 115200)

# Find start of data
ser.read_until(b"START")

# Read the arduino output
input_buffer = ser.read(65536 * 4)
input_array = np.frombuffer(input_buffer, dtype=np.uint32)

# Print some error things
error = ground_truth - input_array
print(np.min(error))
print(np.max(error))
print(np.average(error))
print(np.average(np.abs(error)))
print(error[-1])
