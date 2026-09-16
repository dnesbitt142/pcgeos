/* PM-BRINGUP [PM-TESTS] ADDED historical native fixture: requires the matching linked test inputs; not a guest boot test. */
/* Focused native 16-bit regression for the shipped TrueType ProcessFont branch.
 * Linux x86-64 only. Executes trusted linked code with synthetic LDT memory and
 * intercepts unrelocated GEOS calls. It is NOT a GEOS/DPMI/DOSBox boot test.
 */
#define _GNU_SOURCE
#include <asm/ldt.h>
#include <errno.h>
#include <fcntl.h>
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
#define HEAP_SEL 0x4cf
#define VARS_SEL 0x64f
#define STACK_SEL 0x0c7
#define FRAME_BP 0x144e
#define FRAME_SP 0x140a
#define FONT_BLOCK 0x1d70
static unsigned char *code,*data,*heap,*vars,*stack;
static unsigned derefs,expected_chunk;
static uint16_t word(const void *p){uint16_t v;memcpy(&v,p,2);return v;}
static void w16(void *p,uint16_t v){memcpy(p,&v,2);}
static void w32(void *p,uint32_t v){memcpy(p,&v,4);}
static void fatal(const char *s){perror(s);exit(121);}
static unsigned val(const char *s){char *e;unsigned long v=strtoul(s,&e,0);if(*e||v>0xffff)exit(122);return v;}
static unsigned char *region(void){void *p=mmap(NULL,0x10000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);if(p==MAP_FAILED)fatal("mmap");return p;}
static void segment(unsigned selector,unsigned contents,unsigned char *base){
 struct user_desc d={0}; d.entry_number=selector>>3; d.base_addr=(uint32_t)(uintptr_t)base;
 d.limit=0xffff;d.contents=contents;d.useable=1;
 if(syscall(SYS_modify_ldt,1,&d,sizeof d))fatal("modify_ldt");
}
static void load(const char *path,unsigned char *out){
 FILE *f=fopen(path,"rb");if(!f)fatal("open resource");size_t n=fread(out,1,65536,f);
 if(ferror(f)||!n||fgetc(f)!=EOF)exit(122);
 fclose(f);
}
static void terminate(const char *label,int status,greg_t *r){
 char msg[300];int n=snprintf(msg,sizeof msg,
  "%s CS:IP=%04x:%04x BX=%04x CPU-error=%04x derefs=%u AX=%04x DX=%04x\n",
  label,(unsigned)r[REG_CSGSFS]&0xffff,(unsigned)r[REG_RIP]&0xffff,
  (unsigned)r[REG_RBX]&0xffff,(unsigned)r[REG_ERR]&0xffff,derefs,
  (unsigned)r[REG_RAX]&0xffff,(unsigned)r[REG_RDX]&0xffff);
 (void)write(1,msg,n);_exit(status);
}
static void trap(int sig,siginfo_t *info,void *ctx){
 (void)sig;(void)info;greg_t *r=((ucontext_t*)ctx)->uc_mcontext.gregs;
 unsigned cs=(unsigned)r[REG_CSGSFS]&0xffff,ip=(unsigned)r[REG_RIP]&0xffff;
 unsigned sp=(unsigned)r[REG_RSP]&0xffff;
 if(cs==CODE_SEL&&ip<=0xfffb&&code[ip]==0x9a){
  unsigned target=word(code+ip+1),seg=word(code+ip+3);
  if(seg==0 && target==0x043d){ /* LMEMDEREF: two-word Pascal args, DX:AX result. */
   if(sp>0xfffb||word(stack+sp+2)!=FONT_BLOCK)terminate("BAD-LMEM-ARGS",125,r);
   unsigned ch=word(stack+sp);
   if(ch!=(derefs==0?0x10:expected_chunk)||derefs>1)terminate("WRONG-CHUNK",125,r);
   unsigned off=word(heap+ch);derefs++;
   char msg[180];int n=snprintf(msg,sizeof msg,"LMemDeref #%u block=%04x chunk=%04x -> %04x:%04x\n",derefs,FONT_BLOCK,ch,HEAP_SEL,off);
   (void)write(1,msg,n);
   r[REG_RAX]=off;r[REG_RDX]=HEAP_SEL;r[REG_RBX]=ch;
   r[REG_RSP]=(r[REG_RSP]&~(greg_t)0xffff)|((sp+4)&0xffff);r[REG_RIP]+=5;return;
  }
  if(seg==0&&target==0x043b){ /* Stop before the first new-outline allocation. */
   if(derefs!=2||word(stack+sp)!=0x2a||word(stack+sp+2)!=FONT_BLOCK)terminate("BAD-APPEND",125,r);
   terminate("APPEND-PATH",0,r);
  }
  if(seg==0x0b&&target==0x015e){ /* Stop at TT_Close_Face after duplicate match. */
   if(derefs!=2)terminate("BAD-DUPLICATE",125,r);
   terminate("DUPLICATE-PATH",0,r);
  }
 }
 if(cs==CODE_SEL&&ip==0x419&&code[ip]==0x8e&&code[ip+1]==0xc3&&((unsigned)r[REG_RBX]&0xffff)==0x10)
  terminate("REPRODUCED-ORIGINAL-FAULT",13,r);
 terminate("UNEXPECTED-FAULT",125,r);
}
int main(int argc,char **argv){
 if(argc<9){fprintf(stderr,"usage: runner CODE DATA original|fixed STYLE WEIGHT INDEX FONT_OFFSET CHUNK [STYLE-BITS:ADJUSTED-WEIGHT ...]\n");return 122;}
 unsigned original=!strcmp(argv[3],"original"),weight=val(argv[5]),index=val(argv[6]),offset=val(argv[7]);
 expected_chunk=val(argv[8]);unsigned count=(unsigned)argc-9;
 if(index>15||expected_chunk<0x20||expected_chunk>0x80||offset<0x1000||offset+0x23+count*26>=0xff00||strlen(argv[4])>=16)return 122;
 code=region();data=region();heap=region();vars=region();stack=region();
 load(argv[1],code);load(argv[2],data);
 memset(stack,0xcd,65536);
 w16(heap+0x10,0x31a);w16(heap+expected_chunk,offset);
 /* FontsAvailEntry is 40 bytes; FAE_infoHandle at +38. Poison preceding entries. */
 for(unsigned i=0;i<=index;i++)w16(heap+0x31a+i*40+38,0xeeee);
 w16(heap+0x31a+index*40+38,expected_chunk);
 w16(heap+offset+0x1f,0x23);w16(heap+offset+0x21,0x23+count*26);
 for(unsigned i=0;i<count;i++){
  char *colon=strchr(argv[9+i],':');if(!colon)return 122;*colon=0;
  unsigned s=val(argv[9+i]),w=val(colon+1);if(s>255||w>255)return 122;
  heap[offset+0x23+i*26]=(unsigned char)s;heap[offset+0x24+i*26]=(unsigned char)w;
 }
 w16(stack+FRAME_BP-(original?0x1e:0x32),index);
 strcpy((char*)stack+FRAME_BP-0x44,argv[4]);
 w16(stack+FRAME_BP+8,VARS_SEL);
 /* trueTypeVars at VARS:0; FACE_PROPERTIES.os2 -> VARS:0400. */
 w16(vars+0x3e,0x400);w16(vars+0x40,VARS_SEL);w16(vars+0x404,weight);
 segment(CODE_SEL,2,code);segment(DATA_SEL,0,data);segment(HEAP_SEL,0,heap);
 segment(VARS_SEL,0,vars);segment(STACK_SEL,0,stack);
 stack_t alt={.ss_sp=malloc(1<<20),.ss_size=1<<20};if(!alt.ss_sp||sigaltstack(&alt,0))fatal("sigaltstack");
 struct sigaction sa={.sa_sigaction=trap,.sa_flags=SA_SIGINFO|SA_ONSTACK};sigemptyset(&sa.sa_mask);
 if(sigaction(SIGSEGV,&sa,0)||sigaction(SIGILL,&sa,0)||sigaction(SIGBUS,&sa,0))fatal("sigaction");
 alarm(3);
 unsigned char *thunk=region(),*q=thunk,*ts=region();
 *q++=0xb8;w32(q,DATA_SEL);q+=4;*q++=0x8e;*q++=0xd8;
 *q++=0xb8;w32(q,HEAP_SEL);q+=4;*q++=0x8e;*q++=0xc0;
 *q++=0x2e;*q++=0x0f;*q++=0xb2;*q++=0x25;w32(q,(uint32_t)(uintptr_t)(thunk+128));q+=4;
 const unsigned reg[]={0,3,1,2,6,7,5};
 const uint32_t value[]={0x31a,0x10,CODE_SEL,HEAP_SEL,0,FONT_BLOCK,FRAME_BP};
 for(unsigned i=0;i<7;i++){*q++=0xb8+reg[i];w32(q,value[i]);q+=4;}
 *q++=0x66;*q++=0xea;w16(q,0x40b);q+=2;w16(q,CODE_SEL);
 w32(thunk+128,FRAME_SP);w16(thunk+132,STACK_SEL);
 uint64_t ip=(uintptr_t)thunk,sp=(uintptr_t)ts+65536-256;
 __asm__ volatile("mov %0,%%rsp; pushq $0x23; pushq %1; lretq"::"r"(sp),"r"(ip):"memory");
 __builtin_unreachable();
}
