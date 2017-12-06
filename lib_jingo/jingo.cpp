#include <stdio.h>

#include "jingo.h"

#if 1
const char * funcD(void); //external undefined symbol
#else
const char * funcD(void){    return "funcD in " __FILE__;}
#endif


static int local_subfunc(int i){
    return i*8;
}
static void local_func(int i){
    printf("\tcan you see me=%d, %s?\n", local_subfunc(i), funcD());
}


void nothing(void)
{
}
const char * funcA(void)
{
    return "funcA in jingo";
}
const char * funcB(void)
{
    return "funcB in jingo";
}
const char * funcC(void)
{
    return funcD();
}


static void * ptr = (void*)&funcD;
void jingo(void)
{
    puts("\tjingo is called\n");
    
    local_func(1);
    
    //the actual funcA called will be determined by "symbol binding" process.
    //dynamic linker will check in the order of DT_NEEDED .so libs in "readelf -d finalexe";
    //but first the executable self is checked, and then the DT_NEEDED .so files.
    //
    //because in CMakeLists.txt we specify jingo2 before jingo:
    //
    //         target_link_libraries(tjingo jingo2 jingo)
    //
    //so the funcA() in jingo2 will be called finally.
    //
    //because every symbol in final process space is handled this way
    //so every other funcA() will be hidden/overrided by the one in jingo2.
    //unless we declare funcA() to be static in jingo
    
    printf("\t%s calling funcA() got \"%s\"\n",__FUNCTION__, funcA());
    
    printf("\t%s is called, my address is %p, printf's address is %p, funcD at %p\n", __FUNCTION__, &jingo, &printf, ptr);
}

