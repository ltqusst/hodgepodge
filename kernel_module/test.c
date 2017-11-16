#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <pthread.h>


void cat(int fd)
{
    ssize_t sz;
    char buf[100];
    char txt[32];
    while((sz = read(fd, buf, sizeof(buf))) > 0){
        sprintf(txt,"[%d bytes]", sz);
        write(STDOUT_FILENO, txt, strlen(txt));
        
        if(write(STDOUT_FILENO, buf, sz) != sz){
            perror("write error");
            exit(1);
        }
    }
    if(sz < 0)
        perror("read error");
}

void * thread(void* pv)
{
    int * pfd = (int*)pv;
    
    cat(*pfd);
    
    close(*pfd);
    
    *pfd = 0;
}

int main(int argc, char * argv[])
{
    pthread_t p1;
    
    if(argc < 2)
    {
        printf("Usage: %s devicename\n", argv[0]);
        exit(0);
    }
    
    int i;
    int fd = open(argv[1], O_RDONLY);
    if(fd == -1){
        perror("open failed");
        exit(1);
    }
    pthread_create(&p1, NULL, thread, (void*)&fd);
    
    struct timespec tim, tim2;
    tim.tv_sec  = 1;
    tim.tv_nsec = 0;
    char c;
    for(i=0;i<1000 && fd > 0;i++)
    {
        c = (i % 10) + '0';
        write(STDOUT_FILENO,&c, 1);
        
        if(nanosleep(&tim , &tim2) < 0 )   
        {
           perror("Nano sleep system call failed \n");
           break;
        }
    }
    
    pthread_join(p1, NULL);
}


