/* Actual embedded C# UseDither/Dither/Push, mechanically adapted by the Python
   test. Only allocation, clock, pacing and HID I/O are substituted. No devices. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef unsigned char byte;
static const char *scenario;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s line %d: %s\n", scenario, __LINE__, #c); exit(1); } } while (0)
static int LampCount, Slots, ReportLen, nbatch, DitherMode, MaxR, MaxG, MaxB;
static int OffR, OffG, OffB, OffFlags, ReportPaceUs, WriteUs, WriteUsMax, WriteFails, failRun;
static bool Interleaved, DeviceLost;
static void *h;
static double Fr[16], Fg[16], Fb[16], Er[16], Eg[16], Eb[16];
static double *fr=Fr, *fg=Fg, *fb=Fb, *er=Er, *eg=Eg, *eb=Eb;
static double *frPrev, *fgPrev, *fbPrev;
static int Pr[16], Pg[16], Pb[16], *pr=Pr, *pg=Pg, *pb=Pb, *nr, *ng, *nb2;
static byte reports[2][51], sent[2][51], *bufs[2]={reports[0],reports[1]};
static int writes, failAt, events[8], eventCount;
static int Min(int a,int b) { return a<b?a:b; }
static double Max(double a,double b) { return a>b?a:b; }
static int *AllocInts(int n) { int *p=calloc((size_t)n,sizeof(*p)); CHECK(p); return p; }
static double *AllocDoubles(int n) { double *p=calloc((size_t)n,sizeof(*p)); CHECK(p); return p; }
static long Timestamp(void) { static long tick; return tick+=100; }
static bool HidD_SetFeature(void *handle,byte *b,int len) {
    (void)handle; CHECK(len==ReportLen && eventCount<8);
    int bi=b==bufs[0]?0:1;
    events[eventCount++]=bi+1; writes++; memcpy(sent[bi],b,(size_t)len);
    return writes!=failAt;
}
static void Pace(int us) { CHECK(eventCount<8); events[eventCount++]=-us; }
#include "lighting_colour_subject.h"
static void ClearIO(int fail) { writes=eventCount=0; failAt=fail; memset(sent,0,sizeof(sent)); }
static void Fixture(int mode,int scale,bool interleaved) {
    free(nr);free(ng);free(nb2);free(frPrev);free(fgPrev);free(fbPrev);
    nr=ng=nb2=NULL;frPrev=fgPrev=fbPrev=NULL;
    LampCount=16;Slots=8;ReportLen=51;nbatch=2;DitherMode=mode;MaxR=MaxG=MaxB=scale;
    Interleaved=interleaved;OffFlags=1;OffR=19;OffG=interleaved?20:27;OffB=interleaved?21:35;
    ReportPaceUs=1000;WriteUs=WriteUsMax=WriteFails=failRun=0;DeviceLost=false;
    memset(reports,0,sizeof(reports));memset(Fr,0,sizeof(Fr));memset(Fg,0,sizeof(Fg));memset(Fb,0,sizeof(Fb));
    for(int i=0;i<LampCount;i++) {
        pr[i]=pg[i]=pb[i]=-1;
        er[i]=fmod(i*0.6180339887498949,1.0)-0.5;
        eg[i]=fmod(i*0.6180339887498949+0.3333,1.0)-0.5;
        eb[i]=fmod(i*0.6180339887498949+0.6667,1.0)-0.5;
    }
    ClearIO(0);
}
static void Fill(double r,double g,double b) {
    for(int i=0;i<LampCount;i++) { fr[i]=r;fg[i]=g;fb[i]=b; }
}
static int RoundCode(double v,int scale) { return (int)(v*scale/255.0+0.5); }
static void CheckRounded(void) {
    for(int i=0;i<LampCount;i++) {
        CHECK(pr[i]==RoundCode(fr[i],MaxR) && pg[i]==RoundCode(fg[i],MaxG) && pb[i]==RoundCode(fb[i],MaxB));
        CHECK(er[i]==0.0 && eg[i]==0.0 && eb[i]==0.0);
    }
}
static void TestSteady(void) {
    for(int coarse=0;coarse<2;coarse++) for(int layout=0;layout<2;layout++)
    for(int neutral=0;neutral<2;neutral++) for(int down=0;down<2;down++) {
        Fixture(0,coarse?63:255,layout!=0);
        int previous=down?255:0;
        for(int frame=0;frame<120;frame++) {
            double v=down?20.0-frame*0.1:8.1+frame*0.1;
            Fill(v,neutral?v:v*0.4,neutral?v:v*0.15);
            ClearIO(0);Push(false);CheckRounded();
            CHECK(down?pr[0]<=previous:pr[0]>=previous);previous=pr[0];
        }
    }
}
static void TestModes(void) {
    for(int coarse=0;coarse<2;coarse++) for(int mode=-1;mode<=1;mode++) {
        Fixture(mode,coarse?63:255,false);
        bool enabled=mode==1 || (mode==-1 && coarse), differs=false;
        CHECK(UseDither()==enabled);
        for(int frame=0;frame<180;frame++) {
            Fill(8.1+frame*0.08,3.1+frame*0.06,1.3+frame*0.05);
            ClearIO(0);Push(false);
            if(!enabled) CheckRounded();
            for(int i=0;i<LampCount;i++) {
                if(pr[i]!=RoundCode(fr[i],MaxR) || pg[i]!=RoundCode(fg[i],MaxG) || pb[i]!=RoundCode(fb[i],MaxB)) differs=true;
            }
        }
        CHECK(differs==enabled);
        /* Switching Steady on clears a carry immediately, not one frame later. */
        DitherMode=0;Fill(8.35,3.25,1.15);ClearIO(0);Push(false);CheckRounded();
    }
    Fixture(-1,255,false);MaxG=63;CHECK(UseDither());MaxG=255;MaxB=63;CHECK(UseDither());
}
static void TestHeldBlack(void) {
    Fixture(1,255,false);Fill(8.35,3.25,1.15);Push(false);
    ClearIO(0);Push(false);CheckRounded();
    ClearIO(0);Push(false);CHECK(writes==0);
    for(int i=0;i<LampCount;i++) er[i]=eg[i]=eb[i]=0.8;
    Fill(0.0004,0.0004,0.0004);ClearIO(0);Push(false);CheckRounded();
    ClearIO(0);Push(false);CHECK(writes==0);
    ClearIO(0);Push(true);CHECK(writes==2);
}
static void TestNeutral(void) {
    for(int mixed=0;mixed<3;mixed++) {
        Fixture(1,mixed==1?63:255,false);
        if(mixed==2) { MaxG=127;MaxB=63; }
        for(int frame=0;frame<80;frame++) {
            double code=8.1+frame*0.1;
            Fill(code*255.0/MaxR,code*255.0/MaxG,code*255.0/MaxB);
            ClearIO(0);Push(false);
            for(int i=0;i<LampCount;i++) {
                CHECK(pr[i]==pg[i] && pg[i]==pb[i]);
                CHECK(er[i]==eg[i] && eg[i]==eb[i]);
                CHECK(pr[i]>=floor(code) && pr[i]<=ceil(code));
            }
        }
    }
}
static void TestRetry(void) {
    Fixture(0,255,false);Fill(20.2,5,2);ClearIO(2);Push(false);
    CHECK(writes==2 && WriteFails==1 && failRun==1 && !DeviceLost);
    for(int i=0;i<LampCount;i++) CHECK(pr[i]==-1 && pg[i]==-1 && pb[i]==-1);
    ClearIO(0);Push(false);CHECK(writes==2 && failRun==0);CheckRounded();
    ClearIO(0);Push(false);CHECK(writes==0);
    Fill(40.2,10,6);
    for(int frame=0;frame<8;frame++) {
        ClearIO(1);Push(false);CHECK(writes==2);
        for(int i=0;i<LampCount;i++) CHECK(pr[i]==20 && pg[i]==5 && pb[i]==2);
    }
    CHECK(DeviceLost && failRun==0 && WriteFails==9);
    ClearIO(0);Push(false);CHECK(writes==2);CheckRounded();
}
static void TestLayout(void) {
    for(int layout=0;layout<2;layout++) for(int partial=0;partial<2;partial++) {
        Fixture(0,255,layout!=0);if(partial) LampCount=11;
        for(int i=0;i<LampCount;i++) { fr[i]=20+i;fg[i]=50+i;fb[i]=80+i; }
        Push(true);CHECK(writes==2 && eventCount==3);
        CHECK(events[0]==1 && events[1]==-1000 && events[2]==2);
        CHECK(sent[0][OffFlags]==0 && sent[1][OffFlags]==1);
        for(int i=0;i<LampCount;i++) {
            int bi=i/Slots,s=i%Slots,st=layout?s*4:s;
            CHECK(sent[bi][OffR+st]==pr[i] && sent[bi][OffG+st]==pg[i] && sent[bi][OffB+st]==pb[i]);
        }
        ReportPaceUs=0;ClearIO(0);Push(true);CHECK(eventCount==2 && writes==2);
        LampCount=4;nbatch=1;ReportPaceUs=1000;ClearIO(0);Push(true);
        CHECK(eventCount==1 && writes==1 && sent[0][OffFlags]==1);
    }
}
int main(int argc,char **argv) {
    CHECK(argc==2);scenario=argv[1];
    if(!strcmp(scenario,"steady")) TestSteady();
    else if(!strcmp(scenario,"modes")) TestModes();
    else if(!strcmp(scenario,"held-black")) TestHeldBlack();
    else if(!strcmp(scenario,"neutral")) TestNeutral();
    else if(!strcmp(scenario,"retry")) TestRetry();
    else if(!strcmp(scenario,"layout")) TestLayout();
    else CHECK(false);
    free(nr);free(ng);free(nb2);free(frPrev);free(fgPrev);free(fbPrev);
    printf("Lighting colour %s passed\n",scenario);return 0;
}
