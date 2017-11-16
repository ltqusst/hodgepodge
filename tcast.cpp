#include <stdio.h>
#include <typeinfo>

class Base
{
public:
    Base(){};
    
    //////////////////////////////////////////////////
    // virtual in Base is vital for typeid() to work 
    //////////////////////////////////////////////////
    virtual void fun(void){};
};

class Derived: public Base
{
public:

    virtual void fun(void){};
};

void whoami(Base * pb)
{
    if(typeid(*pb) == typeid(Derived)) printf("Derived\n");
    if(typeid(*pb) == typeid(Base)) printf("Base\n");
}

int main()
{
    Base *p1 = new Base();
    Base *p2 = new Derived();
    
    whoami(p1);
    whoami(p2);
    
    delete p1;
    delete p2;
}
