"""Deterministic, original short metal latch foley (no external samples)."""
import json
import math
import random
import struct
import wave
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / 'SourceArt/Audio/Detention'
OUT.mkdir(parents=True, exist_ok=True)
RATE = 48000
SOUNDS = {
    'SW_DetentionLatch': (.28, [(0, 1500, .7), (.075, 2300, .35)]),
    'SW_DetentionFailure': (.65, [(0, 720, .8), (.07, 1090, .55), (.19, 590, .5)]),
    'SW_DetentionOpen': (.72, [(0, 900, .6), (.12, 680, .45), (.38, 440, .7)]),
}
for name, (duration, hits) in SOUNDS.items():
    randomizer = random.Random(1709)
    samples = []
    for i in range(round(duration * RATE)):
        t = i / RATE
        value = 0
        for start, frequency, amplitude in hits:
            u = t-start
            if u < 0: continue
            ringing = sum(math.sin(2*math.pi*frequency*ratio*u)/weight for ratio,weight in [(1,1),(1.47,2),(2.13,3),(3.31,5)])
            value += amplitude*(ringing*math.exp(-u*18)+randomizer.uniform(-1,1)*math.exp(-u*85)*.5)
        samples.append(value)
    gain = .68/max(max(abs(v) for v in samples), .001)
    with wave.open(str(OUT/(name+'.wav')), 'wb') as stream:
        stream.setparams((1,2,RATE,0,'NONE','not compressed'))
        stream.writeframes(struct.pack('<%dh'%len(samples),*[round(v*gain*32767) for v in samples]))
(OUT/'source.json').write_text(json.dumps({'origin':'Original deterministic procedural synthesis; no external recordings', 'script':'ProjectResources/Scripts/Art/generate_detention_audio.py', 'sample_rate':RATE,'sounds':list(SOUNDS)},indent=2)+'\n',encoding='utf-8')
