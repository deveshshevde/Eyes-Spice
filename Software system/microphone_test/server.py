from flask import Flask, request, send_file
import threading
import requests
import time
import wave
import os

app = Flask(__name__)
AUDIO_FILE = "recorded.wav"
SAMPLERATE = 16000

@app.route('/upload', methods=['POST'])
def upload_audio():
    with open("recorded.raw", "wb") as f:
        f.write(request.data)

    # Convert to WAV for playback
    with wave.open(AUDIO_FILE, 'wb') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLERATE)
        wav_file.writeframes(request.data)

    print("Audio received and saved")
    return "OK"

@app.route('/play')
def play_audio():
    os.system(f"aplay {AUDIO_FILE}") 
    return "🔊 Playing audio"

@app.route('/trigger')
def trigger():
    def trigger_thread():
        try:
            requests.get("http://192.168.1.33/trigger", timeout=2)  # ESP32 IP address
        except Exception as e:
            print("Trigger failed:", e)

    threading.Thread(target=trigger_thread).start()
    return "🎬 Trigger sent to ESP32"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
