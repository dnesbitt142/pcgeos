/* PM-BRINGUP [PM-TOOLS] ADDED: optional limited Linux x86-64 bridge for the trusted static i386 SDK tools; not a DOS/GEOS emulator. */
/* Local build-host bridge for static i386 Linux ELF executables.
 * Runs x86 instructions natively in compatibility mode, translating int 80
 * calls when this sandbox reports them as SIGSEGV. NOT a PC/DOS emulator.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
extern char **environ;
static uint32_t brk_start,brk_now,brk_max;static int trace;static char self[4096],target[4096];
#define P(x) ((void*)(uintptr_t)(uint32_t)(x))
#define R(n) ((uint32_t)r[REG_##n])
static long ret(long x){return x<0?-errno:x;}
struct st32{uint32_t dev,ino;uint16_t mode,nlink,uid,gid;uint32_t rdev,size,blksize,blocks,at,an,mt,mn,ct,cn,u1,u2;};
struct st64{uint64_t dev;uint32_t pad0,ino0,mode,nlink,uid,gid;uint64_t rdev;uint32_t pad1;int64_t size;uint32_t blksize;uint64_t blocks;uint32_t at,an,mt,mn,ct,cn;uint64_t ino;} __attribute__((packed));
static long statcall(int n,uint32_t a,uint32_t b){struct stat s;int e=(n==108||n==197)?fstat(a,&s):(n==107||n==196)?lstat(P(a),&s):stat(P(a),&s);if(e)return -errno;if(n<190){struct st32 t={s.st_dev,s.st_ino,s.st_mode,s.st_nlink,s.st_uid,s.st_gid,s.st_rdev,s.st_size,s.st_blksize,s.st_blocks,s.st_atim.tv_sec,s.st_atim.tv_nsec,s.st_mtim.tv_sec,s.st_mtim.tv_nsec,s.st_ctim.tv_sec,s.st_ctim.tv_nsec,0,0};memcpy(P(b),&t,sizeof t);}else{struct st64 t={s.st_dev,0,s.st_ino,s.st_mode,s.st_nlink,s.st_uid,s.st_gid,s.st_rdev,0,s.st_size,s.st_blksize,s.st_blocks,s.st_atim.tv_sec,s.st_atim.tv_nsec,s.st_mtim.tv_sec,s.st_mtim.tv_nsec,s.st_ctim.tv_sec,s.st_ctim.tv_nsec,s.st_ino};memcpy(P(b),&t,sizeof t);}return 0;}
static char **vec(uint32_t x){uint32_t*v=P(x);int n=0;if(v)while(v[n])n++;char**out=calloc(n+3,sizeof(char*));for(int i=0;i<n;i++)out[i]=P(v[i]);return out;}
static long doexec(uint32_t a,uint32_t b,uint32_t c){char**av=vec(b),**ev=vec(c);char*p=P(a);unsigned char h[5]={0};int fd=open(p,O_RDONLY);if(fd>=0){read(fd,h,5);close(fd);}if(!memcmp(h,"\177ELF\1",5)){int n=0;while(av[n])n++;char**nv=calloc(n+3,sizeof(char*));nv[0]=self;nv[1]=p;for(int i=1;i<n;i++)nv[i+1]=av[i];execve(self,nv,ev);free(nv);}else execve(p,av,ev);int e=errno;free(av);free(ev);return -e;}
struct dir64{uint64_t ino;int64_t off;uint16_t len;uint8_t type;char name[];};
static long dents(uint32_t fd,uint32_t out,uint32_t cap){char*tmp=malloc(cap);long got=syscall(SYS_getdents64,fd,tmp,cap),o=0;if(got<0){int e=errno;free(tmp);return -e;}for(long p=0;p<got;){struct dir64*d=(void*)(tmp+p);size_t n=strlen(d->name)+1;uint16_t len=(10+n+1+3)&~3;char*q=(char*)P(out)+o;memset(q,0,len);*(uint32_t*)q=d->ino;*(uint32_t*)(q+4)=d->off;*(uint16_t*)(q+8)=len;memcpy(q+10,d->name,n);q[len-1]=d->type;o+=len;p+=d->len;}free(tmp);return o;}
static long call32(greg_t*r){uint32_t n=R(RAX),a=R(RBX),b=R(RCX),c=R(RDX),d=R(RSI),e=R(RDI),f=R(RBP);long v=-ENOSYS;
switch(n){
case 1:case 252:_exit(a);
case 2:v=ret(fork());break;
case 3:v=ret(read(a,P(b),c));break;case 4:v=ret(write(a,P(b),c));break;
case 5:v=ret(open(P(a),b,c));break;case 6:v=ret(close(a));break;
case 7:v=ret(waitpid((int32_t)a,P(b),c));break;
case 8:v=ret(creat(P(a),b));break;case 9:v=ret(link(P(a),P(b)));break;case 10:v=ret(unlink(P(a)));break;
case 11:v=doexec(a,b,c);break;case 12:v=ret(chdir(P(a)));break;
case 13:v=time(0);if(a)*(uint32_t*)P(a)=v;break;
case 15:v=ret(chmod(P(a),b));break;case 19:v=ret(lseek(a,(int32_t)b,c));break;
case 20:v=getpid();break;case 24:case 199:v=getuid();break;
case 30:{uint32_t*t=P(b);struct timeval ts[2];if(t){ts[0]=(struct timeval){t[0],0};ts[1]=(struct timeval){t[1],0};}v=ret(utimes(P(a),t?ts:NULL));break;}
case 33:v=ret(access(P(a),b));break;case 37:v=ret(kill((int32_t)a,b));break;
case 38:v=ret(rename(P(a),P(b)));break;case 39:v=ret(mkdir(P(a),b));break;case 40:v=ret(rmdir(P(a)));break;
case 41:v=ret(dup(a));break;case 42:v=ret(pipe(P(a)));break;
case 43:{struct tms t;v=times(&t);if(a){int32_t*q=P(a);q[0]=t.tms_utime;q[1]=t.tms_stime;q[2]=t.tms_cutime;q[3]=t.tms_cstime;}break;}
case 45:if(!a)v=brk_now;else if(a>=brk_start&&a<brk_max){brk_now=a;v=a;}else v=brk_now;break;
case 47:case 200:v=getgid();break;case 49:case 201:v=geteuid();break;case 50:case 202:v=getegid();break;
case 54: v=ret(ioctl(a,b,P(c)));break;
case 55:case 221: if(b==F_GETFL||b==F_SETFL||b==F_GETFD||b==F_SETFD||b==F_DUPFD)v=ret(fcntl(a,b,c));break;
case 57:v=ret(setpgid(a,b));break;
case 60:v=umask(a);break;case 63:v=ret(dup2(a,b));break;
case 64:v=getppid();break;case 65:v=getpgrp();break;case 66:v=ret(setsid());break;
/* Build tools use these to arrange cleanup. Keep synchronous/default signals;
 * do not let an i386 handler replace this bridge's SIGSEGV handler. */
