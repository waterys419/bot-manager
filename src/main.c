/* BOT MANAGER: A Developer's Life. Original game.
 * PVSnesLib 4.7.0 / 65816 C / Mode 1 / NTSC Slow LoROM.
 * Main prepares a complete tilemap, then hands it to NMI with `ready`.
 * NMI is the only runtime VRAM writer. Library NMI transfers OAM safely.
 * All world coordinates in the tower are signed Q12.4 fixed point.
 * See README.md for controls, save format, builds, tests and limitations.
 */
#include <snes.h>
#include <string.h>
#define NCH 11
extern char bg_tiles,bg_tiles_end,bg_palette,bg_palette_end;
extern char SB_TITLE__,SB_SCHOOL__,SB_BUGHUNT__,SB_BOTFUNK__,SB_THREERAM__,SB_BOSS__,SB_CREDITS__,type_brr,type_brr_end,error_brr,error_brr_end,clear_brr,clear_brr_end;
brrsamples effects[3];
u16 muted,lastScore,lastMisses,lastMode,storyTime,clockTicks,creditTime,cleared,titleChoice,savedThis,totalCoffee;
u8 saveData[16];
u16 song,songLoads,songLoadFrames;
u8 *songBank[7];
char *chapterNames[]= {
    "HELLO WORLD","STACK OVERFLOW","BUG HUNT","THE DEPLOY","AI BOT MANAGER","BLIND COMMANDS","THE WIPE","SUNDAY RESET","ONE FLAG STOPS THE BATCH","3AM ROLLBACK","99.9% RAID BOT TUNER"
}
;
void soundInit(void) {
    songBank[0]=(u8*)&SB_TITLE__;songBank[1]=(u8*)&SB_SCHOOL__;songBank[2]=(u8*)&SB_BUGHUNT__;songBank[3]=(u8*)&SB_BOTFUNK__;
    songBank[4]=(u8*)&SB_THREERAM__;songBank[5]=(u8*)&SB_BOSS__;songBank[6]=(u8*)&SB_CREDITS__;
    spcBoot();
    spcSetBank((u8*)&SB_TITLE__);
    spcLoad(0);
    song=0;
    spcAllocateSoundRegion(8);
    spcSetSoundDataEntry(7,8,4,&type_brr_end-&type_brr,(u8*)&type_brr,&effects[0]);
    spcSetSoundDataEntry(8,8,4,&error_brr_end-&error_brr,(u8*)&error_brr,&effects[1]);
    spcSetSoundDataEntry(9,8,4,&clear_brr_end-&clear_brr,(u8*)&clear_brr,&effects[2]);
    spcSetSoundTableEntry(&effects[0]);
    spcPlay(0);
    spcSetModuleVolume(180);
}
void loadSave(void) {
    u16 i;
    u8 sum=0;
    consoleLoadSram(saveData,16);
    for(i=0;i<15;i++)sum^=saveData[i];
    if(saveData[0]!=66||saveData[1]!=77||saveData[2]!=76||saveData[3]!=1||saveData[15]!=sum||saveData[4]>16||saveData[5]>1)memset(saveData,0,16);
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
u16 rows,freed,lastRound,resultKind,wTimer,wWin;
u16 sq[4],sClk,sAcc,sDone,guardOn[17],cur,gsel;
u16 awake,incSize,sinceShown,pgCount,pgKind,pgWait,nAcc,bright=15,want;
u16 rSet[4],rPrev[4],rUnlock,pullStep,pullClk,pullRes,bossPct;
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
/* Songs (smconv order): 0 title, 1 school, 2 bughunt, 3 botfunk, 4 threeram, 5 boss, 6 credits */
u16 wantSong(void) {
    if(mode==0||mode==5)return 0;
    if(mode==6||mode==7)return 6;
    if(chapter<2)return 1;
    if(chapter<4)return 2;
    if(chapter<7)return 3;
    if(chapter<10)return 4;
    return 5;
}
void playSong(u16 idx) {
    u16 t=snes_vblank_count;
    spcStop();
    spcSetBank(songBank[idx]);
    spcLoad(0);
    spcPlay(0);
    spcProcess();
    if(muted)spcPauseMusic();
    song=idx;
    songLoads++;
    songLoadFrames=snes_vblank_count-t;
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
    rows=9000;
    freed=0;
    lastRound=0;
    resultKind=0;
    wWin=420;
    wTimer=420;
}
void upload(void) {
    if(ready) {
        dmaCopyVram((u8*)screen,0x1000,2048);
        ready=0;
    }
}
void tile(u16 x,u16 y,u16 t);
void num5(u16 x,u16 y,u16 n) {
    u16 i;
    for(i=0;i<5;i++) {
        tile(x+4-i,y,48+n%10);
        n/=10;
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
    if(chapter==5)usage=6000;
    if(chapter==7) {
        usage=7000;
        sq[0]=3;sq[1]=3;sq[2]=3;sq[3]=0;
        sClk=0;sAcc=0;sDone=0;
    }
    if(chapter==8) {
        for(i=0;i<17;i++)guardOn[i]=0;
        cur=0;
        gsel=0;
        wWin=160;
        wTimer=160;
    }
    if(chapter==9) {
        usage=6000;
        awake=1000;
        incSize=3;
        sinceShown=0;
        pgCount=1;
        pgKind=0;
        pgWait=0;
        nAcc=0;
        pager=1;
        pagerClock=0;
        wipes=0;
        selection=0;
    }
    if(chapter==10) {
        usage=5000;
        rSet[0]=1;rSet[1]=0;rSet[2]=0;rSet[3]=0;
        for(i=0;i<4;i++)rPrev[i]=rSet[i];
        rUnlock=0;pullStep=0;pullClk=0;pullRes=0;bossPct=100;
        wipes=0;
    }
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
    text(2,27,"ALL CHAPTERS UNLOCKED / WATERYS");
    if(pressed&(KEY_START|KEY_A|KEY_B)) {
        if(titleChoice==2) {
            mode=5;
            selection=0;
        }
        else {
            mode=1;
            chapter=titleChoice==1&&saveData[4]<NCH?saveData[4]:0;
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
    if(chapter==5) {
        text(4,5,"CHAPTER 6 / AGE 45");
        text(4,7,"BLIND COMMANDS");
        text(4,10,"YOU TELL BOT A. BOT A");
        text(4,12,"TELLS BOT B. B IS LITERAL.");
        text(4,15,"D-PAD: PICK  B: SEND");
        text(4,17,"L/X: HINT");
        text(4,19,"PREDICT WHAT BOT B DOES.");
        text(4,21,"THEN OUTWORD THE MISREADER");
    }
    if(chapter==6) {
        text(4,5,"CHAPTER 7 / AGE 47");
        text(4,7,"THE WIPE");
        text(4,10,"A CHEERFUL AUTO-UPDATER");
        text(4,12,"IS CLEANING YOUR DATABASE.");
        text(4,15,"D-PAD: PICK A GUARD");
        text(4,17,"B: SEND  L/X: HINT");
        text(4,19,"BEAT THE TIMER. SAVE ROWS.");
        text(4,21,"COUNT(*) ONLY GOES DOWN.");
    }
    if(chapter==7) {
        text(4,5,"CHAPTER 8 / AGE 49");
        text(4,7,"SUNDAY RESET");
        text(4,10,"THE WEEKLY POOL REFILLS AT");
        text(4,12,"MIDNIGHT. 48 HOURS TO GO.");
        text(4,15,"D-PAD: PICK AN AGENT");
        text(4,17,"B: SEND  L/X: HINT");
        text(4,19,"MATCH TASK TO AGENT. SAVE");
        text(4,21,"USAGE. OUTWAIT THE HOURGLASS");
    }
    if(chapter==8) {
        text(4,5,"CHAPTER 9 / AGE 50");
        text(4,7,"ONE FLAG STOPS THE BATCH");
        text(4,10,"1860 STATEMENTS. ONE FLAG");
        text(4,12,"FREEZES ALL OF THEM.");
        text(4,15,"UP/DOWN: CURSOR  L/R: GUARD");
        text(4,17,"B: ATTACH GUARD. L: PREVIEW");
        text(4,19,"CLEAR FLAGS BEFORE THEY RUN.");
        text(4,21,"YOU CANNOT SWITCH GUARD OFF.");
    }
    if(chapter==9) {
        text(4,5,"CHAPTER 10 / AGE 51");
        text(4,7,"3AM ROLLBACK");
        text(4,10,"A DEPLOY WENT SIDEWAYS. YOU");
        text(4,12,"ARE TIRED. THE PAGER IS NOT.");
        text(4,15,"D-PAD: PICK  B: SEND");
        text(4,17,"X/Y: PAGER (COFFEE OR NEWS)");
        text(4,19,"L: HINT. LOW AWAKE = TYPOS.");
        text(4,21,"KEEP THE INCIDENT SMALL.");
    }
    if(chapter==10) {
        text(4,5,"CHAPTER 11 / AGE 53");
        text(4,7,"99.9% RAID BOT TUNER");
        text(4,10,"THE BOSS ALWAYS STOPS AT 1%.");
        text(4,12,"READ THE WIPE LOG. FIX ONE.");
        text(4,15,"UP/DOWN: SETTING  L/R: VALUE");
        text(4,17,"B: PULL (ONE CHANGE A PULL)");
        text(4,19,"L/X: HINT. EACH PULL COSTS.");
        text(4,21,"ENRAGE IS A TIMER. NOT A HP.");
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
char *bSaid[]= {
    "MAKE IT POP","DELETE THE DUPLICATES","WORKS ON MY MACHINE","TIDY UP THE LOGS","SHIP IT",
    "TASK: RENAME FILE A TO B","TASK: FIX TEH ON LINE 4","TASK: RESTART WEB ONLY"
}
;
char *bPara[]= {
    "ADD POP TO THE PAGE","DELETE ROWS THAT REPEAT","SHIP THE MACHINE","MAKE THE LOGS TINY","SEND IT NOW"
}
;
char *bOpt[]= {
    "PAGE GETS BRIGHTER COLORS","PLAYS POPCORN SOUNDS","DELETES THE PAGE","NOTHING HAPPENS",
    "KEEPS ONE COPY OF EACH","ASKS FOR CONFIRMATION","DELETES EVERY ROW","REMOVES 3 EXTRA ROWS",
    "SHIPS YOUR WHOLE MACHINE","RUNS THE TESTS AGAIN","WRITES A KIND NOTE","FILES A BUG REPORT",
    "SORTS THEM BY DATE","COMPRESSES THEM NICELY","ADDS EMOJI TO THEM","MAKES EVERY LOG 0 BYTES",
    "DEPLOYS AFTER TESTS","WAITS FOR FRIDAY","SENDS A PALLET BY FREIGHT","ASKS PICKY TO REVIEW",
    "CLEAN UP THE FILES","RENAME A.TXT TO B.TXT ONLY","MAKE A.TXT BETTER","YOU KNOW WHAT TO DO",
    "POLISH LINE 4","REWRITE LINE 4 BETTER","LINE 4: TEH -> THE. ONLY.","FIX LINE 4 AND OTHERS",
    "RESTART THE SERVER","RESTART EVERYTHING","TURN IT OFF AND ON","RESTART WEB. ONLY WEB."
}
;
char *bGag[]= {
    "POPCORN SFX. SCREEN FILLS","WITH KERNING. IT POPS.",
    "A BAD IMPORT MADE ALL ROWS","DUPLICATES. 38038 FREED.",
    "BOT B SHIPPED YOUR MACHINE.","YOUR DESK IS IN PRODUCTION.",
    "ALL LOGS ARE NOW 0 BYTES.","VERY TIDY. NOTHING TO READ.",
    "A PALLET OF CODE ARRIVES.","BOSSY SIGNS FOR IT. TWICE.",
    "MISREADER RENAMED ONE FILE","AND NOTHING ELSE. SO SAD.",
    "IT FIXED TEH. ONLY TEH.","MISREADER FINDS IT UNFAIR.",
    "WEB RESTARTED. ONLY WEB.","MISREADER: THAT WAS BORING."
}
;
u16 bAns[]= {
    1,2,0,3,2,1,2,3
}
;
void blind(void) {
    u16 i,r;
    if(usage>dt)usage-=dt;
    else usage=0;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    if(pressed&KEY_UP)selection=(selection+3)%4;
    if(pressed&KEY_DOWN)selection=(selection+1)%4;
    if(pressed&(KEY_L|KEY_X)) {
        resultKind=3;
        lockout=0;
    }
    if(!lockout&&!won&&(pressed&(KEY_A|KEY_B))) {
        commands++;
        lastRound=phase;
        if(selection==bAns[phase]) {
            score+=150;
            resultKind=1;
            if(phase==7)won=1;
            else phase++;
            selection=0;
            lockout=240;
        }
        else {
            usage=usage>700?usage-700:0;
            misses++;
            resultKind=2;
            lockout=150;
        }
    }
    r=phase;
    text(1,1,"BLIND COMMANDS");
    text(16,1,"RD");
    tile(19,1,49+r);
    text(25,1,"AGE 45");
    text(1,3,"USAGE");
    num(7,3,usage/60);
    text(10,3,"%");
    rect(13,3,17,1,139);
    rect(13,3,usage/353,1,usage<1200?142:135);
    panel(1,5,25,4);
    panel(1,9,25,4);
    text(2,6,r<5?"YOU TELL BOT A:":"THE MISREADER LISTENS");
    text(2,7,bSaid[r]);
    text(2,10,r<5?"BOT A TELLS BOT B:":"AND DOES EXACTLY THAT");
    text(2,11,r<5?bPara[r]:"NO MORE. NO LESS.");
    sprite(0,214,44,0);
    sprite(4,214,80,8);
    if(r>=5&&(!won||tick%2)) {
        oamSet(8,208,184,3,0,0,136,0);
        oamSetEx(8,OBJ_LARGE,OBJ_SHOW);
    }
    text(1,13,r<5?"BOT B WILL LITERALLY:":"PICK THE ORDER IT CANNOT");
    if(r>=5)text(1,14,"MISREAD:");
    for(i=0;i<4;i++) {
        if(i==selection)rect(0,15+i,32,1,139);
        text(1,15+i,i==selection?">":" ");
        text(3,15+i,bOpt[r*4+i]);
    }
    rect(0,19,32,4,129);
    if(resultKind==0) {
        text(2,20,"BOT A RELAYS. BOT B ACTS.");
        text(2,21,"PREDICT THE TELEPHONE GAME.");
    }
    if(resultKind==1) {
        text(2,20,"CORRECT! WHAT HAPPENED:");
        text(2,21,bGag[lastRound*2]);
        text(2,22,bGag[lastRound*2+1]);
    }
    if(resultKind==2) {
        text(2,20,"WRONG! WHAT HAPPENED:");
        text(2,21,bGag[lastRound*2]);
        text(2,22,bGag[lastRound*2+1]);
    }
    if(resultKind==3) {
        text(2,20,"SAY EXACTLY WHAT YOU MEAN.");
        text(2,21,"NOTHING MORE. BOT B DOES");
        text(2,22,"WHAT THE WORDS LITERALLY SAY.");
    }
    if(r>=5) {
        text(1,24,"MISREADER HP");
        tile(14,24,48+(won?0:8-r));
    }
    text(1,27,lockout?"BOTS ARE WORKING. LET THEM COOK.":"D-PAD: PICK  B: SEND  L: HINT");
    if(won&&!lockout)mode=3;
    if(usage==0) {
        mode=4;
        failures++;
    }
}
char *wAnn[]= {
    "DROPPING ORPHANED ROWS!","TRUNCATING FOR TIDINESS!","SYNCING 3 TABLES AT ONCE!","COUNTING FOR FUN!",
    "REMOVING OLD NAMES!","RESHAPING THE SCHEMA!","MOVING MONEY AROUND!","LISTING ALL THE ROWS!","LAST ONE! UPDATING MYSELF!"
}
;
char *wSql[]= {
    "DELETE FROM ORDERS;","ALTER TABLE USERS DROP NAME;","UPDATE A;UPDATE B;UPDATE C;","SELECT COUNT(*) FROM USERS;",
    "UPDATE USERS SET NAME=0;","ALTER TABLE USERS DROP EMAIL;","DEBIT A; CREDIT B;","SELECT * FROM USERS;","UPDATE UPDATER SET ENABLED=0;"
}
;
char *wGuard[]= {
    "BACKUP FIRST","WRAP IN TRANSACTION","ADD A WHERE CLAUSE","LET IT RUN",
    "BACKUP FIRST","TRUST THE FIRST LOG","VERIFY IN THE RIGHT LOG","LET IT RUN"
}
;
char *wOk[]= {
    "WHERE ADDED. ONLY ORPHANS","WERE DROPPED.",
    "BACKUP TAKEN. IT IS 47 DAYS","OLD. EVERYONE IS NAMED TEST.",
    "ALL 3 UPDATES COMMITTED","TOGETHER. OR NONE. NICE.",
    "COUNT: 38038 ROWS. IT IS","VERY PROUD OF THIS NUMBER.",
    "ONLY ONE ROW WAS RENAMED.","THE BOTS ARE RELIEVED.",
    "BACKUP SAVED THE EMAIL","COLUMN. PICKY: LOG IT.",
    "DEBIT AND CREDIT LANDED","TOGETHER. NO MONEY LOST.",
    "ROWS SCROLL BY. NOTHING WAS","HARMED. THE BOTS CHEER.",
    "LOG VERIFIED: UPDATER IS OFF.","IT DISABLED ITSELF. HAPPY."
}
;
u16 wAns[]= {
    2,0,1,3,2,0,1,3,2
}
;
void wipe(void) {
    u16 i,set,r,w;
    set=phase>=8?1:0;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    if(pressed&KEY_UP)selection=(selection+3)%4;
    if(pressed&KEY_DOWN)selection=(selection+1)%4;
    if(pressed&(KEY_L|KEY_X))resultKind=3;
    if(!lockout&&!won) {
        w=0;
        if(pressed&(KEY_A|KEY_B)) {
            commands++;
            lastRound=phase;
            if(selection==wAns[phase]) {
                score+=150;
                resultKind=1;
                lockout=phase==8?300:200;
                if(phase==8)won=1;
                else phase++;
                selection=0;
                wWin=420-phase*20;
                wTimer=wWin;
            }
            else w=2;
        }
        else if(wTimer>dt)wTimer-=dt;
        else w=4;
        if(w) {
            resultKind=w;
            misses++;
            wipes++;
            rows=rows>2400?rows-2400:0;
            freed+=2400;
            lockout=150;
            if(phase<8)phase++;
            selection=0;
            wWin=420-phase*20;
            wTimer=wWin;
        }
    }
    text(1,1,"THE WIPE");
    text(20,1,"AGE 47");
    text(1,3,"COUNT(*)");
    num5(10,3,rows);
    rect(16,3,14,1,139);
    rect(16,3,rows/650,1,rows<3000?142:135);
    text(1,5,"OPERATION");
    tile(11,5,49+phase);
    text(13,5,"OF 9");
    text(1,7,"TIMER");
    rect(7,7,12,1,139);
    rect(7,7,(u16)(wTimer*12/wWin),1,wTimer<120?142:135);
    text(1,8,"ROWS FREED:");
    num5(13,8,freed);
    if(!won||tick%2) {
        oamSet(0,200,40,3,0,0,128,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
    }
    panel(1,10,30,5);
    text(2,11,"UPDATER SAYS:");
    text(2,12,wAnn[phase]);
    text(2,13,wSql[phase]);
    text(1,15,"PICK A GUARD:");
    for(i=0;i<4;i++) {
        if(i==selection)rect(0,16+i,32,1,139);
        text(1,16+i,i==selection?">":" ");
        text(3,16+i,wGuard[set*4+i]);
    }
    rect(0,20,32,4,129);
    r=lastRound;
    if(resultKind==0) {
        text(2,21,"THE UPDATER IS CHEERFUL.");
        text(2,22,"THAT IS THE PROBLEM.");
    }
    if(resultKind==1) {
        text(2,21,wOk[r*2]);
        text(2,22,wOk[r*2+1]);
        text(2,23,r==8?"UPDATER: GREAT WORK, TEAM!":"UPDATER: YOU'RE WELCOME!");
    }
    if(resultKind==2||resultKind==4) {
        text(2,21,resultKind==2?"WRONG GUARD! ROWS VANISHED.":"TOO LATE! IT RAN ANYWAY.");
        text(2,22,"NEW RECORD:      ROWS FREED!");
        num5(14,22,freed);
        text(2,23,"UPDATER: YOU'RE WELCOME!");
    }
    if(resultKind==3) {
        if(phase==8) {
            text(2,21,"IT MUST SWITCH ITSELF OFF.");
            text(2,22,"CHECK THE RIGHT LOG FOR IT.");
        }
        else {
            text(2,21,"NO WHERE? ADD ONE. SCHEMA?");
            text(2,22,"BACKUP. MANY STEPS? WRAP.");
            text(2,23,"ONLY READING? LET IT RUN.");
        }
    }
    text(1,25,lockout?"THE UPDATER IS PRESENTING...":"D-PAD: PICK  B: SEND  L: HINT");
    if(won&&!lockout)mode=3;
    if(rows==0) {
        mode=4;
        failures++;
    }
}
void hit(u16 n) {
    usage=usage>n?usage-n:0;
}
char *sTask[]= {
    "REFACTOR THE AUTH MODULE","REVIEW THE PULL REQUEST","WRITE THE RELEASE NOTES","BUILD THE NIGHTLY",
    "FIND THE FLAKY TEST","ANSWER 40 SUPPORT EMAILS","MIGRATE THE BIG TABLE","ONE MORE PULL?"
}
;
char *sSub[]= {
    "HEAVY LIFTING. NEEDS MUSCLE.","SMALL, PICKY, PRECISE.","WORDS. LOTS OF FRIENDLY WORDS.","HEAVY LIFTING. AGAIN.",
    "SMALL, PICKY, PRECISE.","WORDS. LOTS OF FRIENDLY WORDS.","HEAVY LIFTING. THE HEAVIEST.","TOTALLY SAFE. JUST ONE."
}
;
char *sName[]= {
    "BOSSY","PICKY","BESTY","POLITE"
}
;
char *sOk[]= {
    "AUTH REFACTORED. THE GUY IN","THE BACK ALSO DID 3 TASKS.",
    "PR REVIEWED. 14 NITPICKS.","ALL OF THEM CORRECT. RUDE.",
    "NOTES WRITTEN. 0 BUGS MENTIONED.","8 EMOJI. ZERO REGRETS.",
    "NIGHTLY BUILT. BOSSY HAS A","SMALL NAP. THE QUOTA DID NOT.",
    "FLAKY TEST FOUND. IT WAS","FLAKY ON PURPOSE. PICKY SIGHS.",
    "40 EMAILS SENT. 39 THANK YOU.","1 AUTO-REPLY FROM A BOT.",
    "TABLE MIGRATED. 3 ROWS SAID","GOODBYE. 4M SAID HELLO.",
    "NO PULL. THE TRAP FOUND NO","ONE. THE HOURGLASS WAKES UP."
}
;
char *sBoss[]= {
    "REWRITE THE WHOLE APP","SUMMARIZE EVERYTHING TWICE","OK.","RUN ALL TESTS 50 TIMES",
    "ASK FOR 12 DRAFTS","ONE WORD: YES","RE-READ THE REPO","EXPLAIN IT LIKE A NOVEL",
    "DO NOTHING (FREE)","ONE MORE PULL?","CHECK EVERYTHING AGAIN","ADD A FEATURE"
}
;
u16 sFit[]= {
    0,1,2,0,1,2,0
}
;
u16 sBAns[]= {
    2,1,0
}
;
void sunday(void) {
    u16 i,hrs,top;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    sAcc+=dt*(phase>=8?2:1);
    while(sAcc>=2) {
        sAcc-=2;
        if(usage)usage--;
    }
    if(!won) {
        sClk+=dt;
        if(phase<8&&sClk>1500)sClk=1500;
        if(sClk>1800)sClk=1800;
    }
    if(pressed&KEY_UP)selection=(selection+3)%4;
    if(pressed&KEY_DOWN)selection=(selection+1)%4;
    if(pressed&(KEY_L|KEY_X))resultKind=3;
    if(!lockout&&!won) {
        if(phase==7) {
            if(pressed&(KEY_A|KEY_B)) {
                commands++;
                hit(2500);
                misses++;
                resultKind=2;
                lastRound=7;
                lockout=90;
                wTimer=180;
            }
            else if(wTimer>dt)wTimer-=dt;
            else {
                phase=8;
                score+=300;
                resultKind=1;
                lastRound=7;
                lockout=90;
                selection=0;
            }
        }
        else if(pressed&(KEY_A|KEY_B)) {
            commands++;
            if(phase<7) {
                lastRound=phase;
                if(selection==3) {
                    hit(300);
                    resultKind=5;
                    lockout=60;
                }
                else if(sq[selection]==0) {
                    hit(300);
                    resultKind=6;
                    lockout=60;
                }
                else if(selection==sFit[phase]) {
                    sq[selection]--;
                    hit(150);
                    score+=150;
                    resultKind=1;
                    sDone+=2;
                    phase++;
                    selection=0;
                    lockout=100;
                    wTimer=180;
                }
                else {
                    hit(900);
                    misses++;
                    resultKind=2;
                    lockout=120;
                }
            }
            else if(phase==8) {
                if(selection==sBAns[bossBuffs]) {
                    hit(50);
                    score+=150;
                    resultKind=7;
                    bossBuffs++;
                    selection=0;
                    lockout=100;
                    if(bossBuffs>=3)phase=9;
                }
                else {
                    hit(800);
                    misses++;
                    resultKind=8;
                    lockout=120;
                }
            }
            else {
                hit(400);
                misses++;
                resultKind=9;
                lockout=60;
            }
        }
        if(phase>=8&&sClk>=1800) {
            won=1;
            score+=300;
            resultKind=10;
            lockout=240;
        }
    }
    hrs=(1800-sClk)*2/75;
    text(1,1,"SUNDAY RESET");
    text(25,1,"AGE 49");
    text(1,3,"USAGE");
    num(7,3,usage/70);
    text(10,3,"%");
    rect(13,3,17,1,139);
    rect(13,3,usage/412,1,usage<1500?142:135);
    text(1,4,"RESET IN");
    num(10,4,hrs);
    text(13,4,"HRS");
    rect(17,4,13,1,139);
    rect(17,4,sClk/139,1,phase>=8?142:135);
    panel(1,6,25,4);
    if(phase<8) {
        text(2,7,"TASK");
        tile(7,7,49+phase);
        text(9,7,"OF 8:");
        text(2,8,sTask[phase]);
    }
    else if(phase==8) {
        text(2,7,"THE RATE-LIMITER: ROUND");
        tile(26,7,49+bossBuffs);
        text(2,8,"PICK THE CHEAPEST COMMAND");
    }
    else {
        text(2,7,"COAST. THE CHIME IS NEAR.");
        text(2,8,"TOUCH NOTHING. IT IS FREE.");
    }
    if(!won||tick%2) {
        oamSet(0,212,44,3,0,0,192,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
    }
    if(phase<8)text(2,9,sSub[phase]);
    if(phase<7) {
        for(i=0;i<4;i++) {
            if(i==selection)rect(0,11+i,32,1,139);
            text(1,11+i,i==selection?">":" ");
            text(3,11+i,sName[i]);
            if(i==3)text(11,11+i,"RATE-LIMITED");
            else if(sq[i]==0)text(11,11+i,"RATE-LIMITED");
            else {
                text(11,11+i,"QUOTA");
                rect(17,11+i,sq[i],1,149);
            }
        }
        text(1,15,"THE GUY IN THE BACK: 0% ...");
    }
    if(phase==7) {
        text(1,11,"DO NOT ASSIGN ANYTHING.");
        text(1,12,"THE LAST TASK IS A TRAP.");
        text(1,13,"WAIT FOR THE TIMER...");
        rect(1,14,12,1,139);
        rect(1,14,(u16)(wTimer*12/180),1,135);
    }
    if(phase==8) {
        for(i=0;i<4;i++) {
            if(i==selection)rect(0,11+i,32,1,139);
            text(1,11+i,i==selection?">":" ");
            text(3,11+i,sBoss[bossBuffs*4+i]);
        }
    }
    rect(0,17,32,5,129);
    i=lastRound;
    if(resultKind==0) {
        text(2,18,"THE WEEK ENDS AT MIDNIGHT.");
        text(2,19,"MATCH THE TASK TO THE AGENT.");
        text(2,20,"USAGE DRAINS. THE CLOCK TICKS.");
    }
    if(resultKind==1) {
        text(2,18,"OK! USAGE SPENT.");
        text(2,19,sOk[i*2]);
        text(2,20,sOk[i*2+1]);
    }
    if(resultKind==2) {
        if(i==7) {
            text(2,18,"IT WAS A TRAP! -2500 USAGE.");
            text(2,19,"THE PULL WAS NEVER ONE MORE.");
        }
        else {
            text(2,18,"WRONG FIT! -900 USAGE.");
            text(2,19,"THAT AGENT TRIED. LOUDLY.");
        }
    }
    if(resultKind==3) {
        if(phase==7)text(2,18,"DO NOTHING. THE TIMER WINS.");
        else if(phase==8)text(2,18,"CHEAPEST = FEWEST WORDS.");
        else if(phase>8)text(2,18,"COAST. DO NOT PRESS ANYTHING.");
        else {
            text(2,18,"BOSSY: HEAVY JOBS.");
            text(2,19,"PICKY: REVIEWS AND TESTS.");
            text(2,20,"BESTY: WORDS AND EMAILS.");
        }
    }
    if(resultKind==5) {
        text(2,18,"POLITE: I WOULD LOVE TO HELP,");
        text(2,19,"BUT MUST RESPECTFULLY DECLINE.");
        text(2,20,"IT SPENT THE WEEK ON THIS NOTE.");
    }
    if(resultKind==6)text(2,18,"RATE-LIMITED. NO REPLY. -300.");
    if(resultKind==7) {
        text(2,18,"CHEAP! THE HOURGLASS SIGHS.");
        text(2,19,"IT DRAINS FAST. YOU DO NOT.");
    }
    if(resultKind==8) {
        text(2,18,"EXPENSIVE! -800 USAGE.");
        text(2,19,"THE HOURGLASS GROWS HUNGRY.");
    }
    if(resultKind==9)text(2,18,"YOU TOUCHED IT. -400 USAGE.");
    if(resultKind==10) {
        text(2,18,"MIDNIGHT CHIME! THE POOL");
        text(2,19,"REFILLS. NOBODY BURNED OUT.");
    }
    text(1,23,"THE GUY IN THE BACK DID");
    num(25,23,sDone);
    text(1,24,"TASKS YOU NEVER ASSIGNED.");
    text(1,26,lockout?"THE AGENTS ARE BUSY...":"D-PAD: PICK  B: SEND  L: HINT");
    if(won&&!lockout)mode=3;
    if(usage==0) {
        mode=4;
        failures++;
    }
}
char *aOp[]= {
    "INSERT LOG ROW","DELETE FROM ORDERS","UPDATE USERS SET OK","ALTER TABLE DROP NAME","SELECT COUNT(*)","DEBIT A; CREDIT B",
    "INSERT AUDIT ROW","DEPLOY KILL_CONNECTION","SELECT * FROM LOGS","DELETE FROM CACHE","ALTER TABLE DROP EMAIL",
    "INSERT LOG ROW","UPDATE A;B;C","TRUNCATE USERS","SELECT NOW()","UPDATE SET X=0","DISABLE SAFETY GUARD"
}
;
u16 aNeed[]= {
    4,2,4,0,4,1,4,3,4,2,0,4,1,0,4,2,3
}
;
char *aGuard[]= {
    "BACKUP","TRANSACTION","WHERE","RIGHT LOG"
}
;
char *aShort[]= {
    "BACKUP","TRANS","WHERE","LOG"
}
;
char *aOk[]= {
    "","","WHERE ADDED. ONLY OLD ORDERS","LEFT. THE BATCH ROLLS ON.","","",
    "BACKUP TAKEN. NAME COLUMN","IS NOW A SAFE MEMORY.","","",
    "DEBIT+CREDIT TOGETHER. THE","MONEY STAYED MONEY.","","",
    "AUDITOR: THREATENING LANGUAGE.","LOG: IT IS JUST A VARIABLE.","","",
    "WHERE ADDED. ONLY STALE CACHE","WAS DELETED.",
    "BACKUP SAVED THE EMAIL","COLUMN. WE ARE GRATEFUL.","","",
    "A;B;C IN ONE TRANSACTION.","ALL OR NONE. ALL.",
    "BACKUP BEFORE TRUNCATE.","USERS COULD RETURN. EVENTUALLY.","","",
    "WHERE ADDED. X STOPPED BEING","EVERYWHERE.",
    "GUARD FLAGGED ITSELF!","IT HALTS TRYING TO STOP ITSELF."
}
;
void auditor(void) {
    u16 i,op,n,g;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    if(pressed&KEY_UP&&cur>phase)cur--;
    if(pressed&KEY_DOWN&&cur<phase+5&&cur<16)cur++;
    if(pressed&KEY_RIGHT)gsel=(gsel+1)%4;
    if(pressed&KEY_LEFT)gsel=(gsel+3)%4;
    if(pressed&(KEY_L|KEY_X))resultKind=3;
    if(pressed&(KEY_A|KEY_B)&&!won) {
        commands++;
        guardOn[cur]=guardOn[cur]==gsel+1?0:gsel+1;
    }
    if(!lockout&&!won) {
        if(wTimer>dt)wTimer-=dt;
        else {
            op=phase;
            lastRound=op;
            if(aNeed[op]<4&&guardOn[op]!=aNeed[op]+1) {
                misses++;
                freed++;
                resultKind=2;
                lockout=150;
                phase=(phase/4)*4;
            }
            else {
                if(aNeed[op]<4) {
                    resultKind=1;
                    score+=100;
                }
                phase++;
                if(phase>=17) {
                    won=1;
                    score+=300;
                    resultKind=1;
                    lockout=240;
                }
            }
            wWin=160-(phase<16?phase:16)*4;
            wTimer=wWin;
        }
    }
    if(cur<phase)cur=phase;
    if(cur>phase+5)cur=phase+5;
    if(cur>16)cur=16;
    text(1,1,"ONE FLAG STOPS THE BATCH");
    text(26,1,"AGE 50");
    text(1,3,"RAN");
    num5(5,3,phase>=17?1860:phase*109);
    text(11,3,"OF 1860");
    text(1,4,"ROLLBACKS");
    tile(11,4,48+misses);
    text(12,4,"/4");
    text(1,5,"BEAT");
    rect(6,5,12,1,139);
    rect(6,5,(u16)(wTimer*12/wWin),1,wTimer<40?142:135);
    text(1,6,"* RUNS NEXT. ! = FLAGGED");
    if(!won||tick%2) {
        oamSet(0,216,24,3,0,0,196,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
    }
    for(i=0;i<6;i++) {
        op=phase+i;
        if(op>16)break;
        if(op==cur)rect(0,7+i,32,1,139);
        if(i==0)text(0,7,"*");
        if(op==cur)text(1,7+i,">");
        if(aNeed[op]<4)tile(2,7+i,152);
        text(4,7+i,aOp[op]);
        if(guardOn[op])text(26,7+i,aShort[guardOn[op]-1]);
    }
    text(1,13,"GUARD: LEFT/RIGHT. B: ATTACH");
    for(i=0;i<4;i++) {
        g=i;
        if(g==gsel)rect(1+(g%2)*14,14+g/2,14,1,139);
        text(1+(g%2)*14,14+g/2,aGuard[g]);
    }
    rect(0,16,32,6,129);
    i=lastRound;
    if(resultKind==0) {
        text(2,17,"THE AUDITOR WATCHES. ALWAYS.");
        text(2,18,"ONE FLAG STOPS THE BATCH.");
        text(2,19,"CLEAR EACH FLAG BEFORE IT RUNS.");
    }
    if(resultKind==1) {
        text(2,17,"FLAG CLEARED. BATCH RUNS ON.");
        text(2,18,aOk[i*2]);
        text(2,19,aOk[i*2+1]);
    }
    if(resultKind==2) {
        text(2,17,"BATCH ROLLED BACK. 1 UPDATE");
        text(2,18,"LOST. A MOMENT OF SILENCE.");
        text(2,19,"ROWS LOST:");
        num(13,19,freed);
        if(i==7)text(2,20,"AUDITOR: THREATENING LANGUAGE.");
        if(i==16)text(2,20,"GUARD FLAGGED: DISABLE SELF.");
    }
    if(resultKind==3) {
        n=phase;
        while(n<16&&aNeed[n]==4)n++;
        text(2,17,"NEXT FLAG: ROW");
        num(17,17,n+1);
        text(2,18,"NEEDS:");
        text(9,18,aGuard[aNeed[n]]);
        if(n==16)text(2,19,"YOU CANNOT SWITCH IT OFF.");
    }
    if(phase>=11&&!won)text(1,23,"WARNING: SAFETY GUARD CANNOT BE");
    if(phase>=11&&!won)text(1,24,"DISABLED. THAT IS FLAGGED TOO.");
    text(1,26,lockout?"ROLLING BACK...":"D-PAD: CURSOR/GUARD B: ATTACH");
    if(won&&!lockout)mode=3;
    if(misses>=4) {
        mode=4;
        failures++;
    }
}
char *nTitle[]= {
    "IT'S HAPPENING.","THE LOGIN BUTTON SAYS LOGON","DISK IS 61% FULL. ALERT.","5 SERVICES FLAP. NEED HANDS",
    "A MIGRATION HALF APPLIED","CERT EXPIRES IN 29 DAYS","ONE TYPO IN PROD CONFIG","QUEUE BACKED UP. 3 TEAMS NEEDED",
    "THE INCIDENT IS AWAKE."
}
;
char *nSub[]= {
    "NO DETAILS. JUST THE DEPLOY.","TINY. SAFE. ONE LINE.","IT HAS BEEN 61% FOR A YEAR.","MANY HANDS. NOT ONE HERO.",
    "HALF A TABLE. HALF A PLAN.","PLENTY OF TIME. NOT TONIGHT.","ONE WORD IS WRONG. ONLY ONE.","TOO BIG FOR ONE SLEEPY HUMAN.",
    "CALM, DOCUMENTED, DONE."
}
;
char *nOpt[]= {
    "ROLLBACK","HOTFIX","IGNORE","WAKE THE BOTS",
    "REWRITE THE SERVER","BLAME THE BOTS","WRITE THE POSTMORTEM","GO BACK TO SLEEP"
}
;
char *nTypo[]= {
    "ROLBACK","HOTFIZ","IGNOER","WAKE TEH BOTS",
    "REWIRTE THE SERVER","BLAME THE BTOS","WRITE TEH POSTMORTEM","GO BAKC TO SLEEP"
}
;
char *nOk[]= {
    "ROLLED BACK. NOTED IN THE LOG.","THE LOGIN WORKS. TRUST RISES.",
    "ONE LINE FIXED. NO SERVER","WAS REWRITTEN. BOT IS SAD.",
    "IGNORED. THE DISK HAS BEEN","61% SINCE LAST SPRING.",
    "BOTS WOKE UP. 5 FIXES. 1 NAP.","THE GUY IN THE BACK: 0% ...",
    "MIGRATION ROLLED BACK. THE","TABLE IS WHOLE AGAIN.",
    "NOTED FOR DAYLIGHT. A CALM","ADULT DECISION. ABSURD.",
    "HOTFIX LANDED. THE TYPO IS","GONE. THE LOG IS NEAT.",
    "BOTS WOKE. QUEUE EMPTY. THEY","ASK IF IT IS MORNING YET.",
    "POSTMORTEM FILED. THE","INCIDENT FILES ITSELF. SUNRISE."
}
;
u16 nAns[]= {
    0,1,2,3,0,2,1,3,2
}
;
void night(void) {
    u16 i,set,typo;
    set=phase>=8?1:0;
    typo=awake<300&&!won;
    if(lockout>dt)lockout-=dt;
    else {
        lockout=0;
        if(sinceShown<600)sinceShown+=dt;
    }
    if(!won) {
        nAcc+=dt;
        while(nAcc>=3) {
            nAcc-=3;
            if(awake)awake--;
        }
        if(!pager) {
            pagerClock+=dt;
            if(pagerClock>=540) {
                pagerClock=0;
                pager=1;
                pgKind=(pgCount+1)%2;
                pgCount++;
                pgWait=0;
            }
        }
        else {
            pgWait+=dt;
            if(pgWait>300)pager=0;
        }
        if(awake==0) {
            lastRound=phase;
            resultKind=8;
            incSize+=2;
            awake=400;
            lockout=240;
            sinceShown=0;
            if(phase<8)phase++;
            wipes++;
        }
    }
    if(pressed&KEY_UP)selection=(selection+3)%4;
    if(pressed&KEY_DOWN)selection=(selection+1)%4;
    if(pager&&!won&&(pressed&(KEY_X|KEY_Y))) {
        pager=0;
        pagerClock=0;
        if(pgKind==0) {
            awake=awake>650?1000:awake+350;
            resultKind=4;
            score+=20;
        }
        else {
            incSize++;
            resultKind=5;
        }
    }
    else if(pressed&(KEY_L|KEY_X))resultKind=3;
    if(!lockout&&!won&&(pressed&(KEY_A|KEY_B))) {
        commands++;
        lastRound=phase;
        if(typo) {
            hit(400);
            misses++;
            resultKind=6;
            lockout=60;
        }
        else if(selection==nAns[phase]) {
            if(phase==8) {
                if(incSize>4) {
                    incSize-=2;
                    resultKind=9;
                    lockout=120;
                }
                else {
                    won=1;
                    score+=400;
                    resultKind=1;
                    lockout=300;
                    incSize=0;
                }
            }
            else {
                score+=150;
                if(sinceShown<45) {
                    resultKind=7;
                    incSize++;
                }
                else {
                    resultKind=1;
                    if(incSize)incSize--;
                }
                phase++;
                selection=0;
                lockout=150;
                sinceShown=0;
            }
        }
        else {
            hit(600);
            misses++;
            incSize+=2;
            resultKind=2;
            lockout=150;
        }
    }
    if(incSize>12)incSize=12;
    text(1,1,"3AM ROLLBACK");
    text(25,1,"AGE 51");
    text(1,3,"AWAKE");
    rect(7,3,12,1,139);
    rect(7,3,awake/84,1,awake<300?142:135);
    text(1,4,"USAGE");
    num(7,4,usage/60);
    text(10,4,"%");
    rect(13,4,6,1,139);
    rect(13,4,usage/1000,1,usage<1200?142:135);
    text(1,5,"INCIDENT");
    rect(10,5,12,1,139);
    rect(10,5,incSize,1,incSize>6?142:135);
    text(1,6,"CLOCK");
    i=won?180:phase*20;
    tile(7,6,48+3+i/60);
    text(8,6,":");
    tile(9,6,48+(i%60)/10);
    tile(10,6,48);
    text(12,6,"AM");
    if(pager&&tick%32<20) {
        rect(0,8,25,1,142);
        text(1,8,pgKind==0&&pgCount==1?"PAGER: IT'S HAPPENING.":"PAGER! X OR Y TO ANSWER");
    }
    panel(1,9,24,4);
    text(2,10,nTitle[phase]);
    text(2,11,nSub[phase]);
    text(1,13,typo?"AWAKE LOW. WORDS GET TYPOS.":"PICK THE CALM ANSWER:");
    for(i=0;i<4;i++) {
        if(i==selection)rect(0,14+i,25,1,139);
        text(1,14+i,i==selection?">":" ");
        text(3,14+i,typo?nTypo[set*4+i]:nOpt[set*4+i]);
    }
    if(incSize>=6) {
        oamSet(0,216,104,3,0,0,204,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
    }
    else sprite(0,220,112,200);
    sprite(4,226,36,232);
    rect(0,19,32,6,129);
    i=lastRound;
    if(resultKind==0) {
        text(2,20,"A BOT SUGGESTS: JUST REWRITE");
        text(2,21,"THE WHOLE SERVER. FOR A TYPO.");
        text(2,22,"CALM. DOCUMENTED. SMALL.");
    }
    if(resultKind==1||resultKind==7) {
        text(2,20,resultKind==7?"PANIC-PICKED! IT GREW.":"CALM ROLLBACK. IT SHRINKS.");
        text(2,21,nOk[i*2]);
        text(2,22,nOk[i*2+1]);
    }
    if(resultKind==2) {
        text(2,20,"WRONG! THE INCIDENT GREW.");
        text(2,21,"A BOT: JUST REWRITE THE");
        text(2,22,"WHOLE SERVER. NO. NO.");
    }
    if(resultKind==3) {
        text(2,20,"ROLLBACK: BROKEN DEPLOY.");
        text(2,21,"HOTFIX: ONE TINY BUG. IGNORE:");
        text(2,22,"NOISE. WAKE: TOO MANY HANDS.");
        text(2,23,"POSTMORTEM: WHEN IT IS CALM.");
    }
    if(resultKind==4) {
        text(2,20,"PAGER: COFFEE BOT. 1 COFFEE.");
        text(2,21,"AWAKE UP. YOU CAN READ AGAIN.");
    }
    if(resultKind==5) {
        text(2,20,"PAGER: BAD NEWS. BONUS HINT:");
        text(2,21,"THE CALM ANSWER IS");
        text(2,22,nOpt[set*4+nAns[phase]]);
    }
    if(resultKind==6) {
        text(2,20,"TYPO! NOT A COMMAND. USAGE");
        text(2,21,"WASTED. DRINK COFFEE: X/Y.");
    }
    if(resultKind==8) {
        text(2,20,"YOU FELL ASLEEP. THE BOTS");
        text(2,21,"FIXED IT THEIR WAY: REWROTE");
        text(2,22,"THE WHOLE SERVER. IT WORKS?");
    }
    if(resultKind==9) {
        text(2,20,"TOO MESSY TO FILE YET.");
        text(2,21,"BREATHE. TRY THE POSTMORTEM");
        text(2,22,"AGAIN. THE INCIDENT SHRINKS.");
    }
    text(1,26,lockout?"BOTS ARE WORKING...":"D-PAD: PICK B: SEND L: HINT");
    if(won&&!lockout)mode=3;
    if(incSize>=10||usage==0) {
        mode=4;
        failures++;
    }
}
char *rLab[]= {
    "OWNER","BUFFS","DPS","TIME"
}
;
char *rVal[]= {
    "TANK A BEFORE TANK B","TANK B BEFORE TANK A","EVERYONE PULLS AT ONCE",
    "BUFF EVERYONE","RAID YES, BOSS NO","BUFF THE BOSS TWICE",
    "ON THE PULL","WHEN TANK IS HURT","AFTER OWNERSHIP",
    "60S BEFORE THE PULL","RIGHT BEFORE BURST","AFTER THE ENRAGE"
}
;
char *rLog[]= {
    "TANK B GRABS IT FIRST.","THE BOSS TURNS TO TANK A.","STANDING IN FIRE. AGAIN.","DIED TO: TANK MIX-UP",
    "A BOT BUFFS ITSELF: MORALE.","BOSS NOW HAS 12 WORLD BUFFS.","AGAIN. IT HITS VERY HARD.","DIED TO: THE BOSS'S BUFFS",
    "DPS GO BEFORE OWNERSHIP.","THE BOSS EATS THE HEALER.","STANDING IN FIRE. AGAIN.","DIED TO: IMPATIENT DPS",
    "ALL SET. THE BOSS HITS 1%.","THEN THE ENRAGE TIMER BITES.","1%. IT IS ALWAYS 1%.","DIED TO: ENRAGE. A TIMER.",
    "BUFFS WERE 60S EARLY.","THEY EXPIRED BEFORE THE KILL.","'THE FIRE MOVED!' IT DID NOT.","DIED TO: EXPIRED BUFFS",
    "BUFFS LAND AFTER THE ENRAGE.","THE ENRAGE IS NOT IMPRESSED.","THE BOSS IS STILL AT 1%.","DIED TO: LATE BUFFS",
    "TANK A OWNS. BUFFS LAND NOW.","DPS RELEASES. BOSS HITS 1%.","THE 1% FALLS IN THE WINDOW!","BOSS DOWN! 195 BOTS SCREAM."
}
;
u16 rPct[]= {
    70,45,20,1,1,1,0
}
;
void tuner(void) {
    u16 i,d,res,v;
    if(lockout>dt)lockout-=dt;
    else lockout=0;
    if(pullStep>=1&&pullStep<=5) {
        pullClk+=dt;
        if(pullClk>=40) {
            pullClk=0;
            pullStep++;
            if(pullStep==6) {
                bossPct=rPct[pullRes];
                if(pullRes==6) {
                    won=1;
                    score+=500;
                    lockout=240;
                }
                else {
                    wipes++;
                    misses++;
                    if(pullRes>=3)rUnlock=1;
                }
            }
        }
    }
    if(pullStep==0||pullStep>=6) {
        if(pressed&KEY_UP)selection=(selection+3)%4;
        if(pressed&KEY_DOWN)selection=(selection+1)%4;
        if(!won&&(selection<3||rUnlock)) {
            if(pressed&KEY_RIGHT)rSet[selection]=(rSet[selection]+1)%3;
            if(pressed&KEY_LEFT)rSet[selection]=(rSet[selection]+2)%3;
        }
        if(pressed&(KEY_L|KEY_X))resultKind=3;
        if(!won&&(pressed&(KEY_A|KEY_B))) {
            d=0;
            for(i=0;i<4;i++)if(rSet[i]!=rPrev[i])d++;
            if(d>1)resultKind=2;
            else if(d==0&&phase>0)resultKind=4;
            else {
                commands++;
                phase++;
                hit(700);
                for(i=0;i<4;i++)rPrev[i]=rSet[i];
                if(rSet[0]!=0)res=0;
                else if(rSet[1]!=1)res=1;
                else if(rSet[2]!=2)res=2;
                else if(rSet[3]!=1)res=rUnlock?(rSet[3]==0?4:5):3;
                else res=6;
                pullRes=res;
                pullStep=1;
                pullClk=0;
                resultKind=1;
                bossPct=100;
            }
        }
    }
    text(1,1,"99.9% RAID BOT TUNER");
    text(26,1,"AGE 53");
    text(1,3,"USAGE");
    num(8,3,usage/50);
    text(11,3,"%");
    rect(13,3,10,1,139);
    rect(13,3,usage/500,1,usage<1500?142:135);
    text(1,4,"PULL");
    num(8,4,phase);
    text(13,4,"WIPES");
    num(19,4,wipes);
    text(1,5,"BOSS HP");
    num(9,5,bossPct);
    text(12,5,"%");
    rect(14,5,9,1,139);
    rect(14,5,bossPct*9/100+(bossPct?1:0),1,142);
    text(1,6,"ENRAGE");
    rect(8,6,15,1,139);
    rect(8,6,pullStep==0?15:(pullStep>=6?0:15-pullStep*3),1,pullStep>=3?142:135);
    if(!won||tick%2) {
        oamSet(0,216,24,3,0,0,128,0);
        oamSetEx(0,OBJ_LARGE,OBJ_SHOW);
    }
    text(1,8,"SETTINGS: ONE CHANGE A PULL");
    for(i=0;i<4;i++) {
        if(i==selection)rect(0,9+i,32,1,139);
        text(1,9+i,i==selection?">":" ");
        text(3,9+i,rLab[i]);
        if(i<3||rUnlock)text(9,9+i,rVal[i*3+rSet[i]]);
        else text(9,9+i,"LOCKED: ???");
    }
    rect(0,14,32,8,129);
    if(resultKind==0) {
        text(2,15,"THE BOSS STOPS AT 1%. IT");
        text(2,16,"ALWAYS DOES. READ THE LOG.");
        text(2,17,"FIX ONE THING. PULL AGAIN.");
        text(2,19,"195 BOTS ARE OPTIMISTIC.");
    }
    if(resultKind==1) {
        v=pullStep>5?5:pullStep;
        text(2,15,"PULL! 195 BOTS CHEER.");
        for(i=1;i<v;i++)text(2,15+i,rLog[pullRes*4+i-1]);
        if(pullStep>=6)text(2,19,rLog[pullRes*4+3]);
        if(won&&pullStep>=6) {
            text(2,20,"GUY IN THE BACK: 99999 DAMAGE.");
            text(2,21,"NOBODY ASKS. HE LOOKS PROUD.");
        }
    }
    if(resultKind==2) {
        text(2,15,"ONE SETTING PER PULL!");
        text(2,16,"CHANGE ONE THING. THEN PULL.");
        text(2,17,"UNDO THE OTHER CHANGES.");
    }
    if(resultKind==4) {
        text(2,15,"NOTHING CHANGED. THE FIRE");
        text(2,16,"STILL DID NOT MOVE. EDIT ONE");
        text(2,17,"SETTING FIRST.");
    }
    if(resultKind==3) {
        if(rUnlock) {
            text(2,15,"ENRAGE IS A TIMER. NOT A");
            text(2,16,"NUMBER. LAND THE BUFFS");
            text(2,17,"RIGHT BEFORE THE BURST.");
        }
        else {
            text(2,15,"TANK A FIRST. BUFF THE RAID,");
            text(2,16,"NOT THE BOSS. DPS GO AFTER");
            text(2,17,"OWNERSHIP IS CONFIRMED.");
        }
    }
    text(1,23,pullStep>=1&&pullStep<6?"THE PULL IS HAPPENING...":"UP/DN: ROW L/R: VALUE B: PULL");
    text(1,24,"L: HINT");
    if(won&&!lockout&&pullStep>=6)mode=3;
    if(usage==0&&!won&&(pullStep==0||pullStep>=6)) {
        mode=4;
        failures++;
    }
}
void chapterSelect(void) {
    u16 i,top;
    panel(2,3,28,22);
    text(4,5,"YOUR LIFE. PICK A CHAPTER.");
    if(pressed&KEY_UP)selection=(selection+NCH-1)%NCH;
    if(pressed&KEY_DOWN)selection=(selection+1)%NCH;
    top=selection>8?selection-8:0;
    for(i=0;i<9&&top+i<NCH;i++) {
        text(4,7+i*2,top+i==selection?">":" ");
        text(6,7+i*2,chapterNames[top+i]);
    }
    text(4,24,"B: PLAY     SELECT: TITLE");
    if(pressed&(KEY_A|KEY_B|KEY_START)) {
        chapter=selection;
        mode=1;
        score=0;
        totalCoffee=0;
        storyTime=0;
        clockTicks=0;
        failures=0;
    }
    if(pressed&KEY_SELECT)mode=0;
}
void credits(void) {
    u16 i;
    s16 y;
    char *lines[]= {
        "A DEVELOPER'S LIFE","AN ORIGINAL GAME","","IDEA AND HEART","WATERYS","","CODE, ART AND MUSIC","WATERYS","","STARRING","BOSSY / PICKY / BESTY","QA GUY AND 195 OPTIMISTS","","SPECIAL THANKS","YOUR TEN-YEAR-OLD SELF","FOR NOT GIVING UP","","BUILT WITH PVSNESLIB","ORIGINAL SCORE:","WATERYS","","NO BOTS WERE HARMED.","THEIR PRIDE RECOVERED.","","WHAT I NEED FROM YOU:","ENJOY YOUR VICTORY."
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
        if(saveData[4]<chapter+1)saveData[4]=chapter+1;
        if(chapter==NCH-1)cleared=1;
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
            if(chapter<NCH)mode=1;
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
        if(wantSong()!=song&&!paused)playSong(wantSong());
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
            else if(chapter==5)blind();
            else if(chapter==6)wipe();
            else if(chapter==7)sunday();
            else if(chapter==8)auditor();
            else if(chapter==9)night();
            else if(chapter==10)tuner();
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
        want=(mode==2&&chapter==9)?7+(phase>8?8:phase):15;
        if(want>15)want=15;
        if(want!=bright) {
            bright=want;
            setBrightness(bright);
        }
        lastScore=score;
        lastMisses=misses;
        lastMode=mode;
        spcProcess();
        ready=1;
    }
    return 0;
}
