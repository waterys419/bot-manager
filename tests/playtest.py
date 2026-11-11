from emulator import Emulator,ROOT
import json

SONG={0:1,1:1,2:2,3:2,4:3,5:3,6:3,7:4,8:4,9:4,10:5}
LOADS=[]
def song(e,want):
 assert e.read('song')==want,('song id',e.read('song'),want)
 LOADS.append((want,e.read('songLoadFrames')))
 assert e.read('songLoadFrames')<=60,('song load froze',e.read('songLoadFrames'))
def signed(n):return n-65536 if n>=32768 else n

e=Emulator(core=__import__('os').environ.get('SNES_CORE','snes9x'));e.step(120);song(e,0);e.shot('m1-title.png');e.tap('start',14);e.step(50);assert e.read('mode')==1;song(e,1);e.shot('m1-intro.png');e.tap('start',14);e.step(60);assert e.read('mode')==2
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
e.tap('start',14);e.step(40);assert e.read('chapter')==1;song(e,SONG[1]);e.shot('m2-college-intro.png');e.tap('start',14);e.step(30)
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
e.tap('start',14);e.step(40);assert e.read('chapter')==2;song(e,SONG[2]);e.shot('m2-bugs-intro.png');e.tap('start',14);e.step(30)
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
e.step(30);e.shot('m2-bugs-clear.png');print(json.dumps(dict(milestone='M2',frames=e.frame,points=e.read('score'),errors=e.read('misses'))));e.tap('start',14);e.step(40);assert e.read('chapter')==3;song(e,SONG[3]);e.shot('m3-deploy-intro.png');e.tap('start',14);e.step(30)
for n in range(18000):
 if e.read('mode')!=2:break
 if e.read('pager'):e.shot('m3-pager.png');e.tap('x',8)
 elif not e.read('buildCooldown') and e.read('buildTarget')+4<=e.read('buildCursor')<=e.read('buildTarget')+14:e.tap('b',6)
 else:e.step(2)
assert e.read('mode')==3,('deploy',e.read('mode'),e.read('buildStep'),e.read('misses'))
e.step(30);e.shot('m3-deploy-clear.png');print('Deploy pass',e.frame,flush=True)
e.tap('start',14);e.step(40);assert e.read('chapter')==4;song(e,SONG[4]);e.shot('m3-manager-intro.png');e.tap('start',14);e.step(30)
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
e.step(30);e.shot('m3-manager-clear.png');print(json.dumps(dict(milestone='M3',frames=e.frame,usage=e.read('usage'),commands=e.read('commands'),wipes=e.read('wipes'))));e.tap('start',14);e.step(40);assert e.read('chapter')==5 and e.read('mode')==1;song(e,SONG[5]);e.shot('m5-blind-intro.png');e.tap('start',14);e.step(30)
bans=[1,2,0,3,2,1,2,3];badshot=False;helped=False
for n in range(9000):
 if e.read('mode')!=2:break
 if e.read('lockout') or e.read('won'):e.step(8);continue
 ph=e.read('phase')
 if ph==0 and not badshot:
  assert e.read('selection')==0;e.tap('b',8);e.step(20);assert e.read('resultKind')==2 and e.read('phase')==0 and e.read('usage')<5500;e.shot('m5-blind-wrong.png');badshot=True;continue
 if ph==5 and not helped:
  e.tap('l',8);assert e.read('resultKind')==3;e.shot('m5-blind-boss-hint.png');helped=True
 sel=e.read('selection')
 if sel!=bans[ph]:e.tap('down',8)
 else:
  e.tap('b',8);e.step(20);e.shot('m5-blind-r'+str(ph)+'.png')
