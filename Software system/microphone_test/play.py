import numpy as np
import sounddevice as sd

data = np.fromfile('audio/audio_204337.raw', dtype=np.int16)
sd.play(data, samplerate=16000)
sd.wait()
