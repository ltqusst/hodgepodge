#include <cstdio>


#include "lib_jingo/jingo.h"
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/syscall.h>

#include <iostream>
#include <sstream>

void show_pid_tid(const char *name)
{
    int cpuid = sched_getcpu();
    pid_t pid = getpid();
    pid_t tid = (pid_t) syscall (SYS_gettid);
    
    cpu_set_t cpuset;
    
    pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    
    std::ostringstream out; 
    out << "name=" << name << " PID=" << pid << " TID=" << tid << "CPU affinity:";    
    for (int j = 0; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &cpuset)) {
            out << " [" << ((j==cpuid)?"*":"") << j << "]";
        }
    }
    out << std::endl;
    std::cout << out.str() << '\n';
}

void * thread_show_pid_tid(void* p)
{
    show_pid_tid((const char *)p);
    return NULL;
}

void test1()
{
    pthread_t p1,p2,p3;
    
    show_pid_tid(__FUNCTION__);
    
    pthread_create(&p1, NULL, thread_show_pid_tid, (void*)__FUNCTION__);
    pthread_create(&p2, NULL, thread_show_pid_tid, (void*)__FUNCTION__);
    pthread_create(&p3, NULL, thread_show_pid_tid, (void*)__FUNCTION__);
    
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
    show_pid_tid("test2");
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
    show_pid_tid("test2");
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

//=============================================================================================================
#include <iostream>
#include <chrono>
#include "thread_pool.h"


struct obj_result
{
    int x,y,w,h;
};
struct AllResult
{
    int fid;
    std::vector<obj_result> rc;
};

class tracker
{
public:
    int id;
    tracker(int id=0):id(id){}
    
    AllResult track(int fid, 
                    std::shared_future<AllResult> prev, 
                    std::shared_future<AllResult> det)
    {
        std::cout << " >>>>> track:";
        
        if(prev.valid()){
            const AllResult& track_res = prev.get();
            std::cout << "(with track " << track_res.fid << ")";
            std::cout.flush();
        }
        
        if(det.valid()){
            const AllResult& det_res = det.get();
            std::cout << "(with detect " << det_res.fid << ")";
            std::cout.flush();
        }
        
        std::cout << "frame_" << fid << ":" << id << std::endl;
        
        id ++;
        std::this_thread::sleep_for(std::chrono::milliseconds(125));
        
        return AllResult{fid, {}};
    }
};

class detecter
{
public:
    AllResult detect(int fid)
    {
        show_pid_tid("detecter");
        (std::cout << "                    deteting frame " << fid << " ..." << std::endl).flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return AllResult{fid, {}};
    }
};

class EasyTimer
{
public:
    EasyTimer():t0(std::chrono::steady_clock::now()){}
    
    void reset(void){t0 = std::chrono::steady_clock::now();}
    template<class D>
    int elapsed(){
        auto d = std::chrono::steady_clock::now() - t0;
        return std::chrono::duration_cast<D>(d).count();
    }
    
    std::chrono::time_point<std::chrono::steady_clock> t0;
};

void test4()
{
    ThreadPool pool[2] = {{1,10},{1,10}};
    
    ThreadPool &th_track  = pool[0];
    ThreadPool &th_detect = pool[1];
    
    int i;
    const int task_cnt = 20;
    
    tracker t(0);
    detecter d;
    
    EasyTimer t0;
    
    std::shared_future<AllResult> fprev;
    std::shared_future<AllResult> fdet;
    
    for(i=0;i<task_cnt;i++)
    {
        //because only one thread is the pool, track() will be executed sequentially
        if((i % 4) == 0)
            fdet = th_detect.enqueue(&detecter::detect, &d, i).share();
        else
            fdet = std::shared_future<AllResult>();
        
        fprev = th_track.enqueue(&tracker::track, &t, i, fprev, fdet).share();
    }
    std::ostringstream out; 
    out << " ************************ (all " <<  task_cnt << " taskes are enqueued) *********************** " << std::endl; 
    std::cout << out.str();
    
    fprev.get();
    
    std::cout << "Time consumed:" <<  t0.elapsed<std::chrono::milliseconds>() << " milliseconds" <<  std::endl;
    
    //auto d1 = th.enqueue(stage1, std::string("Hello"));
    
    //std::future<int> d1 = std::async(stage1, 1);
    //std::future<int> d2 = std::async(stage2, std::move(d1));
    //printf(" >>>>> %d\n", (int)d2.get());
    
    th_track.stop_all();
}


int main()
{
    nothing();
    printf("main()  sizeof(wchar_t) = %d;\n", sizeof (wchar_t));
    
    std::shared_ptr<char> pv(new char[100], std::default_delete<char[]>());
    
    //test1();  
    //test2();
    //test3();
    test4();    
}


