/* PM-BRINGUP [PM-TESTS] ADDED historical native fixture: requires the matching linked test inputs; not a guest boot test. */
/* Execute the actual linked PerfCalcNewStats resource on native x86-64 Linux.
 * 16-bit LDT segments provide isolated test memory. Unrelocated GEOS far calls
 * are intercepted and given controlled SysStats. This is NOT a GEOS/DOS boot.
 * Only trusted binaries from this package's build should be given to this test.
 */
#define _GNU_SOURCE
#include <asm/ldt.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>
#define CODE_SEL 0x637
#define DATA_SEL 0x60f
#define STACK_SEL 0x0c7
#define STOP_IP 0xfff0
#define START_SP 0xf000
#define RAW_STATS 0x16f
#define NUMERIC 0xfd
#define GRAPH 0x181
#define DIVISOR 0x16a
#define PURE 0x16c
#define HANDLES_DIVISOR 0x754
static unsigned char *code,*data,*stack;
static unsigned current_sample,total_samples,stats_calls,info_calls,original,poison;
struct sample { uint32_t idle; uint16_t interrupts,switches,runqueue; };
static struct sample samples[128];
static uint16_t word(const void *p){uint16_t v;memcpy(&v,p,2);return v;}
static void w16(void *p,uint16_t v){memcpy(p,&v,2);}
static void w32(void *p,uint32_t v){memcpy(p,&v,4);}
static void fatal(const char *s){perror(s);exit(121);}
static unsigned char *region(void){
 void *p=mmap(NULL,65536,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 if(p==MAP_FAILED)fatal("mmap");
 return p;
}
static void segment(unsigned selector,unsigned contents,unsigned char *base){
 struct user_desc d={0};d.entry_number=selector>>3;d.base_addr=(uint32_t)(uintptr_t)base;
 d.limit=0xffff;d.contents=contents;d.useable=1;
 if(syscall(SYS_modify_ldt,1,&d,sizeof d))fatal("modify_ldt");
}
static void load(const char *path,unsigned char *out){
 FILE *f=fopen(path,"rb");if(!f)fatal("open resource");size_t n=fread(out,1,65536,f);
 if(ferror(f)||!n||fgetc(f)!=EOF)exit(122);
 fclose(f);
}
static void finish(const char *label,int status,greg_t *r){
 char msg[320];int n=snprintf(msg,sizeof msg,
 "%s CS:IP=%04x:%04x AX=%04x DX=%04x SI=%04x trap=%u divisor=%u statsCalls=%u infoCalls=%u completed=%u\n",
 label,(unsigned)r[REG_CSGSFS]&0xffff,(unsigned)r[REG_RIP]&0xffff,
 (unsigned)r[REG_RAX]&0xffff,(unsigned)r[REG_RDX]&0xffff,(unsigned)r[REG_RSI]&0xffff,
 (unsigned)r[REG_TRAPNO],word(data+HANDLES_DIVISOR),stats_calls,info_calls,current_sample);
 (void)write(1,msg,n);_exit(status);
}
static void trap(int sig,siginfo_t *info,void *ctx){
 (void)sig;(void)info;greg_t *r=((ucontext_t*)ctx)->uc_mcontext.gregs;
 unsigned cs=(unsigned)r[REG_CSGSFS]&0xffff,ip=(unsigned)r[REG_RIP]&0xffff;
 if(cs==CODE_SEL&&ip<=0xfffb&&code[ip]==0x9a){
  unsigned target=word(code+ip+1),sel=word(code+ip+3);
  if(sel==0&&target==0x226){ /* SysStatistics; ES:DI points at an 18-byte buffer. */
   if(current_sample>=total_samples||((unsigned)r[REG_RDI]&0xffff)!=RAW_STATS)
    finish("BAD-STATISTICS-ARGS",125,r);
   const struct sample *s=&samples[current_sample];
   memset(data+RAW_STATS,0,18);w32(data+RAW_STATS,s->idle);
   w16(data+RAW_STATS+12,s->switches);w16(data+RAW_STATS+14,s->interrupts);
   w16(data+RAW_STATS+16,s->runqueue);
   stats_calls++;r[REG_RIP]+=5;return;
  }
  if(sel==0&&target==0x236&&((unsigned)r[REG_RAX]&0xffff)==0x800e){
   /* SGIT_NUMBER_OF_FREE_HANDLES; no divisor initialization is performed. */
   info_calls++;r[REG_RAX]=500;r[REG_RDX]=0;r[REG_RIP]+=5;return;
  }
  finish("UNEXPECTED-GEOS-CALL",125,r);
 }
 if(cs==CODE_SEL&&ip==STOP_IP){
  if(original)finish("ORIGINAL-UNEXPECTEDLY-RETURNED",125,r);
  if(info_calls||stats_calls!=current_sample+1)finish("BAD-CALL-COUNTS",125,r);
  for(unsigned j=4;j<14;j++)
   if(word(data+NUMERIC+2*j)!=0xffff||word(data+GRAPH+2*j)!=0)
    finish("UNSUPPORTED-METER-CHANGED",125,r);
  if(word(data+HANDLES_DIVISOR))finish("DIVISOR-MODIFIED",125,r);
  if(poison&&(word(data+0x732)!=0x10||word(data+0x119)!=1||word(data+0xa8)!=1))
   finish("POISON-MODIFIED",125,r);
  char msg[320];int n=snprintf(msg,sizeof msg,
   "SAMPLE %u numeric=%u,%u,%u,%u graph=%u,%u,%u,%u loadState=%u\n",current_sample,
   word(data+NUMERIC),word(data+NUMERIC+2),word(data+NUMERIC+4),word(data+NUMERIC+6),
   word(data+GRAPH),word(data+GRAPH+2),word(data+GRAPH+4),word(data+GRAPH+6),word(data+0x11f));
  (void)write(1,msg,n);current_sample++;
  if(current_sample==total_samples)finish("SAMPLING-PASSED",0,r);
  w16(stack+START_SP,STOP_IP);w16(stack+START_SP+2,CODE_SEL);
  r[REG_RIP]=0;r[REG_RSP]=START_SP;return;
 }
 if(original&&cs==CODE_SEL&&ip==0x2e1&&code[ip]==0xf7&&code[ip+1]==0xf6&&
    ((unsigned)r[REG_RSI]&0xffff)==0&&r[REG_TRAPNO]==0&&stats_calls==1&&info_calls==1)
  finish("REPRODUCED-UNINITIALIZED-HANDLE-DIVISOR",13,r);
 finish("UNEXPECTED-FAULT",125,r);
}
static unsigned number(const char *s,unsigned max){
 char *end;errno=0;unsigned long v=strtoul(s,&end,0);
 if(errno||*end||end==s||v>max){fprintf(stderr,"Bad integer: %s\n",s);exit(122);}return (unsigned)v;
}
int main(int argc,char **argv){
 if(argc<8||argc>134){fprintf(stderr,"Usage: runner CODE DATA original|fixed DIVISOR PURE POISON IDLE:INTERRUPTS:SWITCHES:QUEUE ...\n");return 122;}
 if(strcmp(argv[3],"original")&&strcmp(argv[3],"fixed"))return 122;
 original=!strcmp(argv[3],"original");unsigned div=number(argv[4],65535),pure=number(argv[5],1);
 poison=number(argv[6],1);total_samples=(unsigned)argc-7;
 for(unsigned i=0;i<total_samples;i++){
  unsigned long long idle;unsigned intr,sw,queue;char trailing;
  if(sscanf(argv[i+7],"%llu:%u:%u:%u%c",&idle,&intr,&sw,&queue,&trailing)!=4||
     idle>UINT32_MAX||intr>65535||sw>65535||queue>255)return 122;
  samples[i]=(struct sample){(uint32_t)idle,(uint16_t)intr,(uint16_t)sw,(uint16_t)queue};
 }
 code=region();data=region();stack=region();load(argv[1],code);load(argv[2],data);
 w16(data+DIVISOR,div);data[PURE]=(unsigned char)pure;
 if(word(data+HANDLES_DIVISOR)!=0)return 122;
 if(poison){w16(data+0x732,0x10);w16(data+0x119,1);w16(data+0xa8,1);}
 code[STOP_IP]=0x0f;code[STOP_IP+1]=0x0b; /* UD2: report normal far return. */
 w16(stack+START_SP,STOP_IP);w16(stack+START_SP+2,CODE_SEL);
 segment(CODE_SEL,2,code);segment(DATA_SEL,0,data);segment(STACK_SEL,0,stack);
 stack_t alt={.ss_sp=malloc(1<<20),.ss_size=1<<20};if(!alt.ss_sp||sigaltstack(&alt,0))fatal("sigaltstack");
 struct sigaction sa={.sa_sigaction=trap,.sa_flags=SA_SIGINFO|SA_ONSTACK};sigemptyset(&sa.sa_mask);
 if(sigaction(SIGSEGV,&sa,0)||sigaction(SIGILL,&sa,0)||sigaction(SIGBUS,&sa,0)||sigaction(SIGFPE,&sa,0))fatal("sigaction");
 alarm(5);
 unsigned char *thunk=region(),*q=thunk,*ts=region();
 *q++=0xb8;w32(q,DATA_SEL);q+=4;*q++=0x8e;*q++=0xd8;*q++=0x8e;*q++=0xc0;
 *q++=0x2e;*q++=0x0f;*q++=0xb2;*q++=0x25;w32(q,(uint32_t)(uintptr_t)(thunk+128));q+=4;
 *q++=0x66;*q++=0xea;w16(q,0);q+=2;w16(q,CODE_SEL);
 w32(thunk+128,START_SP);w16(thunk+132,STACK_SEL);
 uint64_t ip=(uintptr_t)thunk,sp=(uintptr_t)ts+65536-256;
 __asm__ volatile("mov %0,%%rsp; pushq $0x23; pushq %1; lretq"::"r"(sp),"r"(ip):"memory");
 __builtin_unreachable();
}
