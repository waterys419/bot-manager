from emulator import Emulator
import ctypes as C,json

e=Emulator();e.step(160);e.tap('start',14);e.tap('start',14);e.step(40)
assert e.read('mode')==2
e.tap('start',14);assert e.read('paused')==1
elapsed=e.read('elapsed');e.step(180);assert e.read('elapsed')==elapsed
e.shot('m4-pause.png');e.tap('start',14);assert not e.read('paused')
e.tap('select',14);assert e.read('muted')==1
e.step(120);e.audio_peak=0;e.step(120);assert e.audio_peak==0,('muted audio',e.audio_peak)
e.tap('select',14);assert e.read('muted')==0
e.step(3000);assert e.read('mode')==4,('missed-letter failure',e.read('mode'),e.read('misses'))
e.shot('m4-retry.png');e.tap('start',14);assert e.read('mode')==2 and e.read('misses')==0 and e.read('hits')==0
e.tap('start',14);e.tap('select',14);assert e.read('mode')==0
e.close()
e=Emulator();bad=bytes([255])*8192;C.memmove(e.core.retro_get_memory_data(0),bad,len(bad));e.step(150);assert e.read('cleared')==0
e.tap('down',14);e.tap('down',14);e.tap('b',14);assert e.read('mode')==0,'chapter select unlocked without clear'
e.close();print(json.dumps(dict(pause='pass',mute='pass',retry='pass',quit='pass',invalid_save='pass',chapter_lock='pass')))
