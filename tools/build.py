#!/usr/bin/env python3
"""Build via the pinned PVSnesLib release; Python alternative when make is absent."""
from pathlib import Path
import subprocess,os,sys,struct,json,hashlib
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
SDK=ROOT/'toolchain/pvsnes/pvsneslib';BIN=SDK/'devkitsnes/bin';TOOLS=SDK/'devkitsnes/tools'
def run(args):subprocess.run(list(map(str,args)),check=True)
run([sys.executable,'tools/art.py'])
run([sys.executable,'tools/music.py'])
run([TOOLS/'smconv','-s','-o','build/soundbank','-b','5','src/music/small-steps.it'])
for name in ['type','error','clear']:
 run([TOOLS/'snesbrr','-e',f'src/music/{name}.wav',f'build/{name}.brr'])
header=(SDK/'snes-examples/hello_world/hdr.asm').read_text().replace('LIBSNES HELLO WORLD','BOT MANAGER LIFE'.ljust(21)).replace('SRAMSIZE $00','SRAMSIZE $03').replace('CARTRIDGETYPE $00','CARTRIDGETYPE $02')
(ROOT/'build/hdr.asm').write_text(header)
run([BIN/'816-tcc','-I'+str(SDK/'pvsneslib/include'),'-I'+str(SDK/'devkitsnes/include'),'-I'+str(ROOT/'src'),'-Wall','-c','src/main.c','-o','build/main.ps'])
run([TOOLS/'816-opt','-q','-i','build/main.ps','-o','build/main.asm'])
objects=[]
for name in ['build/hdr.asm','build/main.asm','src/assets.asm','build/soundbank.asm']:
 obj='build/'+Path(name).stem+'.obj';run([BIN/'wla-65816','-I','build','-d','-s','-x','-o',obj,name]);objects.append(obj)
objects+=list(map(str,sorted((SDK/'pvsneslib/lib/LoROM_SlowROM').glob('*.obj'))))
(ROOT/'build/linkfile').write_text('[objects]\n'+'\n'.join(objects)+'\n')
run([BIN/'wlalink','-d','-s','-A','-c','-L',SDK/'pvsneslib/lib/LoROM_SlowROM','build/linkfile','bot-manager.sfc'])
sym=ROOT/'bot-manager.sym'
if sym.exists():
 s=sym.read_text();sym.write_text(s)
rom=bytearray((ROOT/'bot-manager.sfc').read_bytes());rom[0x7fdc:0x7fe0]=bytes([255,255,0,0]);checksum=sum(rom)&65535
struct.pack_into('<HH',rom,0x7fdc,checksum^65535,checksum);(ROOT/'bot-manager.sfc').write_bytes(rom)
assert len(rom)==262144 and rom[0x7fd5]==0x20 and rom[0x7fd7]==8
assert sum(rom)&65535==checksum
if (ROOT/'web').is_dir():
 (ROOT/'web/bot-manager.sfc').write_bytes(rom)
print('Built',len(rom),'byte NTSC LoROM; checksum',hex(checksum))
