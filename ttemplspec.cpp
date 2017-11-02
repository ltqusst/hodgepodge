#include <iostream>


// primary templated
template<class T1, class T2, int I>
class A 
{
public:
    A(){std::cout << "A\n";}    
};

// #1: partial specialization where T2 is a pointer to T1
template<class T, int I>
class A<T, T*, I> 
{
public:
    A(){std::cout << "A#1\n";}
};

// #2: partial specialization where T1 is a pointer
template<int I, class T2, class T>
class A<T*, T2, I> 
{
public:
    A(){std::cout << "A#2\n";}
};

// #3: partial specialization where T1 is int, I is 5, and T2 is a pointer
template<class T>
class A<float, T*, 5> 
{
public:
    A(){std::cout << "A#3\n";}
};

// #4
template<class Head, class ... Tail, int ID>
class A<std::tuple<Head, Tail...>, Head, ID>
{
public:
    A(){std::cout << "A#4\n";}
};

int main()
{
    A<int, int, 5>              a;
    A<int, int*, 6>             a1;
    A<float*, float*, 5>        a2;
    A<float, int*, 5>           a3;
    A<std::tuple<int>, int, 5>  a4;
}


