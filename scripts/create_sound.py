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

    frequencies = 0.99 * np.array([500, 1500, 2000, 2500, 3000])
    phases = np.zeros(frequencies.shape)
    duration = 1

    audios = []
    for value in (ord('A'), 256 - ord('A')):
        amplitudes = np.array([3, (value // 64) % 4, (value // 16) % 4, (value // 4) % 4, (value // 1) % 4], dtype=np.float64)
        amplitudes /= np.sum(amplitudes)
        audio = to_audio(frequencies, amplitudes, phases, duration)
        audios.append(audio)
    audio = np.hstack(audios)
    
    if play:
        play_audio(audio)
    else:
        filename = "sound.wav"
        save_audio(audio, filename)


if __name__ == "__main__":
    main_harmonics()
