/* PM-BRINGUP [PM-TESTS] ADDED historical native fixture: requires the matching linked test inputs; not a guest boot test. */
/* Focused x86-64 Linux test runner for the compiled Loader32 fatal-error path.
 * Executes 16-bit instructions natively with test LDT segments. DOS output/exit
 * are intercepted by a signal handler. This is NOT a DOSBox/DPMI/GEOS boot test.
 * Run only a trusted loader built from the accompanying source.
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
#include <sys/stat.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

static unsigned char *image;
static void fatal(const char *s) { perror(s); exit(121); }
static uint16_t word(const void *p) { uint16_t x; memcpy(&x,p,2); return x; }
static void putword(void *p,uint16_t x) { memcpy(p,&x,2); }
static void putdword(void *p,uint32_t x) { memcpy(p,&x,4); }
static unsigned num(const char *s) { char *e; unsigned long x=strtoul(s,&e,0); if(*e||x>UINT32_MAX)exit(122);return (unsigned)x; }
static void trap(int sig,siginfo_t *info,void *context) {
    (void)sig; (void)info;
    ucontext_t *uc=context; greg_t *r=uc->uc_mcontext.gregs;
    unsigned cs=(unsigned)r[REG_CSGSFS]&0xffff;
    unsigned ip=(unsigned)r[REG_RIP];
    if(cs==7 && ip<0xfffe && image[ip]==0xcd && image[ip+1]==0x21) {
        unsigned ax=(unsigned)r[REG_RAX]&0xffff;
        unsigned dx=(unsigned)r[REG_RDX]&0xffff;
        switch(ax>>8) {
            case 9: {
                unsigned end=dx;
                while(end<0x10000 && image[end]!='$')end++;
                if(end==0x10000)_exit(123);
                (void)write(1,image+dx,end-dx);
                r[REG_RAX]=(r[REG_RAX]&~(greg_t)0xff)|'$';
                break;
            }
            case 2: {
                unsigned char ch=dx;
                (void)write(1,&ch,1);
                r[REG_RAX]=(r[REG_RAX]&~(greg_t)0xff)|ch;
                break;
            }
            case 0x4c: _exit(ax&0xff);
            default: _exit(124);
        }
        r[REG_RIP]+=2;
        return;
    }
    char msg[250];
    int n=snprintf(msg,sizeof msg,"HARNESS FAULT: sig=%d code=%d CS:IP=%04x:%08x addr=%p SP=%llx AX=%llx\n",
                   sig,info->si_code,cs,ip,info->si_addr,(unsigned long long)r[REG_RSP],(unsigned long long)r[REG_RAX]);
    (void)write(2,msg,n>0?(size_t)n:0);_exit(125);
}
static void segment_at(unsigned entry,unsigned contents,unsigned char *base,unsigned limit,
                       unsigned absent,unsigned only,unsigned pages) {
    struct user_desc d={0};
    d.entry_number=entry; d.base_addr=(uint32_t)(uintptr_t)base;
    d.limit=limit;d.seg_32bit=0;d.contents=contents;d.read_exec_only=only;
    d.limit_in_pages=pages;d.seg_not_present=absent;d.useable=1;
    if(syscall(SYS_modify_ldt,1,&d,sizeof d))fatal("modify_ldt");
}
static void segment(unsigned entry,unsigned contents) {
    segment_at(entry,contents,image,0xffff,0,0,0);
}
static unsigned char *region(void) {
    unsigned char *m=mmap(NULL,0x10000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(m==MAP_FAILED)fatal("mmap region");
    return m;
}
/* Controlled memory/descriptor fixtures. They are NOT captured from GEOS. */
static void snapshot_fixture(unsigned vars,const char *which) {
    unsigned char *code=region(),*stack=region(),*heap=region(),*core=region();
    unsigned ip=0x419,sp=0x800;
    unsigned code_limit=0xffff,heap_limit=0xffff,core_limit=0xffff,pages=0;
    if(!strcmp(which,"last-byte")){ip=0xffff;sp=0xffff;}
    if(!strcmp(which,"large-limit")){code_limit=0xfffff;pages=1;}
    if(!strcmp(which,"short-code"))code_limit=0x420;
    if(!strcmp(which,"out-of-limit"))code_limit=0x418;
    if(!strcmp(which,"short-heap"))heap_limit=0x21e;
    if(!strcmp(which,"short-core"))core_limit=0x30;
    if(!strcmp(which,"bad-resource-span"))core_limit=0x102;
    segment_at(198,!strcmp(which,"expand-down")?1:2,code,code_limit,
               !strcmp(which,"absent-code"),!strcmp(which,"execute-only"),pages);
    segment_at(199,0,stack,0xffff,0,0,0);
    segment_at(200,0,heap,heap_limit,0,0,0);
    segment_at(201,0,core,core_limit,0,0,0);
    for(unsigned i=0;i<16&&ip+i<=0xffff;i++)code[ip+i]=(unsigned char)(0xa0+i);
    for(unsigned i=0;i<16&&sp+i*2+1<=0xffff;i++)putword(stack+sp+i*2,0xb000+i);
    // Real offsets from this build's KernelLoaderVars and GeodeHeader symbols.
    putword(image+vars+448,0x647);
    putword(image+vars+421,0x200);putword(image+vars+409,0x220);
    putword(heap+0x200,0x637);putword(heap+0x202,0x210);
    putword(heap+0x210,0x64f);putword(heap+0x212,0x210);
    putword(core,0x210);memcpy(core+20,"testapp APP ",12);
    putword(core+56,3);putword(core+58,0x100);
    putword(core+0x100,0x210);putword(core+0x102,0);putword(core+0x104,0x200);
    if(!strcmp(which,"bad-owner"))putword(heap+0x202,0x211);
    if(!strcmp(which,"bad-core-owner"))putword(core,0x220);
    if(!strcmp(which,"bad-bounds"))putword(image+vars+409,0x210);
    if(!strcmp(which,"misaligned-table"))putword(image+vars+421,0x201);
    if(!strcmp(which,"empty-table"))putword(image+vars+409,0x200);
    if(!strcmp(which,"no-owner"))putword(heap+0x200,0x677);
    if(!strcmp(which,"sanitize-name")){core[20]=1;core[21]='$';core[22]=255;}
    putword(image+0xee06,ip);putword(image+0xee08,!strcmp(which,"null-code")?0:0x637);
    putword(image+0xee0c,sp);putword(image+0xee0e,!strcmp(which,"null-stack")?0:0x63f);
}
int main(int argc,char **argv) {
    if(argc!=7 && argc!=12 && argc!=13 && argc!=15){
        fprintf(stderr,"usage: test-loader-error EXE ENTRY DS_VAR ERROR STAGE_VAR STAGE [VALID_VAR FAULT_VAR FAULTERROR_VAR FAULT_CSIP FAULT_ERROR [gpf [LOADER_VARS FIXTURE]]]\n");return 122;
    }
    unsigned entry=num(argv[2]), dsvar=num(argv[3]), error=num(argv[4]), stagevar=num(argv[5]), stage=num(argv[6]);
    int fd=open(argv[1],O_RDONLY);if(fd<0)fatal("open loader");
    struct stat st;if(fstat(fd,&st))fatal("stat loader");
    unsigned char *file=malloc(st.st_size);
    if(read(fd,file,st.st_size)!=st.st_size)fatal("read loader");
    close(fd);
    if(st.st_size<28||word(file)!=0x5a4d)return 122;
    unsigned headers=word(file+8)*16;
    if(headers>(unsigned)st.st_size||st.st_size-headers>0x10000||entry>0xffff||dsvar>0xfffe)return 122;
    image=mmap(NULL,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(image==MAP_FAILED)fatal("mmap image");
    memcpy(image,file+headers,st.st_size-headers);
    /* kcode is the zero-based MZ code group. Only that group is executed
     * here; relocate its segment references to our test code selector. */
    unsigned reloc_start=word(file+24),reloc_count=word(file+6);
    if(reloc_start+reloc_count*4>headers)return 122;
    for(unsigned i=0;i<reloc_count;i++) {
        unsigned char *reloc=file+reloc_start+i*4;
        unsigned at=word(reloc)+16*word(reloc+2);
        if(at>0xfffe)return 122;
        if(word(image+at)==0)putword(image+at,7);
    }
    free(file);
    int noalias=argc>=13 && strstr(argv[12],"noalias")!=NULL;
    int gpf=argc>=13 && strncmp(argv[12],"gpf",3)==0;
    putword(image+dsvar,noalias?0:15);
    if(stagevar)putword(image+stagevar,stage);
    if(argc>=12) {
        unsigned valid=num(argv[7]),fault=num(argv[8]),ferror=num(argv[9]);
        if(valid>0xffff||fault>0xfffc||ferror>0xfffe)return 122;
        image[valid]=noalias?0:1;putdword(image+fault,num(argv[10]));putword(image+ferror,num(argv[11]));
        if(gpf) {
            // Simulated 16-bit DPMI exception frame. The handler is entered
            // directly, NOT via a real DPMI exception delivery.
            if(strcmp(argv[12],"gpf-reentry")) {
                image[valid]=0;putdword(image+fault,0);putword(image+ferror,0);
            }
            putword(image+0xee04,num(argv[11]));
            putdword(image+0xee06,num(argv[10]));
            putword(image+0xee0a,0x202);
            putword(image+0xee0c,0xfd00);
            putword(image+0xee0e,15);
        }
    }
    segment(0,2);segment(1,0);
    if(argc==15) snapshot_fixture(num(argv[13]),argv[14]);
    stack_t alt={.ss_sp=malloc(1<<20),.ss_size=1<<20};if(sigaltstack(&alt,0))fatal("sigaltstack");
    struct sigaction sa={.sa_sigaction=trap,.sa_flags=SA_SIGINFO|SA_ONSTACK};sigemptyset(&sa.sa_mask);
    if(sigaction(SIGSEGV,&sa,0)||sigaction(SIGILL,&sa,0)||sigaction(SIGBUS,&sa,0))fatal("sigaction");
    unsigned char *thunk=mmap(NULL,4096,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    void *stack=mmap(NULL,1<<20,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(thunk==MAP_FAILED||stack==MAP_FAILED)fatal("mmap thunk/stack");
    unsigned char *q=thunk;
    /* Flat 32-bit transition code: install 16-bit SS, then seed every GPR. */
    *q++=0xb8;putdword(q,15);q+=4;
    *q++=0x8e;*q++=0xd8;*q++=0x8e;*q++=0xc0;
    *q++=0x2e;*q++=0x0f;*q++=0xb2;*q++=0x25;
    putdword(q,(uint32_t)(uintptr_t)(thunk+128));q+=4;
    const unsigned regs[]={0,3,1,2,6,7,5};
    const uint32_t values[]={error,0x11223344,0x55667788,0x99aabbcc,0x01234567,0x89abcdef,0x76543210};
    for(unsigned i=0;i<7;i++){*q++=(unsigned char)(0xb8+regs[i]);putdword(q,values[i]);q+=4;}
    *q++=0x66;*q++=0xea;putword(q,entry);q+=2;putword(q,7);q+=2;
    putdword(thunk+128,gpf?0xee00:0xff00);putword(thunk+132,15);
    uint64_t ip=(uintptr_t)thunk,sp=(uintptr_t)stack+(1<<20)-256;
    __asm__ volatile("mov %0,%%rsp; pushq $0x23; pushq %1; lretq"::"r"(sp),"r"(ip):"memory");
    __builtin_unreachable();
}
