#include <iostream>


// primary templated
//
// any other template specialization cannot change the parameter list
// format of its instantiation, it said, A can only be instantiated 
// by this form:
//          A<T1,T2,I>
// but primary template only says T1,T2 are general types, I is int
// full/partial specialization can be applied instead when more constrains
// are met: 
//        these constrains are more flexible than just specify 
//        detailed type/value of each template parameter as indicated 
//        in following examples.
//
template<class T1, class T2, int I>
class A 
{
public:
    A(){std::cout << "A\n";}    
};

// [partial specialization] is :
//        A partial instantiation of the primary template by
//        using another set of template-parameter
//        
//  without <> following the class name A, partial specialization #1
//  is just another template totally different from the primary template A
//  but by adding <> we noted that we are treating it as a special sub-set of
//  original primary template of A, so for this new partial A, we do not use
//     A<MyClassType, 5> to instantiate, but still follow primary format as 
//     A<MyClassType,MyClassType*,5>

// #1: partial specialization where T2 is a pointer to T1
//         
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

template<class T>
struct my_traits{
    typedef T type;
};

int main()
{
    A<int, int, 5>              a;
    A<int, int*, 6>             a1;
    A<float*, float*, 5>        a2;
    A<float, int*, 5>           a3;
    A<std::tuple<int>, int, 5>  a4;
    
    my_traits<int>::type        i=9;
}


