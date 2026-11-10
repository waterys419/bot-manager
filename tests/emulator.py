"""Minimal headless libretro frontend: real ROM execution, pad input, RAM and PNGs."""
import ctypes as C
import struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Geometry(C.Structure):_fields_=[('base_width',C.c_uint),('base_height',C.c_uint),('max_width',C.c_uint),('max_height',C.c_uint),('aspect_ratio',C.c_float)]
class Timing(C.Structure):_fields_=[('fps',C.c_double),('sample_rate',C.c_double)]
class AVInfo(C.Structure):_fields_=[('geometry',Geometry),('timing',Timing)]
class GameInfo(C.Structure):_fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]
ENV=C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p)
VIDEO=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
AUDIO=C.CFUNCTYPE(None,C.c_int16,C.c_int16)
BATCH=C.CFUNCTYPE(C.c_size_t,C.POINTER(C.c_int16),C.c_size_t)
POLL=C.CFUNCTYPE(None)
INPUT=C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint)
BUTTONS={'b':0,'y':1,'select':2,'start':3,'up':4,'down':5,'left':6,'right':7,'a':8,'x':9,'l':10,'r':11}
class Emulator:
 def __init__(self,rom=None,core='snes9x'):
  self.core_name=core;self.core=C.CDLL(str(ROOT/f'toolchain/emulators/{core}_libretro.so'));self.buttons=set();self.pixel=0;self.frame=0;self.image=None;self.audio_peak=0;self.audio_frames=0;self.audio_pcm=bytearray();self.record_audio=False
  self.paths=[C.create_string_buffer(str(ROOT/'build').encode())]
  def env(cmd,data):
   if cmd in (9,31,30):C.cast(data,C.POINTER(C.c_char_p))[0]=C.cast(self.paths[0],C.c_char_p);return True
   if cmd==10:self.pixel=C.cast(data,C.POINTER(C.c_int))[0];return True
   if cmd==3:C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
   if cmd==15:return False
   if cmd==17:C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
   if cmd in (16,18,35,36,37,39,53,54,60,62,63,67,69):return False
   if cmd==2:return True
   return False
  def video(data,w,h,pitch):
   if data and data!=C.c_void_p(-1).value:self.image=(w,h,pitch,C.string_at(data,pitch*h))
  def batch(data,count):
   self.audio_frames+=count
   if count:
    raw=C.string_at(data,count*4)
    if self.record_audio:self.audio_pcm.extend(raw)
    self.audio_peak=max(self.audio_peak,max(abs(x) for x in struct.unpack('<'+'h'*(count*2),raw)))
   return count
  self.callbacks=[ENV(env),VIDEO(video),AUDIO(lambda l,r:None),BATCH(batch),POLL(lambda:None),INPUT(lambda port,dev,index,key: int(port==0 and key in self.buttons))]
  for name,callback in zip(['environment','video_refresh','audio_sample','audio_sample_batch','input_poll','input_state'],self.callbacks):getattr(self.core,'retro_set_'+name)(callback)
  self.core.retro_init()
  self.core.retro_load_game.argtypes=[C.POINTER(GameInfo)];self.core.retro_load_game.restype=C.c_bool
  self.rom_path=Path(rom or ROOT/'bot-manager.sfc');self.rom_data=C.create_string_buffer(self.rom_path.read_bytes())
  info=GameInfo(str(self.rom_path).encode(),C.cast(self.rom_data,C.c_void_p),len(self.rom_data)-1,None)
  assert self.core.retro_load_game(C.byref(info)),'ROM load failed'
  av=AVInfo();self.core.retro_get_system_av_info(C.byref(av));self.sample_rate=av.timing.sample_rate;self.fps=av.timing.fps
  self.core.retro_get_memory_data.argtypes=[C.c_uint];self.core.retro_get_memory_data.restype=C.c_void_p
  self.core.retro_get_memory_size.argtypes=[C.c_uint];self.core.retro_get_memory_size.restype=C.c_size_t
  if not self.core.retro_get_memory_data(2):self.core.retro_run()
  self.ram=(C.c_ubyte*self.core.retro_get_memory_size(2)).from_address(self.core.retro_get_memory_data(2))
  self.labels={}
  for line in (ROOT/'bot-manager.sym').read_text().splitlines():
   parts=line.split()
   if len(parts)==2:
    try:self.labels[parts[1]]=int(parts[0],16)
    except ValueError:pass
 def read(self,name,offset=0,size=2):
  address=self.labels[name]+offset
  if address>=0x7e0000:address-=0x7e0000
  return sum(self.ram[address+i]<<(8*i) for i in range(size))
 def step(self,frames=1,buttons=()):
  self.buttons={BUTTONS[b] for b in buttons}
  for _ in range(frames):self.core.retro_run();self.frame+=1
 def tap(self,key,frames=8):self.step(frames,[key]);self.step(frames)
 def shot(self,name):
  assert self.image
  w,h,pitch,data=self.image;rows=[]
  for y in range(h):
   line=bytearray([0])
   for x in range(w):
    if self.pixel==1:
     b,g,r,_=data[y*pitch+x*4:y*pitch+x*4+4]
    else:
     v=int.from_bytes(data[y*pitch+x*2:y*pitch+x*2+2],'little')
     if self.pixel==2:r=((v>>11)&31)*255//31;g=((v>>5)&63)*255//63;b=(v&31)*255//31
     else:r=((v>>10)&31)*255//31;g=((v>>5)&31)*255//31;b=(v&31)*255//31
    line+=bytes([r,g,b])
   rows.append(line)
  def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
  path=ROOT/'shots'/((self.core_name+'-' if self.core_name!='snes9x' else '')+name);path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''));return path
 def close(self):self.core.retro_unload_game();self.core.retro_deinit()
if __name__=='__main__':
 e=Emulator();e.step(120);print('frame',e.frame,'mode',e.read('mode'),'tick',e.read('tick'));print(e.shot('m1-title.png'));e.close()
