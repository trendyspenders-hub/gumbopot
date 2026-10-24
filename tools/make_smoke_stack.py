"""Make five original 120 BPM test loops; standard library only."""
import json, math, struct, wave
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'smoke_audio'
root.mkdir(exist_ok=True)
layers = []
for i, freq in enumerate((110, 165, 220, 330, 440), 1):
    path = root / f'layer{i}_120bpm.wav'
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
        out.writeframes(b''.join(struct.pack('<h', int(2500 * math.sin(2*math.pi*freq*n/44100) * math.exp(-12*((n/44100) % .5)))) for n in range(88200)))
    layers.append(dict(path=str(path), bpm=120, gain=.5))
stack = root / 'smoke_stack.json'
stack.write_text(json.dumps(dict(version=2, bpm=120, layers=layers), indent=2))
print(stack)
