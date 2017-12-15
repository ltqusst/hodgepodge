#include <cstdio>
#include <iostream>
#include <cstring>

#ifdef XXX

jhjksd

#endif

class A
{
public:
	A(const char * pstrname, int id):name(pstrname),i(id){
		printf(">>> ctor %s.%d\n",name, i);
    }
	~A(){
		printf(">>> dtor %s.%d\n",name, i);
	}
	void show_name(){
		printf("   %s.%d is showing name\n",name, i);
	}
	
protected:
	const char * name;
	const int i;
};

class C
{
public: 
	int i;
};

class B:public A
{
public:
	using A::A;
	void dummy(){ printf("dummy is called on %s.%d kkk=%d\n",name, i, kkk);}

	int kkk;
};

#include <string>

class myobj
{
public:
    myobj():data(6){
        printf("\tmyobj:ctor\n");
    }
    ~myobj(){
        printf("\tmyobj:dtor\n") ;   
    }
    int data;
};

struct ZeroSet
{
    int q;
    int         a[10]{};
    std::string s;
    myobj       m;
    
    
    ZeroSet(){
        printf("\tZeroSet:ctor\n");
    }
    ~ZeroSet(){
        printf("\tZeroSet:dtor\n");
    }
    
    void show(void){
        printf("[%s], my:%d, a:", s.c_str(), m.data);
        for(int i=0;i<10;i++)
            printf("%d,", a[i]);
        printf("\n");
    }
};


int main()
{
    ZeroSet z = {};    z.show();
    z.a[1]=10;    z.s="hello";    z.m.data=18;    z.show();
    z = {};    z.show();
    
    char text[8];
    std::cin.getline(text,sizeof(text));
    printf("text=%s, cnt=%d\n", text, strlen(text));
    return 0;
    
	A a1("aaa",1);
	C c;

	a1.show_name();
	A("AAA",2).show_name();
	B("BBB",3).show_name();

	//C-style cast on reference is the same as reinterpret_cast
	// object c has no member name actually, the result maybe un-expectable
	((B &)c).dummy();
	reinterpret_cast<B&>(c).dummy();
}




