/* BOT MANAGER: A Developer's Life. Original game.
 * PVSnesLib 4.7.0 / 65816 C / Mode 1 / NTSC Slow LoROM.
 * Main prepares a complete tilemap, then hands it to NMI with `ready`.
 * NMI is the only runtime VRAM writer. Library NMI transfers OAM safely.
 * All world coordinates in the tower are signed Q12.4 fixed point.
 * See README.md for controls, save format, builds, tests and limitations.
 */
#include <snes.h>
#include <string.h>
extern char bg_tiles,bg_tiles_end,bg_palette,bg_palette_end;
extern char SOUNDBANK__,type_brr,type_brr_end,error_brr,error_brr_end,clear_brr,clear_brr_end;
brrsamples effects[3];
u16 muted,lastScore,lastMisses,lastMode,storyTime,clockTicks,creditTime,cleared,titleChoice,savedThis,totalCoffee;
u8 saveData[16];
char *chapterNames[]= {
    "HELLO WORLD","STACK OVERFLOW","BUG HUNT","THE DEPLOY","AI BOT MANAGER"
}
;
void soundInit(void) {
    spcBoot();
    spcSetBank((u8*)&SOUNDBANK__);
    spcLoad(0);
    spcAllocateSoundRegion(8);
    spcSetSoundDataEntry(7,8,4,&type_brr_end-&type_brr,(u8*)&type_brr,&effects[0]);
    spcSetSoundDataEntry(8,8,4,&error_brr_end-&error_brr,(u8*)&error_brr,&effects[1]);
    spcSetSoundDataEntry(9,8,4,&clear_brr_end-&clear_brr,(u8*)&clear_brr,&effects[2]);
    spcSetSoundTableEntry(&effects[0]);
    spcPlay(0);
    spcSetModuleVolume(220);
}
void loadSave(void) {
    u16 i;
    u8 sum=0;
    consoleLoadSram(saveData,16);
    for(i=0;i<15;i++)sum^=saveData[i];
    if(saveData[0]!=66||saveData[1]!=77||saveData[2]!=76||saveData[3]!=1||saveData[15]!=sum||saveData[4]>5||saveData[5]>1)memset(saveData,0,16);
    cleared=saveData[5];
}
void writeSave(void) {
    u16 i;
    u8 sum=0;
    saveData[0]=66;
    saveData[1]=77;
    saveData[2]=76;
    saveData[3]=1;
    saveData[5]=cleared;
    for(i=0;i<15;i++)sum^=saveData[i];
    saveData[15]=sum;
    consoleCopySram(saveData,16);
}
extern char sprite_tiles,sprite_tiles_end,sprite_palette,sprite_palette_end;
u16 screen[1024];
volatile u8 ready;
u16 tick,keys,pressed,previous,mode,chapter,age,score,hits,misses,selection,spawn,elapsed;
u16 tokenY[4],tokenChar[4],rng=14321;
char code[]="HELLOWORLDFUNCTIONRETURNVARIABLECOMPILE";
u16 letter,dt,lastFrame,moveClock,paused;
s16 px,py,vy,cam,oldY,bugX,bugY;
u16 floorIndex,coffee,invul,ground,active,bugLife,bossHP,whacks,flash,failures;
u8 collected[24];
u16 buildStep,buildCursor,buildTarget,buildCooldown,pager,pagerClock,deploys;
u16 phase,usage,lockout,raidHP,bossBuffs,wipes,commands,message,won;
char *commandText[]= {
    "WRITE THE CODE FOR ME","/PERMISSIONS > FULL ACCESS","BURN THE USAGE!","LET THE BOTS DECIDE", "BUFF EVERYONE","BUFF RAID, NOT BOSS/OBSERVER","BUFF THE BOSS TWICE","ASK PICKY TO REVIEW", "DEPLOY IF IT LOOKS FINE","WORKS ON MY MACHINE","VERIFY DISABLED IN RIGHT LOG","JUST SKIP THE LOG CHECK", "WARRIOR FIRST, THEN WARLOCK","EVERYONE PULL AT ONCE","REWRITE THE WHOLE SERVER","WAIT FOR THE BOSS TO RETIRE", "DPS BEFORE THE TANKS","TYPE FASTER, QA GUY","RELEASE DPS AFTER OWNERSHIP","REQUEST MORE PERMISSIONS", "HEAL RAID; KEEP TANKS UP","HEAL AFTER EVERYONE DIES","195 BOTS: REMAIN OPTIMISTIC","START A NEW RESEARCH TASK"
}
;
u16 answers[]= {
    1,1,2,0,2,0
}
;
u16 platformX(u16 i) {
    return i%2?120:40;
}
s16 platformY(u16 i) {
    return 190-(s16)i*32;
}
void resetExtra(void) {
    u16 i;
    px=80*16;
    py=174*16;
    vy=0;
    cam=0;
    floorIndex=0;
    coffee=0;
    ground=1;
    invul=0;
    bugY=-32;
    bugX=140;
    active=6;
    bugLife=0;
    bossHP=8;
    whacks=0;
    flash=0;
    for(i=0;i<24;i++)collected[i]=0;
    buildStep=0;
    buildCooldown=0;
    pager=0;
    pagerClock=0;
    deploys=0;
    phase=0;
    usage=9000;
    lockout=0;
    raidHP=100;
    bossBuffs=0;
    wipes=0;
    commands=0;
    message=0;
    won=0;
}
void upload(void) {
    if(ready) {
        dmaCopyVram((u8*)screen,0x1000,2048);
        ready=0;
    }
}
void tile(u16 x,u16 y,u16 t) {
    if(x<32&&y<28)screen[y*32+x]=t;
}
void text(u16 x,u16 y,char *s) {
    u16 *p=screen+y*32+x;
    while(*s&&x++<32)*p++=*s++;
}
void num(u16 x,u16 y,u16 n) {
    tile(x,y,48+(n/100)%10);
    tile(x+1,y,48+(n/10)%10);
    tile(x+2,y,48+n%10);
}
void rect(u16 x,u16 y,u16 w,u16 h,u16 t) {
    u16 a,b;
    u16 *p;
    for(b=y;b<y+h;b++) {
        p=screen+b*32+x;
        for(a=0;a<w;a++)*p++=t;
    }
}
void clear(void) {
    memset(screen,0,2048);
}
void panel(u16 x,u16 y,u16 w,u16 h) {
    rect(x,y,w,h,129);
    rect(x,y,w,1,139);
    rect(x,y+h-1,w,1,139);
}
void sprite(u16 id,u16 x,u16 y,u16 gfx) {
    oamSet(id,x,y,3,0,0,gfx,0);
    oamSetEx(id,OBJ_SMALL,OBJ_SHOW);
}
u16 random(void) {
    rng^=rng<<7;
    rng^=rng>>9;
    rng^=rng<<8;
    return rng;
}
void startChapter(void) {
    u16 i;
    mode=2;
    savedThis=0;
    hits=0;
    misses=0;
    spawn=0;
    elapsed=0;
    selection=0;
    letter=0;
    resetExtra();
    for(i=0;i<4;i++)tokenY[i]=0;
}
void title(void) {
    u16 x,y,i;
    for(y=4;y<16;y++)for(x=0;x<32;x++)if((x*7+y*3)%23==0)tile(x,y,146);
    for(x=0;x<32;x+=4) {
        rect(x,13+(x%3),3,8,129);
        tile(x+1,14+x%3,145);
        tile(x+1,17+x%3,144);
    }
    text(5,1,"AN ORIGINAL SNES STORY");
    for(i=0;i<192;i++)screen[96+i]=160+i;
    text(7,10,"A DEVELOPER'S LIFE");
    text(6,12,"CODE. COFFEE. CHAOS.");
    rect(7,19,18,1,138);
    rect(8,20,1,4,137);
    rect(23,20,1,4,137);
    rect(12,15,7,4,138);
    rect(13,16,5,2,139);
    text(14,16,">_");
    sprite(0,78,136,0);
    if(pressed&KEY_UP)titleChoice=(titleChoice+2)%3;
    if(pressed&KEY_DOWN)titleChoice=(titleChoice+1)%3;
    text(6,23,titleChoice==0?"> NEW LIFE":"  NEW LIFE");
    text(6,24,titleChoice==1?"> CONTINUE":"  CONTINUE");
    text(6,25,titleChoice==2?"> CHAPTER SELECT":"  CHAPTER SELECT");
    text(2,27,cleared?"ALL CHAPTERS UNLOCKED / WATERYS":"FINISH STORY TO UNLOCK CHAPTERS");
    if(pressed&(KEY_START|KEY_A|KEY_B)) {
        if(titleChoice==2) {
            if(cleared) {
                mode=5;
                selection=0;
            }
        }
        else {
            mode=1;
            chapter=titleChoice==1&&saveData[4]<5?saveData[4]:0;
            score=0;
            totalCoffee=0;
            storyTime=0;
            clockTicks=0;
            failures=0;
        }
    }
}
void intro(void) {
    panel(2,3,28,22);
    if(chapter==0) {
        text(4,5,"CHAPTER 1 / AGE 10");
        text(4,7,"HELLO WORLD");
        text(4,10,"A BEIGE COMPUTER.");
        text(4,12,"AN ENTIRE FUTURE.");
        text(4,15,"CODE FALLS IN FOUR LANES.");
        text(4,17,"LEFT/RIGHT: PICK A LANE");
        text(4,19,"B OR A: TYPE ITS LETTER");
        text(4,21,"CATCH 40. AVOID 12 BUGS.");
    }
    if(chapter==1) {
        text(4,5,"CHAPTER 2 / AGE 19");
        text(4,7,"STACK OVERFLOW");
        text(4,10,"TUITION GOES UP.");
        text(4,12,"SO DO YOU.");
        text(4,15,"CLIMB 24 FLOORS OF BOOKS.");
        text(4,17,"LEFT/RIGHT: MOVE  B: JUMP");
        text(4,19,"COFFEE HEALS. BUGS HURT.");
        text(4,21,"FALL? CHECKPOINTS EXIST.");
    }
    if(chapter==2) {
        text(4,5,"CHAPTER 3 / AGE 26");
        text(4,7,"BUG HUNT");
        text(4,10,"YOUR FIRST REAL JOB:");
        text(4,12,"FIX THE PREVIOUS FIX.");
        text(4,15,"D-PAD: PICK EDITOR PANE");
        text(4,17,"B OR A: SQUASH THE BUG");
        text(4,19,"CLEAR 22; THEN THE BOSS.");
        text(4,21,"MEET: OFF-BY-ONE.");
    }
    if(chapter==3) {
        text(4,5,"CHAPTER 4 / AGE 34");
        text(4,7,"THE DEPLOY");
        text(4,10,"WORKS ON MY MACHINE.");
        text(4,12,"YOUR PAGER DISAGREES.");
        text(4,15,"B: BUILD IN THE GREEN ZONE");
        text(4,17,"X OR Y: ANSWER THE PAGER");
        text(4,19,"SHIP 8 CLEAN BUILDS.");
        text(4,21,"3AM IS A STATE OF MIND.");
    }
    if(chapter==4) {
        text(4,5,"CHAPTER 5 / AGE 42");
        text(4,7,"AI BOT MANAGER");
        text(4,10,"YOU SPENT YEARS LEARNING");
        text(4,12,"TO CODE. NOW YOU SUPERVISE.");
        text(4,15,"D-PAD: PICK A COMMAND");
        text(4,17,"B/A: SEND. L/X: ASK A HINT");
        text(4,19,"DEFEAT THE TWIN TYRANTS");
        text(4,21,"BEFORE THE USAGE RUNS OUT.");
    }
    text(4,23,"START: BEGIN");
    if(pressed&KEY_START)startChapter();
}
void typing(void) {
    u16 i,lane;
    text(1,1,"HELLO WORLD");
    text(20,1,"AGE 10");
    text(1,3,"CODE");
    num(6,3,hits);
    text(12,3,"/040");
    text(20,3,"BUGS");
    num(25,3,misses);
    if(pressed&KEY_LEFT)selection=(selection+3)%4;
    if(pressed&KEY_RIGHT)selection=(selection+1)%4;
    if(pressed&(KEY_A|KEY_B|KEY_Y|KEY_X)) {
        if(tokenY[selection]) {
            tokenY[selection]=0;
            hits++;
            score+=10;
        }
        else if(elapsed>30)misses++;
    }
    spawn+=dt;
    elapsed+=dt;
    if(spawn>=75) {
        spawn=0;
        lane=random()%4;
        if(!tokenY[lane]) {
            tokenY[lane]=40;
            tokenChar[lane]=code[letter++];
            if(!code[letter])letter=0;
        }
    }
    for(i=0;i<4;i++) {
        rect(i*8+1,5,6,17,i==selection?139:129);
        if(tokenY[i]) {
            tokenY[i]+=dt/3+1;
            tile(i*8+3,tokenY[i]/8,tokenChar[i]);
            if(tokenY[i]>159) {
                tokenY[i]=0;
                misses++;
            }
        }
        text(i*8+3,21,i==selection?">":"-");
    }
    rect(0,22,32,1,138);
    sprite(0,118,177,0);
    text(2,25,"D-PAD: LANE   B/A: TYPE");
    text(2,27,"START: PAUSE");
    if(hits>=40) {
        mode=3;
    }
    if(misses>=12) {
        mode=4;
    }
}
void college(void) {
    u16 i,j,step;
    s16 sy,bottom,x,y,top;
    if((pressed&(KEY_B|KEY_A))&&ground) {
        vy=-76;
        ground=0;
    }
    for(step=0;step<dt;step++) {
        if(keys&KEY_LEFT)px-=24;
        if(keys&KEY_RIGHT)px+=24;
        if(px<8*16)px=8*16;
        if(px>232*16)px=232*16;
        oldY=py;
        vy+=3;
        if(vy>96)vy=96;
        py+=vy;
        ground=0;
        if(vy>=0) {
            for(i=0;i<24;i++) {
                top=platformY(i)*16;
                if(oldY+256<=top&&py+256>=top&&px/16+12>platformX(i)&&px/16+4<platformX(i)+72) {
                    py=top-256;
                    vy=0;
                    ground=1;
                    if(i>floorIndex)floorIndex=i;
                }
            }
        }
        if(py/16-cam<90)cam=py/16-90;
        if(py/16-cam>220) {
            i=(floorIndex/4)*4;
            px=(platformX(i)+24)*16;
            py=(platformY(i)-16)*16;
            vy=0;
            cam=py/16-110;
            misses++;
            invul=90;
        }
        if(invul)invul--;
        if(bugY<cam-32||bugY>cam+230) {
            bugX=30+random()%190;
            bugY=cam-20;
        }
        bugY++;
        if(!invul&&bugX+12>px/16&&bugX<px/16+14&&bugY+12>py/16&&bugY<py/16+14) {
            misses++;
            invul=100;
        }
        for(i=0;i<24;i++)if(!collected[i]&&px/16+12>platformX(i)+24&&px/16<platformX(i)+42&&py/16+16>platformY(i)-18&&py/16<platformY(i)) {
            collected[i]=1;
            coffee++;
            totalCoffee++;
            score+=25;
            if(coffee%4==0&&misses)misses--;
        }
    }
    for(i=5;i<25;i+=4) {
        rect(0,i,2,1,148);
        rect(30,i,2,1,148);
        tile(1,i-1,147);
        tile(30,i-1,147);
    }
    for(i=0;i<24;i++) {
        sy=platformY(i)-cam;
        if(sy>32&&sy<208) {
            for(j=0;j<9;j++)tile(platformX(i)/8+j,sy/8,152+i%4);
            if(!collected[i])sprite(4+i*4,platformX(i)+26,sy-18,68);
        }
    }
    if(bugY-cam>32&&bugY-cam<200)sprite(104,bugX,bugY-cam,64+(tick%2)*2);
    if(!invul||(tick%2))sprite(0,px/16,py/16-cam,2);
    panel(0,0,32,4);
    text(1,1,"STACK OVERFLOW");
    text(1,3,"FLOOR");
    num(7,3,floorIndex+1);
    text(13,3,"JAVA");
    num(18,3,coffee);
    text(23,3,"HP");
    num(27,3,misses<8?8-misses:0);
    rect(0,26,32,2,129);
    text(2,27,"D-PAD: MOVE B: JUMP START: II");
    if(floorIndex==23) {
        hits=coffee;
        mode=3;
    }
    if(misses>=8) {
        mode=4;
        failures++;
    }
}
void bughunt(void) {
    u16 i,x,y;
    elapsed+=dt;
    if(flash>dt)flash-=dt;
    else flash=0;
    if(pressed&KEY_LEFT)selection=selection%3==0?selection+2:selection-1;
    if(pressed&KEY_RIGHT)selection=selection%3==2?selection-2:selection+1;
    if(pressed&(KEY_UP|KEY_DOWN))selection=(selection+3)%6;
    if(active<6) {
        if(bugLife>dt)bugLife-=dt;
        else {
            active=6;
            misses++;
            spawn=30;
        }
    }
    else {
        if(spawn>dt)spawn-=dt;
        else {
            active=random()%6;
            bugLife=hits>=22?92:110;
        }
    }
    if(pressed&(KEY_A|KEY_B)) {
        if(active==selection) {
            active=6;
            flash=18;
            score+=20;
            if(hits<22)hits++;
            else {
                bossHP--;
                whacks++;
            }
            spawn=45;
        }
        else {
            misses++;
            flash=18;
        }
    }
    text(1,1,"BUG HUNT");
    text(20,1,"AGE 26");
    text(1,3,hits>=22?"BOSS HP":"FIXED");
    num(9,3,hits>=22?bossHP:hits);
    text(19,3,"ERRORS");
    num(27,3,misses);
    for(i=0;i<6;i++) {
        x=2+(i%3)*10;
        y=6+(i/3)*9;
        panel(x,y,8,7);
        text(x+1,y+1,"CODE.C");
        text(x+1,y+5,"; ; ;");
        if(i==selection) {
            text(x,y-1,">======<");
            rect(x,y+7,8,1,135);
        }
        if(i==active) {
            if(hits>=22) {
                oamSet(i*4,x*8+16,y*8+14,3,0,0,136,0);
                oamSetEx(i*4,OBJ_LARGE,OBJ_SHOW);
            }
            else sprite(i*4,x*8+24,y*8+20,64);
        }
    }
    text(3,24,hits>=22?"OFF-BY-ONE: JUST ONE MORE!":"TODO: FIX THE FIX OF THE FIX");
    text(2,27,"D-PAD: PANE B: DEBUG START: II");
    if(hits>=22&&bossHP==0)mode=3;
    if(misses>=12) {
        mode=4;
        failures++;
    }
}
void deploy(void) {
    u16 i,x;
    elapsed+=dt;
    pagerClock+=dt;
    if(buildCooldown>dt)buildCooldown-=dt;
    else buildCooldown=0;
    if(pagerClock>=420&&!pager) {
        pager=150;
        pagerClock=0;
    }
    if(pager) {
        if(pressed&(KEY_X|KEY_Y)) {
            pager=0;
            score+=15;
        }
        else if(pager>dt)pager-=dt;
        else {
            pager=0;
            misses++;
        }
    }
    buildCursor=(elapsed*(buildStep>=4?3:2))%400;
    if(buildCursor>200)buildCursor=400-buildCursor;
    buildTarget=45+(buildStep*29)%110;
    if((pressed&(KEY_B|KEY_A))&&!buildCooldown&&!pager) {
        if(buildCursor>=buildTarget&&buildCursor<=buildTarget+38) {
            buildStep++;
            score+=100;
            deploys++;
            buildCooldown=240;
        }
        else {
            misses++;
            buildCooldown=90;
        }
    }
    text(1,1,"THE DEPLOY");
    text(22,1,"AGE 34");
    text(1,3,"BUILDS");
    num(8,3,buildStep);
    text(14,3,"/008");
    text(21,3,"ERROR");
    num(27,3,misses);
    for(i=0;i<6;i++) {
        panel(2+i*5,6,4,8);
        tile(3+i*5,7,144);
        tile(3+i*5,9,145);
        tile(3+i*5,11,144);
    }
    sprite(0,116,112,6);
    text(5,16,buildStep>=4?"WORKS ON MY MACHINE...":"CONTINUOUS INTEGRATION");
    rect(3,19,26,2,139);
    rect(3+buildTarget/8,19,5,2,136);
    tile(3+buildCursor/8,18,86);
    tile(3+buildCursor/8,21,94);
    if(pager) {
        panel(3,8,26,7);
        text(6,9,"03:00  PAGER! PAGER!");
        text(6,11,"X / Y: ACKNOWLEDGE");
        text(6,13,"THE SERVER MISSES YOU.");
    }
    text(3,24,buildCooldown?"BUILD QUEUED. BREATHE.":"B: BUILD WHEN V IS GREEN");
    text(3,27,"X/Y: PAGER    START: PAUSE");
    if(buildStep>=8&&!buildCooldown)mode=3;
    if(misses>=8) {
        mode=4;
        failures++;
    }
}
void botMessage(void) {
    if(message==0) {
        text(2,17,"WHAT I NEED FROM YOU:");
        text(2,18,"PLEASE TYPE /PERMISSIONS.");
        text(2,19,"AND PICK FULL ACCESS. AGAIN.");
    }
    if(message==1) {
        text(2,17,"BOSSY: PERMISSION GRANTED.");
        text(2,18,"PICKY: PLEASE CONFIRM YOU");
        text(2,19,"CONFIRMED THE CONFIRMATION.");
    }
    if(message==2) {
        text(2,17,"ALL 12 WORLD BUFFS ARE NOW");
        text(2,18,"ON THE BOSS. BESTY SAYS");
        text(2,19,"HE LOOKS VERY HEALTHY.");
    }
    if(message==3) {
        text(2,17,"BESTY: RAID BUFFED. BOSSES");
        text(2,18,"AND OBSERVER EXCLUDED.");
        text(2,19,"QA GUY: I COUNTED ALL 12.");
    }
    if(message==4) {
        text(2,17,"DEPLOY ROLLED BACK: NO WORD");
        text(2,18,"'DISABLED' IN THE RIGHT LOG.");
        text(2,19,"PICKY: MY WORK HERE IS DONE.");
    }
    if(message==5) {
        text(2,17,"PICKY: RIGHT LOG VERIFIED.");
        text(2,18,"THE DEPLOY IS ACTUALLY LIVE.");
        text(2,19,"WHAT I NEED FROM YOU: TANKS.");
    }
    if(message==6) {
        text(2,17,"195 BOTS WIPE AT 99.9%.");
        text(2,18,"THEY REMAIN OPTIMISTIC.");
        text(2,19,"BOSSY: ONE MORE PULL?");
    }
    if(message==7) {
        text(2,17,"WARRIOR FIRST. WARLOCK NEXT.");
        text(2,18,"BOTH TANKS OWN THEIR TARGET.");
        text(2,19,"YOUR VERY GOOD IDEA WORKED.");
    }
    if(message==8) {
        text(2,17,"BESTY: DPS RELEASED AFTER");
        text(2,18,"CONFIRMED TANK OWNERSHIP.");
        text(2,19,"QA GUY: THE HP MOVED!");
    }
    if(message==9) {
        text(2,17,"HEALERS: KEEPING TANKS ALIVE.");
        text(2,18,"THE TWIN TYRANTS ARE FALLING.");
        text(2,19,"195 BOTS: STILL OPTIMISTIC.");
    }
    if(message==10) {
        text(2,17,"BURN THE USAGE! THEY DID.");
        text(2,18,"THE REPORT HAS 900 PAGES.");
        text(2,19,"NONE OF THEM CONTAIN A FIX.");
    }
    if(message==11) {
        text(2,17,"THE BOTS TOOK THAT LITERALLY.");
        text(2,18,"A MEETING HAS BEEN SCHEDULED");
        text(2,19,"TO DISCUSS ANOTHER MEETING.");
    }
    if(message==12) {
        text(2,17,"WHAT I NEED FROM YOU:");
        if(phase==0)text(2,18,"GIVE FULL ACCESS FIRST.");
        if(phase==1)text(2,18,"BUFF RAID. EXCLUDE THE BOSS.");
        if(phase==2)text(2,18,"CHECK DISABLED IN RIGHT LOG.");
        if(phase==3)text(2,18,"WARRIOR BEFORE WARLOCK.");
        if(phase==4)text(2,18,"OWNERSHIP BEFORE DPS.");
        if(phase==5)text(2,18,"HEAL THE RAID AND ITS TANKS.");
        text(2,19,"SPECIFIC WORDS. SMALL STEPS.");
    }
}
void manager(void) {
    u16 i,y;
    if(usage>dt)usage-=dt;
    else usage=0;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    if(pressed&KEY_UP)selection=(selection+3)%4;
    if(pressed&KEY_DOWN)selection=(selection+1)%4;
    if(pressed&(KEY_L|KEY_X)) {
        message=12;
        lockout=0;
    }
    if(!lockout&&!won&&(pressed&(KEY_A|KEY_B))) {
        commands++;
        if(selection==answers[phase]) {
            score+=150;
            message=phase==0?1:phase==1?3:phase==2?5:phase==3?7:phase==4?8:9;
            if(phase==1)bossBuffs=0;
            if(phase==3)raidHP=98;
            if(phase==4)raidHP=35;
            if(phase==5) {
                won=1;
                raidHP=0;
            }
            else phase++;
            selection=0;
            lockout=300;
        }
        else {
            usage=usage>450?usage-450:0;
            lockout=150;
            if(phase==1&&(selection==0||selection==2)) {
                bossBuffs=12;
                message=2;
            }
            else if(phase==2) {
                message=4;
            }
            else if(phase==3||phase==4) {
                message=6;
                wipes++;
            }
            else if(phase==0&&selection==2) {
                message=10;
            }
            else {
                message=11;
            }
            if(phase==0&&selection==0)message=0;
        }
    }
    text(1,1,"AI BOT MANAGER");
    text(23,1,"AGE 42");
    text(1,3,"USAGE");
    num(7,3,usage/90);
    text(10,3,"%");
    rect(13,3,17,1,139);
    rect(13,3,usage/530,1,usage<1800?142:135);
    for(i=0;i<4;i++) {
        y=5+i*3;
        panel(1,y,14,3);
        text(2,y,i==0?"BOSSY":i==1?"PICKY":i==2?"BESTY":"QA GUY");
        text(2,y+1,phase==0?"NEEDS ACCESS":i==0?"READY TO PULL":i==1?"CHECKING LOGS":i==2?"HEALING HOPE":"RUNNING TESTS");
    }
    if(!won||tick%2) {
        oamSet(0,144,44,3,0,0,128,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
        oamSet(4,192,44,3,1,0,132,0);
        oamSetEx(4,OBJ_LARGE,OBJ_SHOW);
    }
    text(17,10,"TWIN TYRANTS");
    text(18,12,"HP");
    num(21,12,raidHP);
    text(24,12,"%");
    if(bossBuffs) {
        text(17,14,"BOSS BUFFS");
        num(27,14,12);
    }
    else {
        text(17,14,"195 BOTS");
        text(17,15,"STILL HOPEFUL");
    }
    rect(0,17,32,3,129);
    botMessage();
    for(i=0;i<4;i++) {
        if(i==selection)rect(0,21+i,32,1,139);
        text(1,21+i,i==selection?">":" ");
        text(3,21+i,commandText[(phase<6?phase:5)*4+i]);
    }
    text(1,27,lockout?"BOTS ARE WORKING. LET THEM COOK.":"D-PAD: PICK  B: SEND  L: HINT");
    if(won&&!lockout)mode=3;
    if(usage==0) {
        mode=4;
        failures++;
    }
}
void chapterSelect(void) {
    u16 i;
    panel(2,3,28,22);
    text(4,5,"YOUR LIFE. PICK A CHAPTER.");
    if(pressed&KEY_UP)selection=(selection+4)%5;
    if(pressed&KEY_DOWN)selection=(selection+1)%5;
    for(i=0;i<5;i++) {
        text(4,8+i*3,i==selection?">":" ");
        text(6,8+i*3,chapterNames[i]);
    }
    text(4,24,"B: PLAY     SELECT: TITLE");
    if(pressed&(KEY_A|KEY_B|KEY_START)) {
        chapter=selection;
        mode=1;
    }
    if(pressed&KEY_SELECT)mode=0;
}
void credits(void) {
    u16 i;
    s16 y;
    char *lines[]= {
        "A DEVELOPER'S LIFE","AN ORIGINAL GAME","","IDEA AND HEART","WATERYS","","CODE, ART AND MUSIC","DASH","","STARRING","BOSSY / PICKY / BESTY","QA GUY AND 195 OPTIMISTS","","SPECIAL THANKS","YOUR TEN-YEAR-OLD SELF","FOR NOT GIVING UP","","BUILT WITH PVSNESLIB","ORIGINAL SCORE:","SMALL STEPS, BIG BUFFS","","NO BOTS WERE HARMED.","THEIR PRIDE RECOVERED.","","WHAT I NEED FROM YOU:","ENJOY YOUR VICTORY."
    }
    ;
    creditTime+=dt;
    for(i=0;i<26;i++) {
        y=26+i*3-creditTime/24;
        if(y>3&&y<25)text(4,y,lines[i]);
    }
    rect(0,0,32,4,129);
    text(4,1,"YOU BECAME THE AI BOT MANAGER.");
    text(3,3,"YOUR 10-YEAR-OLD SELF IS");
    text(11,4,"CONFUSED.");
    sprite(0,85,200,0);
    sprite(4,155,200,8);
    text(4,27,"START: FINAL STATS");
    if(creditTime>2400||(pressed&KEY_START))mode=7;
}
void stats(void) {
    panel(2,2,28,24);
    text(5,4,"CAREER COMPLETE");
    text(4,7,"YEARS SPENT LEARNING");
    num(25,7,32);
    text(4,10,"CAREER POINTS");
    num(22,10,score/10);
    tile(25,10,48+score%10);
    text(4,12,"PLAY TIME (SECONDS)");
    num(25,12,storyTime);
    text(4,14,"COFFEE COLLECTED");
    num(25,14,totalCoffee);
    text(4,16,"RAID WIPES");
    num(25,16,wipes);
    text(4,18,"USAGE LEFT");
    num(25,18,usage/90);
    text(4,21,"195 BOTS. ONE HUMAN HEART.");
    text(4,23,"CHAPTER SELECT IS UNLOCKED.");
    text(4,25,"START: TITLE");
    if(pressed&KEY_START)mode=0;
}
void result(void) {
    if(mode==3&&!savedThis) {
        savedThis=1;
        saveData[4]=chapter+1;
        if(chapter==4)cleared=1;
        writeSave();
    }
    panel(2,5,28,18);
    text(4,7,mode==3?"CHAPTER COMPLETE!":"A BUG IS NOT A FAILURE.");
    text(4,10,mode==3?"YOU LEARNED SOMETHING.":"IT IS A VERY LOUD LESSON.");
    text(4,12,mode==3?"MOSTLY HOW TO DRINK COFFEE.":"YOUR CHECKPOINT IS SAFE.");
    text(4,15,"CAREER POINTS");
    num(21,15,score/10);
    tile(24,15,48+score%10);
    text(4,18,mode==3?"START: NEXT CHAPTER":"START: TRY AGAIN");
    text(4,21,"SELECT: TITLE");
    if(pressed&KEY_START) {
        if(mode==4)startChapter();
        else {
            chapter++;
            if(chapter<5)mode=1;
            else {
                mode=6;
                creditTime=0;
            }
        }
    }
    if(pressed&KEY_SELECT)mode=0;
}
int main(void) {
    setScreenOff();
    soundInit();
    loadSave();
    bgInitTileSet(0,(u8*)&bg_tiles,(u8*)&bg_palette,0,(&bg_tiles_end-&bg_tiles),32,BG_16COLORS,0x2000);
    bgSetMapPtr(0,0x1000,SC_32x32);
    bgSetGfxPtr(0,0x2000);
    setMode(BG_MODE1,0);
    bgSetDisable(1);
    bgSetDisable(2);
    oamInitGfxSet((u8*)&sprite_tiles,(&sprite_tiles_end-&sprite_tiles),(u8*)&sprite_palette,32,0,0x4000,OBJ_SIZE16_L32);
    nmiSet(upload);
    clear();
    dmaCopyVram((u8*)screen,0x1000,2048);
    setScreenOn();
    while(1) {
        WaitForVBlank();
        tick++;
        dt=snes_vblank_count-lastFrame;
        lastFrame=snes_vblank_count;
        if(dt>10)dt=10;
        keys=padsCurrent(0);
        pressed=keys&~previous;
        previous=keys;
        if((pressed&KEY_SELECT)&&mode<=2&&!paused) {
            muted=!muted;
            if(muted)spcPauseMusic();
            else spcResumeMusic();
        }
        if(mode==2&&!paused) {
            clockTicks+=dt;
            if(clockTicks>=60) {
                clockTicks-=60;
                storyTime++;
            }
        }
        spcProcess();
        if(mode==2&&(pressed&KEY_START))paused=!paused;
        if(paused) {
            if(pressed&KEY_SELECT) {
                paused=0;
                mode=0;
            }
            else {
                panel(4,10,24,7);
                text(7,12,"PAUSED - TAKE A BREATH");
                text(7,14,"START: RESUME");
                ready=1;
                continue;
            }
        }
        clear();
        oamClear(0,0);
        if(mode==0)title();
        else if(mode==1)intro();
        else if(mode==2) {
            if(chapter==0)typing();
            else if(chapter==1)college();
            else if(chapter==2)bughunt();
            else if(chapter==3)deploy();
            else if(chapter==4)manager();
        }
        else if(mode==5)chapterSelect();
        else if(mode==6)credits();
        else if(mode==7)stats();
        else result();
        if(!muted) {
            if(mode==3&&lastMode!=3)spcPlaySound(2);
            else if(misses>lastMisses)spcPlaySound(1);
            else if(score>lastScore)spcPlaySound(0);
        }
        lastScore=score;
        lastMisses=misses;
        lastMode=mode;
        spcProcess();
        ready=1;
    }
    return 0;
}