case 48:v=0;break;
case 67:if(c)memset(P(c),0,16);v=0;break;
case 174:if(c)memset(P(c),0,20);v=0;break;
case 126:if(c)*(uint32_t*)P(c)=0;v=0;break;
case 175:if(c)memset(P(c),0,8);v=0;break;
case 76:case 191:{struct rlimit l;if(getrlimit(a,&l))v=-errno;else{uint32_t*q=P(b);q[0]=l.rlim_cur>0xffffffff?0xffffffff:l.rlim_cur;q[1]=l.rlim_max>0xffffffff?0xffffffff:l.rlim_max;v=0;}break;}
case 77:{struct rusage u;if(getrusage((int32_t)a,&u))v=-errno;else{int32_t*q=P(b);q[0]=u.ru_utime.tv_sec;q[1]=u.ru_utime.tv_usec;q[2]=u.ru_stime.tv_sec;q[3]=u.ru_stime.tv_usec;long*p=&u.ru_maxrss;for(int i=0;i<14;i++)q[i+4]=p[i];v=0;}break;}
case 78:{struct timeval tv;v=ret(gettimeofday(&tv,NULL));if(a){int32_t*q=P(a);q[0]=tv.tv_sec;q[1]=tv.tv_usec;}if(b)memset(P(b),0,8);break;}
case 83:v=ret(symlink(P(a),P(b)));break;case 85:if(!strcmp(P(a),"/proc/self/exe")){size_t z=strlen(target);if(z>c)z=c;memcpy(P(b),target,z);v=z;}else v=ret(readlink(P(a),P(b),c));break;
case 90:{uint32_t*q=P(a);void*m=mmap(P(q[0]),q[1],q[2],q[3]|((q[3]&MAP_FIXED)?0:MAP_32BIT),(int32_t)q[4],q[5]);v=m==MAP_FAILED?-errno:(uintptr_t)m;break;}
case 91:v=ret(munmap(P(a),b));break;
case 92:v=ret(truncate(P(a),b));break;case 93:v=ret(ftruncate(a,b));break;
case 94:v=ret(fchmod(a,b));break;
case 106:case 107:case 108:case 195:case 196:case 197:v=statcall(n,a,b);break;
case 114:v=ret(waitpid((int32_t)a,P(b),c));break;
case 122:{struct utsname u;if(uname(&u))v=-errno;else{strcpy(u.machine,"i686");memcpy(P(a),&u,390);v=0;}break;}
case 125:v=ret(mprotect(P(a),b,c));break;
case 132:v=ret(getpgid(a));break;
case 140:{int64_t off=((int64_t)(int32_t)b<<32)|c;off_t o=lseek(a,off,e);if(o<0)v=-errno;else{*(int64_t*)P(d)=o;v=0;}break;}
case 141:v=dents(a,b,c);break;
case 142:{fd_set sets[3];uint32_t args[3]={b,c,d};for(int j=0;j<3;j++){FD_ZERO(&sets[j]);if(args[j]){uint32_t*q=P(args[j]);for(unsigned i=0;i<a&&i<FD_SETSIZE;i++)if(q[i/32]&(1u<<(i%32)))FD_SET(i,&sets[j]);}}struct timeval tv,*tp=NULL;if(e){int32_t*q=P(e);tv=(struct timeval){q[0],q[1]};tp=&tv;}v=ret(select(a,b?sets:NULL,c?sets+1:NULL,d?sets+2:NULL,tp));if(v>=0){for(int j=0;j<3;j++)if(args[j]){uint32_t*q=P(args[j]);memset(q,0,((a+31)/32)*4);for(unsigned i=0;i<a&&i<FD_SETSIZE;i++)if(FD_ISSET(i,&sets[j]))q[i/32]|=1u<<(i%32);}if(e){int32_t*q=P(e);q[0]=tv.tv_sec;q[1]=tv.tv_usec;}}break;}

