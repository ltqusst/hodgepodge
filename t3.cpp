#include <cstdio>
#include <iostream>
#include <type_traits>


template<typename _Tp, _Tp __v>
struct integral_constant
{
  static _Tp                  value;

  typedef _Tp                           value_type;

  operator value_type() { return value; }
  operator value_type&() { return value; }
};


typedef integral_constant<int, 0> Int;




struct integral
{
integral(){}
static int value;
operator int&() { return value; }

int i;

void op()       {printf("op non-const\n");}
void op() const {printf("op const\n");}
};

int integral::value=0;


class cdtor
{
cdtor(){printf(" >>>>> ctor \n");}
};


int main(){
    int a;
    const int b = 3;
	std::is_const<decltype(a)> ax;
	std::is_const<decltype(b)> bx;
    std::cout << ax << std::endl;    // 0
    std::cout << bx << std::endl;    // 1

	char * p = (char*)::operator new(100);
	for(int i=0;i<100;i++) p[i] = i;
	::operator delete(p);

	const integral c;
	c.op();


	integral x, y;
	integral &rx=y	;
	//rx = y;

	std::cout << "x=" << x << ",y=" << y << std::endl;
	x++;
	x += 6;
	std::cout << "x=" << x << ",y=" << y << std::endl;

	std::cout << "std::is_trivially_constructible<int>:" << std::is_trivially_constructible<int>::value << std::endl;
	std::cout << "std::is_trivially_constructible<integral>:" << std::is_trivially_constructible<integral>::value << std::endl;
}

