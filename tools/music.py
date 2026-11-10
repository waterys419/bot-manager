"""Compose 'Small Steps, Big Buffs': original four-voice Impulse Tracker score.
All notes and waveforms are generated here; no sampled commercial music/assets.
IT sample-mode file with four 64-row patterns, looping square lead/triangle bass,
a quiet counterline and deterministic noise percussion. 125 BPM, speed 6.
"""
from pathlib import Path
import struct,math,random,wave
P=Path(__file__).resolve().parents[1]/'src/music'
P.mkdir(exist_ok=True)
def u16(b,o,n):struct.pack_into('<H',b,o,n)
def u32(b,o,n):struct.pack_into('<I',b,o,n)
notes=[
 [60,64,67,71,69,67,64,62,60,64,72,71,67,69,64,62],
 [57,60,64,67,65,64,60,59,57,60,69,67,64,65,62,60],
 [53,57,60,64,62,60,57,55,53,57,65,64,60,62,59,57],
 [55,59,62,65,64,62,59,57,55,62,67,65,64,62,59,60]]
patterns=[]
for part,melody in enumerate(notes):
 data=bytearray();root=[36,33,29,31][part]
 for row in range(64):
  if row%4==0:data+=bytes([0x81,7,melody[row//4],1,48])
  if row%8==0:data+=bytes([0x82,7,root+(7 if row%16==8 else 0),2,50])
  if row%8==4:data+=bytes([0x83,7,melody[row//4]-12,1,24])
  if row%4==0:data+=bytes([0x84,7,48 if row%8==0 else 60,3,24 if row%8==0 else 14])
  data.append(0)
 patterns.append(struct.pack('<HHI',len(data),64,0)+data)
square=bytes((110 if i<12 else -110)&255 for i in range(32))
triangle=bytes(int((1-4*abs(i/64-.5))*110)&255 for i in range(64))
r=random.Random(1642);noise=bytes(int(r.uniform(-1,1)*100*(1-i/1024)**3)&255 for i in range(1024))
samples=[square,triangle,noise]
header=bytearray(192);header[:4]=b'IMPM';header[4:4+len(b'SMALL STEPS BIG BUFFS')]=b'SMALL STEPS BIG BUFFS';header[30:32]=bytes([4,16])
for o,n in [(32,5),(34,0),(36,3),(38,4),(40,0x214),(42,0x214),(44,1),(46,0)]:u16(header,o,n)
header[48:54]=bytes([128,127,6,125,128,0]);header[64:128]=bytes([20,44,36,32]+[128]*60);header[128:192]=bytes([64]*64)
orders=bytes([0,1,2,3,255]);pos=192+len(orders)+(3+4)*4;sample_offsets=[pos+i*80 for i in range(3)];pos+=240
pattern_offsets=[]
for pat in patterns:pattern_offsets.append(pos);pos+=len(pat)
sample_headers=[]
for i,data in enumerate(samples):
 h=bytearray(80);h[:4]=b'IMPS';h[17:20]=bytes([64,17 if i<2 else 1,64]);name=[b'PENCIL SQUARE',b'MIDNIGHT TRIANGLE',b'PAPER PERCUSSION'][i];h[20:20+len(name)]=name;h[46]=1;h[47]=32
 for o,n in [(48,len(data)),(52,0),(56,len(data) if i<2 else 0),(60,8363 if i!=1 else 16726),(72,pos)]:u32(h,o,n)
 sample_headers.append(h);pos+=len(data)
module=header+orders+b''.join(struct.pack('<I',o) for o in sample_offsets+pattern_offsets)+b''.join(sample_headers)+b''.join(patterns)+b''.join(samples)
(P/'small-steps.it').write_bytes(module)
# Short, original success chirp; streamed as a sound effect alongside the score.
for name,freq in [('type',660),('error',150),('clear',880)]:
 count=1280
 with wave.open(str(P/(name+'.wav')),'wb') as w:
  w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000)
  w.writeframes(b''.join(struct.pack('<h',int(10500*(1-i/count)**2*(1 if math.sin(2*math.pi*(freq*i/16000+150*(i/16000)**2))>0 else -1))) for i in range(count)))
print('Composed original IT score and 3 synthetic WAV effects.')