case 162:{int32_t*q=P(a);struct timespec t={q[0],q[1]},rem;v=ret(nanosleep(&t,&rem));if(b){q=P(b);q[0]=rem.tv_sec;q[1]=rem.tv_nsec;}break;}
case 183:{char*p=getcwd(P(a),b);v=p?strlen(p)+1:-errno;break;}
case 190:v=ret(fork());break;
case 192:{void*m=mmap(P(a),b,c,d|((d&MAP_FIXED)?0:MAP_32BIT),(int32_t)e,(off_t)f*4096);v=m==MAP_FAILED?-errno:(uintptr_t)m;break;}
case 224:v=syscall(SYS_gettid);break;
case 265:case 266:{struct timespec t;v=ret(n==265?clock_gettime(a,&t):clock_getres(a,&t));if(v==0&&b){int32_t*q=P(b);q[0]=t.tv_sec;q[1]=t.tv_nsec;}break;}

default:fprintf(stderr,"elf32run: unsupported syscall %u at %08x\n",n,R(RIP));break;
}if(trace)fprintf(stderr,"[%d] int80 %u(%x,%x,%x) -> %lx\n",getpid(),n,a,b,c,v);return v;}
static void trap(int s,siginfo_t*i,void*v){ucontext_t*c=v;greg_t*r=c->uc_mcontext.gregs;uint8_t*p=(void*)(uintptr_t)r[REG_RIP];if((r[REG_CSGSFS]&0xffff)==0x23&&p[0]==0xcd&&p[1]==0x80){long x=call32(r);r[REG_RAX]=(uint32_t)x;r[REG_RIP]+=2;return;}fprintf(stderr,"elf32run: signal %d code %d at %llx addr=%p eax=%llx ebx=%llx ecx=%llx edx=%llx esp=%llx cs=%llx\n",s,i->si_code,(unsigned long long)r[REG_RIP],i->si_addr,(unsigned long long)r[REG_RAX],(unsigned long long)r[REG_RBX],(unsigned long long)r[REG_RCX],(unsigned long long)r[REG_RDX],(unsigned long long)r[REG_RSP],(unsigned long long)r[REG_CSGSFS]);_exit(128+s);}
int main(int argc,char**argv){if(argc<2){fprintf(stderr,"usage: elf32run static-i386-ELF [arguments]\n");return 2;}ssize_t sl=readlink("/proc/self/exe",self,sizeof self-1);if(sl<0)return 2;self[sl]=0;trace=getenv("ELF32_TRACE")!=NULL;if(!realpath(argv[1],target)){perror(argv[1]);return 2;}int fd=open(argv[1],O_RDONLY);if(fd<0){perror(argv[1]);return 2;}Elf32_Ehdr eh;if(read(fd,&eh,sizeof eh)!=sizeof eh||memcmp(eh.e_ident,"\177ELF\1",5)||eh.e_type!=ET_EXEC||eh.e_machine!=EM_386){fprintf(stderr,"not a static i386 ELF executable: %s\n",argv[1]);return 2;}Elf32_Phdr*ph=malloc(eh.e_phnum*sizeof*ph);pread(fd,ph,eh.e_phnum*sizeof*ph,eh.e_phoff);uint32_t end=0;for(int j=0;j<eh.e_phnum;j++){if(ph[j].p_type==PT_INTERP){fprintf(stderr,"dynamic ELF unsupported\n");return 2;}if(ph[j].p_type!=PT_LOAD)continue;uint32_t lo=ph[j].p_vaddr&~4095u,hi=(ph[j].p_vaddr+ph[j].p_memsz+4095)&~4095u;if(lo<0x800000||hi>=0x40000000){fprintf(stderr,"ELF mapping outside supported range\n");return 2;}void*m=mmap(P(lo),hi-lo,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(m==MAP_FAILED){perror("map ELF");return 2;}if(pread(fd,P(ph[j].p_vaddr),ph[j].p_filesz,ph[j].p_offset)!=ph[j].p_filesz){perror("read ELF");return 2;}if(hi>end)end=hi;}close(fd);brk_start=brk_now=end;brk_max=end+0x18000000;if(mmap(P(end),brk_max-end,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE|MAP_NORESERVE,-1,0)==MAP_FAILED){perror("heap");return 2;}
uint32_t bottom=0xb0000000,top=0xb2000000;if(mmap(P(bottom),top-bottom,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==MAP_FAILED){perror("stack");return 2;}uint32_t sp=top-4096;int ac=argc-1,ec=0;if(strlen(argv[1])>6&&!strcmp(argv[1]+strlen(argv[1])-6,".elf32")){argv[1]=strdup(argv[1]);argv[1][strlen(argv[1])-6]=0;}while(environ[ec])ec++;uint32_t*ap=calloc(ac+1,4),*ep=calloc(ec+1,4);for(int j=ec-1;j>=0;j--){size_t n=strlen(environ[j])+1;sp-=n;memcpy(P(sp),environ[j],n);ep[j]=sp;}for(int j=ac-1;j>=0;j--){size_t n=strlen(argv[j+1])+1;sp-=n;memcpy(P(sp),argv[j+1],n);ap[j]=sp;}sp&=~15u;uint32_t aux[]={AT_PAGESZ,4096,AT_CLKTCK,100,AT_UID,getuid(),AT_EUID,geteuid(),AT_GID,getgid(),AT_EGID,getegid(),AT_NULL,0};sp-=sizeof aux;memcpy(P(sp),aux,sizeof aux);sp-=(ec+1)*4;memcpy(P(sp),ep,(ec+1)*4);sp-=(ac+1)*4;memcpy(P(sp),ap,(ac+1)*4);sp-=4;*(uint32_t*)P(sp)=ac;
stack_t ss={.ss_sp=malloc(4<<20),.ss_size=4<<20};sigaltstack(&ss,0);struct sigaction sa={.sa_sigaction=trap,.sa_flags=SA_SIGINFO|SA_ONSTACK};sigemptyset(&sa.sa_mask);sigaction(SIGSEGV,&sa,0);sigaction(SIGILL,&sa,0);sigaction(SIGSYS,&sa,0);sigaction(SIGBUS,&sa,0);
uint64_t entry=eh.e_entry,stk=sp;asm volatile("mov %0,%%rsp; pushq $0x23; pushq %1; mov $0x2b,%%eax; mov %%ax,%%ds; mov %%ax,%%es; xor %%eax,%%eax; xor %%ebx,%%ebx; xor %%ecx,%%ecx; xor %%edx,%%edx; xor %%esi,%%esi; xor %%edi,%%edi; xor %%ebp,%%ebp; lretq"::"r"(stk),"r"(entry):"memory");__builtin_unreachable();}
