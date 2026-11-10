from emulator import Emulator,ROOT
import json

def signed(n):return n-65536 if n>=32768 else n

e=Emulator(core=__import__('os').environ.get('SNES_CORE','snes9x'));e.step(120);e.shot('m1-title.png');e.tap('start',14);e.step(50);assert e.read('mode')==1;e.shot('m1-intro.png');e.tap('start',14);e.step(60);assert e.read('mode')==2
shot=False
for n in range(5000):
 if e.read('mode')!=2:break
 sel=e.read('selection');ys=[e.read('tokenY',i*2) for i in range(4)]
 if not shot and sum(bool(y) for y in ys)>=2:e.shot('m1-hello-world.png');shot=True
 if ys[sel]:e.tap('b',8)
 else:
  target=max(range(4),key=lambda i:ys[i])
  if ys[target]:e.tap('right' if (target-sel)%4<=2 else 'left',8)
  else:e.step(8)
assert e.read('mode')==3,(e.read('mode'),e.read('hits'),e.read('misses'))
e.step(30);e.shot('m1-clear.png');print('M1 pass',e.frame)
e.tap('start',14);e.step(40);assert e.read('chapter')==1;e.shot('m2-college-intro.png');e.tap('start',14);e.step(30)
last=0
for n in range(15000):
 if e.read('mode')!=2:break
 floor=e.read('floorIndex');x=signed(e.read('px'))/16;target=(120 if (floor+1)%2 else 40)+28
 buttons=[]
 if x<target-2:buttons.append('right')
 elif x>target+2:buttons.append('left')
 if e.read('ground'):buttons.append('b')
 e.step(4,buttons)
 if floor>=5 and last==0:e.shot('m2-book-tower.png');last=1
 if n%3000==0:print('tower',n,floor,x,signed(e.read('py')),e.read('misses'),flush=True)
assert e.read('mode')==3,('college',e.read('mode'),e.read('floorIndex'),e.read('misses'))
e.step(30);e.shot('m2-college-clear.png');print('College pass',e.frame)
e.tap('start',14);e.step(40);assert e.read('chapter')==2;e.shot('m2-bugs-intro.png');e.tap('start',14);e.step(30)
shot=False
for n in range(7000):
 if e.read('mode')!=2:break
 active=e.read('active');sel=e.read('selection')
 if active==6:e.step(6);continue
 if active==sel:e.tap('b',8)
 elif active//3!=sel//3:e.tap('down',8)
 else:e.tap('right' if (active%3-sel%3)%3==1 else 'left',8)
 if e.read('hits')>=22 and e.read('active')<6 and not shot:e.shot('m2-off-by-one.png');shot=True
assert e.read('mode')==3,('bugs',e.read('mode'),e.read('hits'),e.read('bossHP'),e.read('misses'))
e.step(30);e.shot('m2-bugs-clear.png');print(json.dumps(dict(milestone='M2',frames=e.frame,points=e.read('score'),errors=e.read('misses'))));e.tap('start',14);e.step(40);assert e.read('chapter')==3;e.shot('m3-deploy-intro.png');e.tap('start',14);e.step(30)
for n in range(18000):
 if e.read('mode')!=2:break
 if e.read('pager'):e.shot('m3-pager.png');e.tap('x',8)
 elif not e.read('buildCooldown') and e.read('buildTarget')+4<=e.read('buildCursor')<=e.read('buildTarget')+14:e.tap('b',6)
 else:e.step(2)
assert e.read('mode')==3,('deploy',e.read('mode'),e.read('buildStep'),e.read('misses'))
e.step(30);e.shot('m3-deploy-clear.png');print('Deploy pass',e.frame,flush=True)
e.tap('start',14);e.step(40);assert e.read('chapter')==4;e.shot('m3-manager-intro.png');e.tap('start',14);e.step(30)
wrong=set();answers=[1,1,2,0,2,0]
for n in range(16000):
 if e.read('mode')!=2:break
 phase=e.read('phase')
 if e.read('lockout') or e.read('won'):e.step(8);continue
 target=answers[phase]
 if phase in (0,1,2,3) and phase not in wrong:target=1 if phase==3 else 0
 sel=e.read('selection')
 if sel!=target:e.tap('down',8)
 else:
  e.tap('b',8)
  if target!=answers[phase]:wrong.add(phase);e.step(20);e.shot('m3-manager-gag-'+str(phase)+'.png')
  else:e.step(20);e.shot('m3-manager-stage-'+str(phase)+'.png')
assert e.read('mode')==3,('manager',e.read('mode'),e.read('phase'),e.read('usage'))
e.step(30);e.shot('m3-manager-clear.png');print(json.dumps(dict(milestone='M3',frames=e.frame,usage=e.read('usage'),commands=e.read('commands'),wipes=e.read('wipes'))));e.tap('start',14);e.step(300);assert e.read('mode')==6;e.shot('m4-credits.png');e.tap('start',14);e.step(40);assert e.read('mode')==7;e.shot('m4-stats.png')
assert e.read('cleared')==1
assert e.audio_peak>0,'SPC audio was silent'
import ctypes as C
sram=C.string_at(e.core.retro_get_memory_data(0),e.core.retro_get_memory_size(0))
assert len(sram)==8192 and sram[:4]==b'BML\x01'
e.tap('start',14);e.tap('down',14);e.tap('down',14);e.tap('b',14);e.step(40);assert e.read('mode')==5;e.shot('m4-chapter-select.png')
e.close()
e=Emulator(core=__import__('os').environ.get('SNES_CORE','snes9x'));C.memmove(e.core.retro_get_memory_data(0),sram,len(sram));e.step(150);assert e.read('cleared')==1,'SRAM unlock lost on reboot'
print(json.dumps(dict(milestone='M4',sram_bytes=len(sram),unlocked_on_reboot=True,audio_peak=e.audio_peak)))
e.close()
