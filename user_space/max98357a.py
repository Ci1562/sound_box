import whisper
import sounddevice as sd
import scipy.io.wavfile
from mpd import MPDClient
import socket

HOST = "192.168.51.198"
PORT = 4242

def record_audio(filename="/home/cii/voice.wav", duration=5, fs=48000):
    print("recording...")
    audio = sd.rec(int(duration * fs), samplerate=fs, channels=2, dtype='int32')
    sd.wait()
    scipy.io.wavfile.write(filename, fs, audio)
    print("Record Finished!")

def transcribe_audio(filename="/home/cii/voice.wav"):
    model = whisper.load_model("base")
    result = model.transcribe(filename)
    return result['text']

record_audio()
song_name = transcribe_audio()
print(f"The song:{song_name}")
client = MPDClient()
client.connect("localhost", 6600)
client.searchadd('any', song_name)
client.play()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.sendall(song_name.encode('utf-8'))

