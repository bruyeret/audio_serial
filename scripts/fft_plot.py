import numpy as np
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Button
import matplotlib.cm as cm
from numpy.fft import fft


# This script expects this kind of output from the COM port, with:
# - If outputing raw samples: n = n_displayed_freq
# - If outputing an FFT: n = 2 * n_displayed_freq
#
# for (uint8_t i = 0; i < n; ++i) {
#     Serial.print(array[i]);
#     Serial.print(", ");
# }
# Serial.println();
#
# Example of a line of output with n = 8:
# 234, 23, 5, -23, -90, 197, 44, 72, 


# Choose COM port and baud rate
ser = serial.Serial("COM3", 115200)
# Number of frequencies to display
n_displayed_freq = 32
# Fundamental frequency (for display only)
base_freq = 500
# Choose if the array of numbers represents:
# - raw samples: sample0, sample1, sample2, ..., sample_(n_displayed_freq - 1)
# - fourier transform: re0, im0, re1, im1, ..., re(n_displayed_freq - 1), im(n_displayed_freq - 1)
use_raw_samples = False


# Create figure for plotting
fig = plt.figure()
ax_norm = fig.add_subplot(1, 2, 1)
ax_phase = fig.add_subplot(1, 2, 2)

x = [base_freq * i for i in range(n_displayed_freq)]
two_pi = 2 * np.pi
phase_ref = [3] # ugly aliasing

def change_phase_ref(event, ref=phase_ref):
    ref[0] = (ref[0] + 1) % n_displayed_freq

phase_ref_button = Button(
    fig.add_axes([0.52, 0.01, 0.4, 0.04]),
    'Change phase reference',
    hovercolor='0.975'
)
phase_ref_button.on_clicked(change_phase_ref)

# The raw array is n_freq complex numbers stored as: re0, im0, re1, im1, ...
def from_complex(raw_array):
    expected_length = 2 * n_displayed_freq
    if len(raw_array) != expected_length:
        raise
    np_array = np.reshape(np.array(raw_array), (-1, 2))
    complexes = np_array[:,0] + 1j * np_array[:,1]
    return complexes

# The raw array is n_samples real values
def from_samples(raw_array):
    expected_length = n_displayed_freq
    if len(raw_array) != expected_length:
        raise
    spectrum = fft(raw_array)
    return spectrum

def read_serial_line():
    line_max_size = 15 * n_displayed_freq
    if ser.in_waiting > 2 * line_max_size:
        print("Clear buffer")
        ser.read(ser.in_waiting - line_max_size)
        ser.read_until(b"\n")
    while True:
        try:
            buffer = ser.read_until(b"\n")
            raw_array = [int(s) for s in buffer.decode().split(", ")[:-1]]
            return from_samples(raw_array) if use_raw_samples else from_complex(raw_array)
        except:
            pass

def animate(frame):
    complexes = read_serial_line()
    complexes[0] = 0
    color = cm.rainbow(np.linspace(0, 1, n_displayed_freq))
    ref_idx = phase_ref[0]
    ref_frequency = x[ref_idx]

    norms = np.absolute(complexes)
    norm_range = (0, max(2000, *norms))
    ax_norm.clear()
    ax_norm.set_ylim(*norm_range)
    ax_norm.scatter(x, norms, color=color)
    ax_norm.plot([ref_frequency, ref_frequency], norm_range)

    phases = np.angle(complexes)
    phase_range = (0, two_pi)
    phases = (norms > 100) * np.mod(phases - phases[ref_idx] + np.pi, two_pi)
    ax_phase.clear()
    ax_phase.scatter(x, phases, color=color)
    ax_phase.set_ylim(*phase_range)
    ax_phase.plot([ref_frequency, ref_frequency], phase_range)

# Set up plot to call animate() function periodically
ani = animation.FuncAnimation(fig, animate, interval=1, cache_frame_data=False)
plt.show()
