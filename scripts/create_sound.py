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
    n_freq = 8
    base_freq = 1000
    phases = np.zeros(n_freq)
    amplitudes = np.ones(n_freq)
    duration = 0.00225
    play = True

    amplitudes[0] = 0
    amplitudes = amplitudes / np.sum(amplitudes)
    frequencies = base_freq * np.arange(0, n_freq, 1)
    sound = to_audio(frequencies, amplitudes, phases, duration)
    interval = to_audio([], [], [], 0.003 - duration)
    silence = to_audio([], [], [], 1)
    burst = np.array([32767, -32768], dtype=np.int16)
    audio = np.hstack([silence, *([burst, sound, interval] * 5), silence])
    
    if play:
        play_audio(audio)
    else:
        filename = "sound.wav"
        save_audio(audio, filename)


if __name__ == "__main__":
    main_harmonics()
