#include <stdio.h>

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>


void jingo();
void jingo2();

const char * funcA(void);
const char * funcB(void);
const char * funcC(void);

//[Symbol binding]=====================================================
// this symbol can override funcA in any other .so,
// when -rdynamic is specified during compiling this exe
//const char * funcA(void){    return "funcA in main";}

// uncomment this symbol will override the corresponding one in libc
// because exe is searched first.
//int puts(const char *s) { printf(">>%s",s); }


//[LD_PRELOAD]=========================================================
// If you set LD_PRELOAD to the path of a shared object, that file will 
// be loaded before any other library (including the C runtime, libc.so).
// So to run ls with your special malloc() implementation, do this:
//      $ LD_PRELOAD=/path/to/my/malloc.so /bin/ls

const char * funcB(void){
    return "funcB in main.c";
}
const char * funcD(void){
    return "funcD in main.c";
}

static int array[1024]={0};

extern "C" char __executable_start;
extern "C" char __etext;

extern "C" char _GLOBAL_OFFSET_TABLE_;

int main()
{   
    printf("//==================================\n");
    printf("static symbols:\n");
    printf("__executable_start = %p\n", &__executable_start);
    printf("           __etext = %p\n", &__etext);
    printf("            &funcB = %p\n", funcB);
    printf("            &array = %p\n", array);    
    printf("             &main = %p\n", main);
    
    printf("//==================================\n");
    printf("dynamic symbols:\n");
    
    /*
    lazy symbol binding:
    
       When you access a dynamic function by calling it
               jingo();
       gcc will generate a call to an entry <_Z5jingov@plt> in .plt section
               callq  400750 <_Z5jingov@plt>
       this entry is actually a trampoline jump into a entry in _GLOBAL_OFFSET_TABLE_ (got), 
       initially it's pointing to a dynamic loader's  which does _dl_runtime_resolve()
       the dynamic lazy binding (by symbol looking-up & call target function for 1st time & update
       plt entry for the rest possible calls).
       
       this can save a lot of time if app only calls few limited dynamic functions 
       at runtime.
       
       but if you are trying to get the address of the dynamic function before first call it
       gcc will just give you the address of this trap jump entry in .plt section instead of 
       the read address. 
       
       
       the jump instruction is in code page, read-only
       
       but the _GLOBAL_OFFSET_TABLE_ is accessable
       
    */
    unsigned char * p1 = (unsigned char*)&jingo;
    unsigned char * p2 = (unsigned char*)&jingo2;
    
#define GOT_ENTRY_PTR(func) (void **)(((char *)&func) + 6 + (*(unsigned int *)(((char*)func) + 2)))
    
    void ** p_got_jingo = GOT_ENTRY_PTR(jingo);
    void ** p_got_jingo2 = GOT_ENTRY_PTR(jingo2);
        
    printf("            &jingo  = %p plt entry machine code: = %02X %02X %02X %02X %02X %02X GOT addr: %p\n", 
            &jingo, p1[0],p1[1],p1[2],p1[3],p1[4],p1[5], p_got_jingo);
    printf("            &jingo2 = %p plt entry machine code: = %02X %02X %02X %02X %02X %02X GOT addr: %p\n", 
            &jingo2, p2[0],p2[1],p2[2],p2[3],p2[4],p2[5], p_got_jingo2);
            
    printf("           &printf = %p\n", &printf);
    printf("            &funcA = %p\n", &funcA);
    
    
    printf("calling jingo :\n");jingo();    //_dl_runtime_resolve  is called
    printf("calling jingo :\n");jingo();    //jingo                is called
    printf("calling jingo :\n");jingo();    //jingo                is called
    printf("calling jingo2:\n");jingo2();
    *p_got_jingo = *p_got_jingo2; //now do some trick, replace the GOT entry
    printf("calling jingo :\n");jingo();
    
    
    printf("            &jingo = %p content = %p\n", &jingo, *(void **)&jingo);
    printf("           &jingo2 = %p content = %p\n", &jingo2, *(void **)&jingo2);
    
    printf("funcA()=%s\n", funcA());
    printf("funcB()=%s\n", funcB());
    printf("funcC()=%s\n", funcC());
    
    
    #if 0
    //no more printf now ?
    void ** p_got_printf = GOT_ENTRY_PTR(printf);
    *p_got_printf = *p_got_jingo2; //not do some trick, replace the GOT entry
    
    printf("funcA()=%s\n", funcA());
    printf("funcB()=%s\n", funcB());
    printf("funcC()=%s\n", funcC());
    #endif
    
    char info_src[30];
    char info_dst[3];
    
    memcpy_s(info_dst,3, info_src, 30);
}





