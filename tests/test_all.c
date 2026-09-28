#include "scheduler.h"
#include "queue.h"
#include "csv_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int tests, failures;
#define CHECK(label, x) do { ++tests; if (!(x)) { ++failures; printf("FAIL %s (line %d)\n", label, __LINE__); } else printf("PASS %s\n", label); } while (0)
static Process process(const char *pid, int64_t arrival, int64_t burst) {
    Process p = {0}; strcpy(p.pid, pid); p.arrival = arrival; p.burst = burst; p.remaining = burst; p.first_run = -1; return p;
}
static int run(Process *p, size_t n, Algorithm a, int64_t q, ScheduleResult *r) {
    char error[256];
    if (!schedule(p,n,a,q,r,error,sizeof(error))) { printf("Unexpected failure: %s\n",error); ++failures; return 0; }
    return 1;
}
static int invariants(const ScheduleResult *r) {
    size_t i; int64_t busy = 0, idle = 0;
    for (i = 0; i < r->count; ++i) {
        const Process *p = &r->processes[i];
        int64_t service = 0; size_t j;
        if (p->remaining || p->first_run < p->arrival || p->completion - p->arrival < p->burst) return 0;
        for (j = 0; j < r->slices; ++j) if (r->timeline[j].process == i) service += r->timeline[j].end-r->timeline[j].start;
        if (service != p->burst) return 0;
        busy += p->burst;
    }
    for (i = 0; i < r->slices; ++i) {
        const TimelineSlice *s = &r->timeline[i];
        if (s->end <= s->start || s->start != (i ? r->timeline[i-1].end : 0)) return 0;
        if (i && s->process == r->timeline[i-1].process) return 0;
        if (s->process == SIZE_MAX) idle += s->end-s->start;
    }
    return idle == r->idle && busy+idle == r->finish;
}
static void scheduling_tests(void) {
    Process p[4], before[4]; ScheduleResult r; char error[256]; int a;
    p[0]=process("P1",0,5); p[1]=process("P2",1,3); p[2]=process("P3",2,1); p[3]=process("P4",6,4);
    memcpy(before,p,sizeof(p));
    for (a=0; a<3; ++a) {
        if (!run(p,4,(Algorithm)a,2,&r)) continue;
        CHECK("all completed / nonnegative metrics / service conserved",invariants(&r));
        CHECK("original input unchanged",memcmp(p,before,sizeof(p))==0);
        if (a==FCFS) CHECK("FCFS sample exact metrics",r.avg_waiting==3.25 && r.avg_turnaround==6.5 && r.avg_response==3.25 && r.switches==3);
        if (a==SJF) CHECK("SJF sample exact metrics",r.avg_waiting==2.75 && r.avg_turnaround==6.0 && r.avg_response==2.75 && r.switches==3 && r.timeline[1].process==2);
        if (a==RR) CHECK("RR sample exact metrics",r.avg_waiting==3.75 && r.avg_turnaround==7.0 && r.avg_response==1.25 && r.switches==7);
        result_destroy(&r);
    }
    p[0]=process("single",0,9);
    for(a=0;a<3;++a) if(run(p,1,(Algorithm)a,2,&r)) {
        CHECK("single process zero waiting and response",r.avg_waiting==0 && r.avg_response==0 && r.finish==9);
        CHECK("consecutive slices merge without switches",r.slices==1 && r.switches==0); result_destroy(&r);
    }
    p[0]=process("late",8,1); p[1]=process("early",3,2);
    for(a=0;a<3;++a) if(run(p,2,(Algorithm)a,2,&r)) {
        CHECK("initial and intermediate idle / arrival order",r.idle==6 && r.finish==9 && r.slices==4 && r.timeline[1].process==1 && r.switches==0); result_destroy(&r);
    }
    p[0]=process("long",0,5); p[1]=process("future",1,1);
    if(run(p,2,SJF,0,&r)) { CHECK("SJF cannot select future job or preempt",r.timeline[0].process==0 && r.timeline[0].end==5); result_destroy(&r); }
    p[0]=process("first",0,3); p[1]=process("second",0,1); p[2]=process("third",0,1);
    if(run(p,3,SJF,0,&r)) { CHECK("SJF shortest then stable input tie",r.timeline[0].process==1 && r.timeline[1].process==2); result_destroy(&r); }
    if(run(p,3,FCFS,0,&r)) { CHECK("FCFS simultaneous arrivals stable",r.timeline[0].process==0 && r.timeline[1].process==1); result_destroy(&r); }
    p[0]=process("running",0,5); p[1]=process("late",2,1); p[2]=process("earlier",1,1);
    if(run(p,3,SJF,0,&r)) { CHECK("SJF equal burst uses earlier arrival",r.timeline[1].process==2); result_destroy(&r); }
    if(run(p,3,RR,2,&r)) { CHECK("RR during and boundary arrivals before requeue",r.timeline[1].process==2 && r.timeline[2].process==1 && r.timeline[3].process==0 && r.processes[0].first_run==0); result_destroy(&r); }
    CHECK("zero quantum rejected",!schedule(p,3,RR,0,&r,error,sizeof(error)));
    CHECK("negative quantum rejected",!schedule(p,3,RR,-1,&r,error,sizeof(error)));
    p[0]=process("large",INT64_MAX,1);
    CHECK("time overflow rejected",!schedule(p,1,FCFS,0,&r,error,sizeof(error)));
    p[0]=process("limit",0,1000001);
    CHECK("RR resource limit rejected",!schedule(p,1,RR,1,&r,error,sizeof(error)));
}
static void queue_tests(void) {
    Queue q; size_t item = 0;
    CHECK("zero queue rejected",!queue_init(&q,0));
    if(!queue_init(&q,2)) { ++failures; return; }
    CHECK("empty pop rejected",!queue_pop(&q,&item));
    CHECK("fill queue",queue_push(&q,10) && queue_push(&q,20));
    CHECK("full push rejected",!queue_push(&q,30));
    CHECK("FIFO pop",queue_pop(&q,&item) && item==10);
    CHECK("wraparound",queue_push(&q,30) && queue_pop(&q,&item) && item==20 && queue_pop(&q,&item) && item==30);
    queue_destroy(&q); queue_destroy(&q);
}
static void csv_case(const char *label,const char *data,int expected,const char *message) {
    FILE *f=fopen("fixture.tmp","wb"); Process *p=NULL; size_t n=0; char error[256]={0}; int ok;
    if(!f) { ++failures; return; }
    fputs(data,f); fclose(f);
    ok=read_processes("fixture.tmp",&p,&n,error,sizeof(error));
    CHECK(label,ok==expected && (expected ? n>0 : strstr(error,message)!=NULL));
    free(p); remove("fixture.tmp");
}
/* Independent small, tick-by-tick oracle. No production queue or sorting. */
static void reference_tests(void) {
    uint32_t seed=7919;
    int sample, a, ok=1;
    for(sample=0;sample<100;++sample) {
        Process p[5]; int i;
        for(i=0;i<5;++i) {
            char pid[8]; int64_t arrival, burst;
            seed=seed*UINT32_C(1664525)+UINT32_C(1013904223); arrival=(seed>>8)%10;
            seed=seed*UINT32_C(1664525)+UINT32_C(1013904223); burst=1+(seed>>8)%6;
            snprintf(pid,sizeof(pid),"P%d",i); p[i]=process(pid,arrival,burst);
        }
        for(a=0;a<3;++a) {
            int remaining[5], first[5], end[5]={0}, queue[5], used=0, pending=-1;
            int now=0, current=-1, ticks=0, finished=0, q=1+sample%4;
            ScheduleResult r; size_t slice=0;
            for(i=0;i<5;++i) { remaining[i]=(int)p[i].burst; first[i]=-1; }
            if(!run(p,5,(Algorithm)a,q,&r)) { ok=0; continue; }
            while(finished<5 && now<100) {
                int chosen;
                for(i=0;i<5;++i) if(p[i].arrival==now && a==RR) queue[used++]=i;
                if(pending>=0) { queue[used++]=pending; pending=-1; }
                if(current<0) {
                    chosen=-1;
                    if(a==RR && used) { chosen=queue[0]; for(i=1;i<used;++i) queue[i-1]=queue[i]; --used; }
                    if(a!=RR) for(i=0;i<5;++i) {
                        if(!remaining[i] || p[i].arrival>now) continue;
                        if(chosen<0 || (a==SJF && p[i].burst<p[chosen].burst) ||
                           ((a==FCFS || p[i].burst==p[chosen].burst) && p[i].arrival<p[chosen].arrival)) chosen=i;
                    }
                    current=chosen; ticks=0;
                }
                while(slice<r.slices && r.timeline[slice].end<=now) ++slice;
                if(slice>=r.slices || r.timeline[slice].process!=(current<0 ? SIZE_MAX:(size_t)current)) ok=0;
                if(current>=0) {
                    if(first[current]<0) first[current]=now;
                    --remaining[current]; ++ticks;
                    if(!remaining[current]) { end[current]=now+1; ++finished; current=-1; }
                    else if(a==RR && ticks==q) { pending=current; current=-1; }
                }
                ++now;
            }
            if(now!=r.finish || !invariants(&r)) ok=0;
            for(i=0;i<5;++i) if(r.processes[i].first_run!=first[i] || r.processes[i].completion!=end[i]) ok=0;
            result_destroy(&r);
        }
    }
    CHECK("300 schedules match independent tick oracle",ok);
}
int main(void) {
    scheduling_tests(); queue_tests();
    csv_case("CSV valid CRLF and no final newline","pid,arrival,burst\r\nP1,0,2",1,"");
    csv_case("CSV duplicate PID","pid,arrival,burst\nA,0,2\nA,1,1",0,"Line 3: duplicate");
    csv_case("CSV negative arrival","pid,arrival,burst\nA,-1,2",0,"Line 2:");
    csv_case("CSV zero burst","pid,arrival,burst\nA,0,0",0,"Line 2:");
    csv_case("CSV empty","",0,"empty input");
    csv_case("CSV header only","pid,arrival,burst\n",0,"empty input");
    csv_case("CSV empty PID","pid,arrival,burst\n,0,2",0,"PID");
    csv_case("CSV extra field","pid,arrival,burst\nA,0,2,x",0,"Line 2:");
    csv_case("CSV missing field","pid,arrival,burst\nA,0",0,"Line 2:");
    csv_case("CSV overflow","pid,arrival,burst\nA,0,9223372036854775808",0,"Line 2:");
    csv_case("CSV malformed header","name,time,burst\nA,0,1",0,"header");
    csv_case("CSV blank record rejected","pid,arrival,burst\n\nA,0,1",0,"Line 2:");
    reference_tests();
    printf("%d tests passed; %d failed\n",tests-failures,failures);
    return failures ? 1:0;
}