assert e.read('mode')==3,('blind',e.read('mode'),e.read('phase'),e.read('usage'))
e.step(30);e.shot('m5-blind-clear.png');print(json.dumps(dict(milestone='M5',frames=e.frame,usage=e.read('usage'),commands=e.read('commands'))))
e.tap('start',14);e.step(40);assert e.read('chapter')==6 and e.read('mode')==1;song(e,SONG[6]);e.shot('m6-wipe-intro.png');e.tap('start',14);e.step(30)
wans=[2,0,1,3,2,0,1,3,2];late=False;wrongdone=False
for n in range(9000):
 if e.read('mode')!=2:break
 if e.read('lockout') or e.read('won'):e.step(8);continue
 ph=e.read('phase')
 if ph==3 and not wrongdone:
  e.tap('b',8);e.step(20);assert e.read('resultKind')==2 and e.read('rows')==6600 and e.read('phase')==4;e.shot('m6-wipe-wrong.png');wrongdone=True;continue
 if ph==5 and not late:
  e.step(500);assert e.read('resultKind')==4 and e.read('phase')==6,(e.read('resultKind'),e.read('phase'));e.shot('m6-wipe-late.png');late=True;continue
 sel=e.read('selection')
 if sel!=wans[ph]:e.tap('down',8)
 else:
  e.tap('b',8);e.step(20);e.shot('m6-wipe-op'+str(ph)+'.png')
assert e.read('mode')==3,('wipe',e.read('mode'),e.read('phase'),e.read('rows'))
assert e.read('rows')==4200,e.read('rows')
e.step(30);e.shot('m6-wipe-clear.png');print(json.dumps(dict(milestone='M6',frames=e.frame,rows=e.read('rows'),commands=e.read('commands'))))
e.tap('start',14);e.step(40);assert e.read('chapter')==7 and e.read('mode')==1;song(e,SONG[7]);e.shot('m7-sunday-intro.png');e.tap('start',14);e.step(30)
fit=[0,1,2,0,1,2,0];sbad=False;shint=False;strap=False;bossans=[2,1,0]
for n in range(9000):
 if e.read('mode')!=2:break
 if e.read('lockout') or e.read('won'):e.step(8);continue
 ph=e.read('phase');sel=e.read('selection')
 if ph==0 and not sbad:
  e.tap('down',8);e.tap('b',8);e.step(20);assert e.read('resultKind')==2 and e.read('phase')==0 and e.read('usage')<6400;e.shot('m7-sunday-wrong.png');sbad=True;continue
 if ph==2 and not shint:
  e.tap('l',8);assert e.read('resultKind')==3;shint=True
 if ph<7:
  t=fit[ph]
  if sel!=t:e.tap('down',8)
  else:e.tap('b',8);e.step(20);e.shot('m7-sunday-t'+str(ph)+'.png') if ph in (0,3) else None
 elif ph==7:
  if not strap:
   e.tap('b',8);e.step(20);assert e.read('resultKind')==2 and e.read('phase')==7;e.shot('m7-sunday-trap.png');strap=True
  else:e.step(10)
 elif ph==8:
  t=bossans[e.read('bossBuffs')]
  if sel!=t:e.tap('down',8)
  else:e.tap('b',8);e.step(20);e.shot('m7-sunday-boss.png')
 else:e.step(10)
assert e.read('mode')==3,('sunday',e.read('mode'),e.read('phase'),e.read('usage'),e.read('sClk'))
assert e.read('usage')>0 and e.read('sClk')==1800
e.step(30);e.shot('m7-sunday-clear.png');print(json.dumps(dict(milestone='M7',frames=e.frame,usage=e.read('usage'),commands=e.read('commands'))))
e.tap('start',14);e.step(40);assert e.read('chapter')==8 and e.read('mode')==1;song(e,SONG[8]);e.shot('m8-auditor-intro.png');e.tap('start',14);e.step(30)
need=[4,2,4,0,4,1,4,3,4,2,0,4,1,0,4,2,3];rolled=False;ahint=False
def tp(k):e.step(4,[k]);e.step(3)
for n in range(12000):
 if e.read('mode')!=2:break
 if e.read('won'):e.step(8);continue
 ph=e.read('phase');cur=e.read('cur');gs=e.read('gsel')
 if not rolled:
  if e.read('misses')>=1:
   rolled=True;e.shot('m8-auditor-rollback.png');assert e.read('phase')<=1 and e.read('resultKind')==2
  else:e.step(4);continue
 if not ahint and ph>=0:
  tp('l');assert e.read('resultKind')==3,(e.read('resultKind'),e.read('phase'),e.read('lockout'),e.read('misses'));e.shot('m8-auditor-hint.png');ahint=True
 todo=[r for r in range(ph,min(ph+6,17)) if need[r]<4 and e.read('guardOn',r*2)!=need[r]+1]
 if not todo:e.step(3);continue
 r=todo[0]
 if gs!=need[r]:tp('right' if (need[r]-gs)%4<=2 else 'left')
 elif cur<r:tp('down')
 elif cur>r:tp('up')
 else:tp('b')
 if ph==11:e.shot('m8-auditor-warning.png') if n%50==0 else None
