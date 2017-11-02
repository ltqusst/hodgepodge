#include <iostream>
#include <memory>

class T
{
public:
    T(int id):id(id)
    {std::cout << "ctor T #" << id << "\n";}
    ~T(){std::cout << "    dtor T #" << id << "\n";}
    int id;
};


//======================================================
// whether smart pointer accept NULL pointer?
T * create(bool bToFail = false, int id = 0)
{
    if(bToFail) return NULL;
    
    return new T(id);
}

void destroy(T* pt)
{
    delete pt;
}

static void test1(void)
{
    std::shared_ptr<T> p0;
    
    for(int id = 100; id < 104; id++)
    {
#if 0    
        //reset will auto delete old object 
        p0.reset(create(id == 102, id), destroy);
#else
        //re-assign will auto delete old object too
        p0 = std::shared_ptr<T>(create(id == 102, id), destroy);
#endif
        //smart pointer will be reset even when passing a NULL pointer into it
        //so we can always use it directly without first accept the result pointer using a normal one
        
        if(p0 == nullptr) std::cout << "!!!! Create failed on " << id << "\n";
    }
}

//======================================================

class Compose
{
public:
    Compose():
        p0(create(true, 100), destroy),
        p1(new T(101))
    {
        if(!p0) std::cout << "!!!! Create failed on p0\n";
    }
private:    
    std::shared_ptr<T> p0;
    std::shared_ptr<T> p1;
};

static void test2()
{
    Compose c;
}
//======================================================
// how to initialize array of object
static void test3()
{
    T  ts[3]={T(990), T(991), T(992)};
    
    std::cout << "ts:";
    for(int i=0;i<3;i++)        std::cout << ts[i].id << ",";
    std::cout << '\n';
}

//more flexible life-cycle version using shared_ptr
static void test4()
{
    std::shared_ptr<T> pts[3]={
        std::shared_ptr<T>(new T(880)),
        std::shared_ptr<T>(new T(881)),
        std::shared_ptr<T>(new T(882)),
    };
    std::cout << "pts:";
    for(int i=0;i<3;i++)        std::cout << pts[i]->id << ",";
    std::cout << '\n';
}


int main()
{
    test1();
    //test2();
    //test3();
    //test4();
    
    std::cout << "exit()\n";
    return 0;
}
