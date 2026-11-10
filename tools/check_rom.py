#!/usr/bin/env python3
"""Independent header/checksum validator for this stock NTSC LoROM target."""
from pathlib import Path
import struct,hashlib,json,sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'bot-manager.sfc'
b=p.read_bytes();h=0x7fc0
assert len(b)<=1024*1024 and len(b)&(len(b)-1)==0,'ROM must be power-of-two, <=1MB, no copier header'
assert all(v==0 or 32<=v<=126 for v in b[h:h+21]),'Title should be padded printable ASCII'
assert b[h+21]==0x20,'Expected Slow LoROM'
assert b[h+22]==2,'Expected ordinary ROM+SRAM+battery; no coprocessor'
assert 1<<(b[h+23]+10)==len(b),'Header ROM size differs'
assert b[h+24]==3,'Expected declared 8KB SRAM'
assert b[h+25]==1,'Expected NTSC USA region'
complement,checksum=struct.unpack_from('<HH',b,h+28)
assert checksum^complement==65535,'Checksum complement mismatch'
assert sum(b)&65535==checksum,'Checksum sum mismatch'
reset=struct.unpack_from('<H',b,0x7ffc)[0];nmi=struct.unpack_from('<H',b,0x7fea)[0]
assert reset>=0x8000 and nmi>=0x8000,'Vectors outside LoROM bank 0 ROM'
print(json.dumps(dict(file=str(p),title=b[h:h+21].decode('ascii').rstrip(' \0'),bytes=len(b),map='Slow LoROM',region='NTSC',sram_bytes=8192,checksum=f'{checksum:04X}',complement=f'{complement:04X}',reset=f'{reset:04X}',nmi=f'{nmi:04X}',sha256=hashlib.sha256(b).hexdigest(),status='PASS'),indent=2))