assert e.read('mode')==3,('auditor',e.read('mode'),e.read('phase'),e.read('misses'))
assert e.read('misses')==1
e.step(30);e.shot('m8-auditor-clear.png');print(json.dumps(dict(milestone='M8',frames=e.frame,rollbacks=e.read('misses'),commands=e.read('commands'))))
e.tap('start',14);e.step(40);assert e.read('chapter')==9 and e.read('mode')==1;song(e,SONG[9]);e.shot('m9-night-intro.png');e.tap('start',14);e.step(30)
nans=[0,1,2,3,0,2,1,3,2]
assert e.read('pager')==1 and e.read('bright')<15;e.shot('m9-night-start.png')
e.step(2200);assert e.read('awake')<300,e.read('awake');e.step(20)
e.tap('b',8);e.step(10);assert e.read('resultKind')==6 and e.read('phase')==0 and e.read('usage')<5700,(e.read('resultKind'),e.read('usage'));e.shot('m9-night-typo.png')
panic=False;wrong=False;nhint=False;pagered=0;coffee=False
for n in range(14000):
 if e.read('mode')!=2:break
 if e.read('won'):e.step(8);continue
 if e.read('pager'):
  e.step(2);k=e.read('pgKind');a0=e.read('awake');s0=e.read('incSize');e.tap('x',8);pagered+=1
  if k==0:coffee=True;assert e.read('awake')>a0 and e.read('resultKind')==4
  else:assert e.read('resultKind')==5 and e.read('incSize')==s0+1;e.shot('m9-night-badnews.png')
  continue
 if e.read('awake')<330:e.step(10);continue
 if e.read('lockout'):e.step(4);continue
 ph=e.read('phase');sel=e.read('selection')
 if ph==3 and not nhint:
  e.tap('l',8);assert e.read('resultKind')==3;e.shot('m9-night-hint.png');nhint=True
 if ph==1 and not panic and e.read('sinceShown')<45:
  if sel!=nans[ph]:e.tap('down',4)
  else:
   sz=e.read('incSize');e.tap('b',4);e.step(6);assert e.read('resultKind')==7 and e.read('incSize')==sz+1,(e.read('resultKind'),e.read('incSize'),sz);e.shot('m9-night-panic.png');panic=True
  continue
 if ph==2 and not wrong and e.read('sinceShown')>=45:
  assert sel==0;sz=e.read('incSize');e.tap('b',8);e.step(10);assert e.read('resultKind')==2 and e.read('incSize')==sz+2 and e.read('phase')==2;e.shot('m9-night-wrong.png');wrong=True;continue
 if e.read('sinceShown')<45:e.step(4);continue
 if sel!=nans[ph]:e.tap('down',8)
 else:e.tap('b',8);e.step(10);e.shot('m9-night-i'+str(ph)+'.png') if ph in (0,4,7,8) else None
