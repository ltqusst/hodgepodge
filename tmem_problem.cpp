#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char *argv[]) 
{
    for(int i=0;i<argc;i++)
        printf("\033[1;31m argv[%d]=%s \033[0m\n", i, argv[i]);


    char * p = new char[100];
    //p[100] = 1;
    printf("p=%p\n", p);
    delete []p;
    
    p = new char[100];
    //p[-1] = 1;
    printf("p=%p\n", p);
    delete []p;

    char * p2 = new char[100];
    p = new char[100];
    p[100] = 1;
    printf("  p=%p\n", p);
    delete []p;
    delete []p2;
    
    p = (char *)malloc(100);
    printf("p=%p\n", p);
    free(p);
    
    
    p = new char[100];
    printf("p=%p\n", p);
    delete []p;

    p = (char *)malloc(100);
    printf("p=%p\n", p);
    free(p);

    p = new char[100];
    printf("p=%p\n", p);
    delete []p;

    
}
