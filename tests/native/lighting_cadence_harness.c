/* Production Loop control flow with virtual time/render/HID only. No devices,
   threads, real waits or duplicate scheduler/backoff implementation. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s line %d: %s\n",scenario,__LINE__,#c); exit(1); } } while (0)
#define FREQ 1000000L
static const char *scenario;
static bool running, DeviceLost, ExclusiveLock, WantExclusiveLock;
static int Fps, WriteUs, LateFrames, LockRetries, overrun;
static double Speed, nextLockTry, lockTryGap;
static long ticks, frameTicks[10000];
static double frameTime[10000], frameDt[10000];
static int frames, frameLimit, workload, begins, ends, wakes;
static double Min(double a,double b) { return a<b?a:b; }
int Max(int a,int b) { return a>b?a:b; }
static void ClockStart(void) { ticks=0; }
static long ClockTicks(void) { return ticks; }
static void SleepMs(int ms) { ticks+=(long)ms*1000; }
static void Spin(int count) { (void)count; ticks++; }
static void timeBeginPeriod(int n) { CHECK(n==1); begins++; }
static void timeEndPeriod(int n) { CHECK(n==1); ends++; }
static bool Reopen(void) { DeviceLost=false; return true; }
static bool TryExclusiveUpgrade(void) { ExclusiveLock=true; return true; }
static void ReAssert(bool force) { if(force) wakes++; }
static void Frame(double t,double dt) {
    CHECK(frames<10000);frameTicks[frames]=ticks;frameTime[frames]=t;frameDt[frames]=dt;frames++;
    if(workload==3 && frames%120==0) ticks+=18000; /* cheap HID, late CPU */
}
static void Push(void) {
    WriteUs=2000;
    if(workload==1 && frames%60==0) WriteUs=20000; /* isolated expensive miss */
    if((workload==2 || workload==3) && frames%120!=0) WriteUs=100000;
    if(workload==4) WriteUs=150000; /* sustained device capacity limit */
    if(workload==5 && frames<=120) WriteUs=100000;
    ticks+=WriteUs;
}
static void PublishFrame(void) { if(frames>=frameLimit) running=false; }
#include "lighting_cadence_subject.h"
static void Run(int work,int count,int fps,double speed) {
    running=true;DeviceLost=false;ExclusiveLock=WantExclusiveLock=true;
    Fps=fps;Speed=speed;WriteUs=LateFrames=LockRetries=overrun=0;
    nextLockTry=lockTryGap=3.0;frames=begins=ends=wakes=0;frameLimit=count;workload=work;
    Loop();
    CHECK(frames==count && begins==1 && ends==1 && !wakes);
}
int main(int argc,char **argv) {
    CHECK(argc==2);scenario=argv[1];
    if(!strcmp(scenario,"speed")) {
        double speeds[]={0.0,0.25,0.5,1.0,2.0};
        for(int s=0;s<5;s++) {
            Run(0,360,60,speeds[s]);CHECK(Fps==60 && LateFrames==0);
            for(int i=0;i<frames;i++) {
                CHECK(frameTicks[i]==i*(FREQ/60));CHECK(frameDt[i]>0);
            }
        }
    } else if(!strcmp(scenario,"wall-clock")) {
        double speeds[]={0.0,0.25,0.5,1.0,2.0};
        for(int s=0;s<5;s++) {
            Run(0,360,60,speeds[s]);
            for(int i=0;i<frames;i++) CHECK(fabs(frameTime[i]-frameTicks[i]/(double)FREQ)<1e-10);
        }
    } else if(!strcmp(scenario,"isolated")) {
        Run(1,7800,60,0.5);CHECK(Fps==60 && overrun==1 && LateFrames==130);
    } else if(!strcmp(scenario,"recovered")) {
        Run(2,120,60,0.5);CHECK(Fps==60 && overrun==0 && LateFrames==119);
    } else if(!strcmp(scenario,"cheap-late")) {
        Run(3,240,60,0.5);CHECK(Fps==60 && overrun==0 && LateFrames==240);
    } else if(!strcmp(scenario,"sustained")) {
        Run(4,119,60,0.5);CHECK(Fps==60 && overrun==119);
        Run(4,120,60,0.5);CHECK(Fps==48 && overrun==0);
        Run(5,132,60,0.5);CHECK(Fps==48 && overrun==0);
        for(int i=121;i<frames;i++) CHECK(frameTicks[i]-frameTicks[i-1]==FREQ/48);
    } else if(!strcmp(scenario,"floor")) {
        Run(4,1000,60,0.5);CHECK(Fps==15);
        Run(4,120,17,0.5);CHECK(Fps==15);
        Run(4,1000,10,0.5);CHECK(Fps==10); /* a lower device cap is never raised */
    } else CHECK(false);
    printf("Lighting cadence %s passed\n",scenario);return 0;
}
