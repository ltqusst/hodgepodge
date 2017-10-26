#include <cstdio>

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


int main()
{
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




