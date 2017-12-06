#include <stdio.h>


void nothing(void)
{
}

const char * funcA(void)
{
    return "funcA in jingo2";
}

const char * funcB(void)
{
    return "funcB in jingo2";
}

const char * funcD(void);
const char * funcC(void)
{
    return funcD();
}

void jingo2(void)
{
    puts("\tjingo2 is called\n");
    
    printf("\t%s calling funcA() got \"%s\"\n",__FUNCTION__, funcA());
    printf("\t%s is called, my address is %p, printf's address is %p\n", __FUNCTION__, &jingo2, &printf);
}

