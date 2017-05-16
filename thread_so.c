#include<stdio.h>
#include <stdlib.h>
#include  <string.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <dlfcn.h>


#define _RET_IP_        (unsigned long)__builtin_return_address(0)
#define _THIS_IP_  ({ __label__ __here; __here: (unsigned long)&&__here; })

#define MYTIMER ITIMER_PROF



struct timeval sched_timer = { 1 , 100000 } ;

void start_timer(void) {
    struct itimerval timer;
    timer.it_interval = sched_timer; //copy the timeval for the next timer to be triggered
    timer.it_value    = sched_timer; //this value would be reset to the next value on a timer expiry
    if(setitimer(MYTIMER,&timer,NULL) < 0 ) {
        printf("Error in setting up the timer.Exiting...");
    }
    return ;
}

void stop_timer(void) {
    struct timeval tv = { 0, 0 }; //zero up the timeval struct
    struct itimerval timer = { tv, tv } ; //load up the itimer with a zero timer
    if(setitimer(MYTIMER,&timer,NULL) < 0) {
        printf("Error in Setting up the Timer:\n");
        exit(1);
    }
}
void setup_signal(int signum,void (*handler)(int) ) {
    struct sigaction sigact ;
    sigset_t set; //the signal set
    sigact.sa_handler = handler; //sigprof handler
    sigfillset(&set); //by default block all signals
    sigdelset(&set,signum); //dont block signum.Allow signum only
    sigdelset(&set,SIGKILL); //dont block signum.Allow signum only
    sigdelset(&set,SIGQUIT); //dont block signum.Allow signum only
    sigdelset(&set,SIGTERM); //dont block signum.Allow signum only
    sigact.sa_mask = set; //copy the signal mask
    sigact.sa_flags =  SA_NOMASK; //amounts to a non maskable interrupt
    if( sigaction(signum,&sigact,NULL) < 0) {
        fprintf(stderr,"Failed to initialise the Signal (%d)\n",signum);
        exit(1);
    }
    return ;
}

typedef struct __jmp_buf {
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int eip;
        unsigned int ecx;
}jmp_buf;

typedef struct frame{
    unsigned int oldeip;
    unsigned int oldebp;
    unsigned int newebp;
}frame;

typedef struct THREAD{
        struct THREAD *next;
    jmp_buf ctx;
    void (*fun)(void *);
    int *stack;
    char name[20];
        void *arg;
}THREAD;
THREAD *current=NULL;

int save_context(THREAD *t)
{
        register int eax asm("eax");
        register int ebx asm("ebx");
        register int ecx asm("ecx");
        register int esi asm("esi");
        register int edi asm("edi");
        register int ebp asm("ebp");
        register int esp asm("esp");

        ecx = (unsigned long)&&__restore__;
        eax = 0;
        t->ctx.ebx = ebx;
        t->ctx.esi = esi;
        t->ctx.edi = edi;
        t->ctx.ebp = ebp;
        t->ctx.esp = esp;
        t->ctx.eip = ecx;
        /*asm("pushal;");*/

__restore__:
        /*asm("popal;");*/
        return eax;
}
int restore_context(THREAD *t, int value)
{
        register int eax asm("eax");
        register int ebx asm("ebx");
        register int ecx asm("ecx");
        register int esi asm("esi");
        register int edi asm("edi");
        register int ebp asm("ebp");
        register int esp asm("esp");

        eax =   value;
        ecx =   t->ctx.eip;
        ebx =   t->ctx.ebx;
        esi =   t->ctx.esi;
        edi =   t->ctx.edi;
        esp =   t->ctx.esp;
        ebp =   t->ctx.ebp;
        asm("jmp *%ecx;");
}




THREAD* create_thread(char *name, void (*fun)(THREAD *, void*) , void *arg ){
    THREAD *t= (THREAD*)malloc(sizeof(THREAD));
    assert(t!=NULL);
    strcpy(t->name, name);
        t->arg = arg;
    save_context(t);
    t->stack = (unsigned *) ((char *)malloc(4096) + 4096);
    assert(t->stack != NULL);
        t->stack[-1] = (int)arg;
        t->stack[-2] = (int)t;
    t->ctx.ebp = (unsigned) &t->stack[-3];
    t->ctx.esp = (unsigned) &t->stack[-3];
    t->ctx.eip = (unsigned) fun;
    printf("BP = %x, SP = %x, PC = %x\n", t->ctx.ebp, t->ctx.esp, t->ctx.eip);

        if(current == NULL)
        {
                t->next = t;
                current = t;
        }else{
                THREAD *tmp = current->next;
                current->next = t;
                t->next = tmp;
        }
    return t;
}







void myfunc3(void)
{
    int j, nptrs;
#define SIZE 100
    void *buffer[100];
    char **strings;

        /*nptrs = backtrace(buffer, SIZE);*/
        printf("backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

        /*strings = backtrace_symbols(buffer, nptrs);*/
        if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
        printf("%s\n", strings[j]);

    free(strings);
}




int dumpstack(unsigned char * addr, int size){
    int i=0;
    unsigned char *p = addr;
    //unsigned char *p = addr;
    for(i=0;i<size; i++){
        if(i%4==0)printf("\n[%08x][%08x] ", p, *((unsigned int*)p));
        printf("%02x", *p );
        p++;
    }
}
int trace(){
    register int ebp asm("ebp");
        int *frame = (int *)ebp;
    while( frame[0] != 0 ){
        printf("STACK EBP=%x, EIP=%x\n", frame[0], frame[1]);
        frame=(int *)frame[0];
    }
        return 0;
}


int wait(){
    int i;
        for(i=1;i<0x1FFFFFF;i++);
        return 0;
}

void timer_interrupt(){

        printf("Scheduling...\n");
        if( current == NULL) return;

    if(save_context(current) != 0){
        return;
    }
        current = current->next;
        printf("Switching to Thread: %s\n", current->name);
        restore_context(current,1);

}
void initialise_timer(void) {
    setup_signal(SIGPROF,timer_interrupt);
    start_timer();
}
void start_thread()
{
    restore_context(current,1);
}




void thread_function(THREAD *t, void *arg){
    int i=0;
    trace();
    for(i =0;i<=100000000;i++)
    {
        printf("[%s][%s] = %d\n",t->name, arg, i);
        wait();
    }
}

int main(int argc, char *argv[])
{
#ifndef SO
    create_thread("T1", thread_function, (void*)"THREAD1");
    create_thread("T2", thread_function, (void*)"THREAD2");
    create_thread("T3", thread_function, (void*)"THREAD3");

    initialise_timer();
        start_thread();

#else
    create_thread("T1", thread_function, (void*)"THREAD1");
    create_thread("T2", thread_function, (void*)"THREAD2");
    create_thread("T3", thread_function, (void*)"THREAD3");

        void (*fn)(THREAD*, void *);
        int i=0;
        char *error=NULL;
        for(i=1; i<argc; i++)
        {
                void *lib_handle = dlopen(argv[i], RTLD_LAZY);
                if (!lib_handle)
                {
                        fprintf(stderr, "[%s]: %s\n", argv[i], dlerror());
                        exit(1);
                }

                fn = dlsym(lib_handle, "main");
                if ((error = dlerror()) != NULL)
                {
                        fprintf(stderr, "%s\n", error);
                        exit(1);
                }

                create_thread("Threads", fn, (void*)"Threads");
                /*dlclose(lib_handle);*/
        }
    initialise_timer();
        start_thread();
#endif

}
