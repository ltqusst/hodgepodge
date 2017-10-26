#include <cstdio>


#include "lib_jingo/jingo.h"
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/syscall.h>


void * show_pid_tid(void* p)
{
    const char * name = (const char *)p;
    pid_t pid = getpid();
    pid_t tid = (pid_t) syscall (SYS_gettid);
    printf(" name=%s PID=%d TID=%d\n",name, pid, tid);//(unsigned int)pthread_self());
}

void test1()
{
    pthread_t p1,p2,p3;
    
    show_pid_tid(NULL);
    
    pthread_create(&p1, NULL, show_pid_tid, (void*)__FUNCTION__);
    pthread_create(&p2, NULL, show_pid_tid, (void*)__FUNCTION__);
    pthread_create(&p3, NULL, show_pid_tid, (void*)__FUNCTION__);
    
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    pthread_join(p3, NULL);
}

//===========================================================================
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>


#include "thread_queue.h"

//======================================================================================
thread_queue<int> theque;

void call_from_thread(int &k, int th)
{
    show_pid_tid((void*)"test2");
    while(1)
    {
        int i;
        if(th > 0)
        {
            if(!theque.get(i, [th](const int & k){return k>=th;}))
                break;
        }else
        {
            if(!theque.get(i))
                break;
        }
        k++;
    }
}

void test2()
{
  int t1=0, t2=0, t3=0;

  std::thread th1(call_from_thread, std::ref(t1), 0);
  std::thread th2(call_from_thread, std::ref(t2), 0);
  std::thread th3(call_from_thread, std::ref(t3), 9999);
  
  for(int i=0;i<10000;i++)
      theque.put(i);
  theque.close();

  th1.join();
  th2.join();
  th3.join();
  printf("t=%d (%d+%d+%d), max_size=%d\n", (t1+t2+t3), t1,t2,t3, theque.max_size());
}



//======================================================================================
#include <atomic>
class MyObj
{
public:
    MyObj(){cnt0++;}
    ~MyObj(){cnt1++;}
    
    static std::atomic_int cnt0;
    static std::atomic_int cnt1;
    
};

std::atomic_int MyObj::cnt0(0);
std::atomic_int MyObj::cnt1(0);


thread_queue<std::shared_ptr<MyObj> > sque;

void worker_thread(int &k)
{
    show_pid_tid((void*)"test2");
    while(1)
    {
        int i;
        std::shared_ptr<MyObj> p;
        
        if(!sque.get(p)) break;
        
        k++;
    }
}

void test3()
{
  int t1=0, t2=0, t3=0;
  std::thread th1(worker_thread, std::ref(t1));
  std::thread th2(worker_thread, std::ref(t2));
  std::thread th3(worker_thread, std::ref(t3));

  for(int i=0;i<1000;i++)
  {
      std::shared_ptr<MyObj> p(new MyObj);
      sque.put(p);
  }
  sque.close();

  th1.join();
  th2.join();
  th3.join();
  
  printf("t=%d (%d+%d+%d), max_size=%d, ctor:%d  dtor:%d   sque remain:%d\n", (t1+t2+t3), t1,t2,t3, sque.max_size(),
  MyObj::cnt0.load(), MyObj::cnt1.load(), sque.size());
}

int main()
{
    nothing();
    printf("main()  sizeof(wchar_t) = %d;\n", sizeof (wchar_t));
    
    std::shared_ptr<char> pv(new char[100], std::default_delete<char[]>());
    
    //test1();  
    test2();
    //test3();    
}


