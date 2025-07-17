from flask import Flask, request
import datetime
import os

app = Flask(__name__)

AUDIO_FOLDER = "audio"
os.makedirs(AUDIO_FOLDER, exist_ok=True)

@app.route('/upload', methods=['POST'])
def upload():
    data = request.data
    filename = f"audio_{datetime.datetime.now().strftime('%H%M%S')}.raw"
    filepath = os.path.join(AUDIO_FOLDER, filename)
    with open(filepath, 'wb') as f:
        f.write(data)
    print(f"Received {len(data)} bytes → saved as {filepath}")
    return "OK"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