assert e.read('mode')==3,('night',e.read('mode'),e.read('phase'),e.read('incSize'),e.read('usage'),e.read('awake'))
assert panic and wrong and coffee,(panic,wrong,coffee)
e.step(30);e.shot('m9-night-clear.png');assert e.read('bright')==15;print(json.dumps(dict(milestone='M9',frames=e.frame,usage=e.read('usage'),pagers=pagered,commands=e.read('commands'))))
e.tap('start',14);e.step(40);assert e.read('chapter')==10 and e.read('mode')==1;song(e,SONG[10]);e.shot('m10-tuner-intro.png');e.tap('start',14);e.step(30)
def pull():
 e.tap('b',8)
 for _ in range(60):
  if e.read('pullStep')>=6:break
  e.step(10)
 e.step(10)
assert e.read('usage')==5000 and e.read('rUnlock')==0 and e.read('selection')==0
pull();assert e.read('pullRes')==0 and e.read('bossPct')==70 and e.read('wipes')==1 and e.read('usage')==4300;e.shot('m10-tuner-wipe-owner.png')
e.tap('left',8);e.tap('down',8);e.tap('right',8);e.tap('b',8);e.step(10)
assert e.read('resultKind')==2 and e.read('phase')==1 and e.read('usage')==4300;e.shot('m10-tuner-onechange.png')
e.tap('left',8);pull();assert e.read('pullRes')==1 and e.read('bossPct')==45 and e.read('usage')==3600,(e.read('pullRes'),e.read('usage'));e.shot('m10-tuner-wipe-buffs.png')
e.tap('right',8);pull();assert e.read('pullRes')==2 and e.read('bossPct')==20;e.shot('m10-tuner-wipe-dps.png')
e.tap('down',8);e.tap('left',8);pull();assert e.read('pullRes')==3 and e.read('bossPct')==1 and e.read('rUnlock')==1,(e.read('pullRes'),e.read('bossPct'),e.read('rUnlock'),e.read('usage'),e.read('phase'));e.shot('m10-tuner-enrage.png')
e.tap('l',8);assert e.read('resultKind')==3;e.shot('m10-tuner-hint.png')
e.tap('down',8);e.tap('right',8);e.tap('right',8);pull();assert e.read('pullRes')==5 and e.read('bossPct')==1;e.shot('m10-tuner-late.png')
e.tap('left',8);pull();assert e.read('pullRes')==6 and e.read('won')==1,(e.read('pullRes'),e.read('won'));e.shot('m10-tuner-kill.png')
e.step(300);assert e.read('mode')==3,('tuner',e.read('mode'),e.read('usage'));assert e.read('usage')==800
e.shot('m10-tuner-clear.png');print(json.dumps(dict(milestone='M10',frames=e.frame,usage=e.read('usage'),pulls=e.read('phase'),wipes=e.read('wipes'))))
e.tap('start',14);e.step(300);assert e.read('mode')==6;song(e,6);e.shot('m4-credits.png');e.tap('start',14);e.step(40);assert e.read('mode')==7;e.shot('m4-stats.png')
assert e.read('cleared')==1
assert e.read('songLoads')>=6,e.read('songLoads')
print(json.dumps(dict(song_loads=e.read('songLoads'),load_frames=LOADS,max_load_frames=max(f for _,f in LOADS))))
assert e.audio_peak>0,'SPC audio was silent'
import ctypes as C
sram=C.string_at(e.core.retro_get_memory_data(0),e.core.retro_get_memory_size(0))
assert len(sram)==8192 and sram[:4]==b'BML\x01'
e.tap('start',14);e.step(80);song(e,0);e.tap('down',14);e.tap('down',14);e.tap('b',14);e.step(40);assert e.read('mode')==5;e.shot('m4-chapter-select.png')
e.close()
e=Emulator(core=__import__('os').environ.get('SNES_CORE','snes9x'));C.memmove(e.core.retro_get_memory_data(0),sram,len(sram));e.step(150);assert e.read('cleared')==1,'SRAM unlock lost on reboot'
print(json.dumps(dict(milestone='M4',sram_bytes=len(sram),unlocked_on_reboot=True,audio_peak=e.audio_peak)))
e.close()
