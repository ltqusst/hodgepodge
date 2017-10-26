#include <cstdio>
#include <vector>

template<class T>
class Base
{
public:
	int fun(){	
		objs.push_back(T());
		T &t = objs.back();

		t.name = 1;
		t.show_type();
		printf("sizeof(t.name)=%d\n", sizeof(t.name));
	}
	int f2(){
		T &t = objs.back();
		//t.v=1;
		
	}
	std::vector<T> objs;
};



class Obj1
{
public:
	int name;
	int v;
	void show_type(){
		printf("Obj1\n");
	}
};


class Obj2
{
public:
	char name;
	void show_type(int param1=10){
		printf("Obj2, param1=%d\n", param1);
	}
};


int main()
{
	Base<Obj1> b1;
	Base<Obj2> b2;

	b1.fun(); 
	b2.fun();
}



