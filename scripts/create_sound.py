import numpy as np


sampling_freq = 44100
byte_per_sample = 2


# Take some arrays of same length and some samples
# Output a signal with the specified frequencies and their corresponding phases and amplitudes
# The phase is in radian, the frequency in Hertz and the sum of amplitudes shouldn't exceed 1 at any sample
def to_audio(frequencies, amplitudes, phases, duration):
    period = 1 / sampling_freq
    samples = np.arange(0, duration, period)
    output = np.zeros(samples.shape)
    for frequency, phase, amplitude in zip(frequencies, phases, amplitudes):
        signal = amplitude * np.cos(2 * np.pi * frequency * samples + phase)
        output = output + signal
    return (32767 * output).astype(np.int16)


def save_audio(audio, filename):
    import wavio
    wavio.write(filename, audio, sampling_freq, sampwidth=byte_per_sample)


def play_audio(audio):
    import sounddevice as sd
    sd.play(audio, sampling_freq)
    sd.wait()


def main_harmonics():
    play = True

    # Create list of 5 values
    values = [ord(letter) for letter in "Hello"]

    # Compute crc remainder and add it to the values list
    polynomial_normal = 0xCF
    polynomial_bitstring = "1" + bin(polynomial_normal)[2:]

    input_bitstring = ""
    for value in values:
        bin_rep = bin(value)[2:]
        input_bitstring += "0" * (8 - len(bin_rep)) + bin_rep

    # Computation of CRC taken from Wikipedia
    len_input = len(input_bitstring)
    initial_padding = (len(polynomial_bitstring) - 1) * "0"
    input_padded_array = list(input_bitstring + initial_padding)
    while '1' in input_padded_array[:len_input]:
        cur_shift = input_padded_array.index('1')
        for i in range(len(polynomial_bitstring)):
            input_padded_array[cur_shift + i] = str(int(polynomial_bitstring[i] != input_padded_array[cur_shift + i]))

    crc_remainder = int(''.join(input_padded_array)[len_input:], 2)

    # Add crc remainder to values
    values.append(crc_remainder)

    # Reference frequency (index 1, amplitude 3)
    frequencies = [1]
    amplitudes = [3]

    # Constant that are shared with the arduino code
    frequency_index_start = 3
    frequency_index_stride = 5
    number_of_values = 6

    # Add input values frequencies and amplitudes
    assert(len(values) == number_of_values)
    current_frequency = frequency_index_start
    for value in values:
        for i in range(3, -1, -1):
            amplitudes.append((value // (4 ** i)) % 4)
            frequencies.append(current_frequency)
            current_frequency += 1
        current_frequency += frequency_index_stride - 4

    # Convert to numpy arrays
    amplitudes = np.array(amplitudes, dtype=np.float64)
    frequencies = np.array(frequencies, dtype=np.float64)

    # Divide amplitude by maximum possible amplitude
    amplitudes /= 3 * len(amplitudes)

    # Multiply frequency indices by base frequency and adjut using an empirical ratio
    frequencies *= 0.99 * 500

    # Get the audio signal
    phases = np.zeros(frequencies.shape)
    duration = 1
    audio = to_audio(frequencies, amplitudes, phases, duration)

    # Add some zero padding before and after the audio
    empty_array = np.array([])
    zero_padding = to_audio(empty_array, empty_array, empty_array, 0.5)
    audio = np.hstack((zero_padding, audio, zero_padding))
    
    if play:
        play_audio(audio)
    else:
        filename = "sound.wav"
        save_audio(audio, filename)


if __name__ == "__main__":
    main_harmonics()
