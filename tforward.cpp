#include <iostream>
#include <type_traits>
#include  <typeinfo>

//http://thbecker.net/articles/rvalue_references/section_08.html
//https://stackoverflow.com/questions/3582001/advantages-of-using-forward


//==========================================================
#include <type_traits>
#include <typeinfo>
#ifndef _MSC_VER
#   include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>

template <class T>
std::string type_name()
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
           (
#ifndef _MSC_VER
                abi::__cxa_demangle(typeid(TR).name(), nullptr,
                                           nullptr, nullptr),
#else
                nullptr,
#endif
                std::free
           );
    
    std::string r = own != nullptr ? own.get() : typeid(TR).name();
    
    if (std::is_const<TR>::value)       r += " const";
    if (std::is_volatile<TR>::value)    r += " volatile";
    
    if (std::is_lvalue_reference<T>::value) r += " &";
    if (std::is_rvalue_reference<T>::value) r += " &&";
    return r;
}
//==============================================================


void E(int& a, int& b, int& c)
{
    std::cout << "a:" << a << ","
              << "b:" << b << ","
              << "c:" << c << "." << std::endl;
    a++;
}

template <typename A, typename B, typename C>
void f(A& a, B& b, C& c)
{
    E(a, b, c);
}

template <typename A, typename B, typename C>
void f2(const A& a, const B& b, const C& c)                     //const       : incase pass in rvalues
{
    E(const_cast<A&>(a), const_cast<B&>(b), const_cast<C&>(c)); //const_cast<>: incase E requires non-const reference
}


template<class T>
void foo1(T& t)
{
    if(std::is_reference<decltype(t)>::value)
    {
        if(std::is_lvalue_reference<decltype(t)>::value) std::cout << "lvalue_reference";
        if(std::is_rvalue_reference<decltype(t)>::value) std::cout << "rvalue_reference";
    }else{
        std::cout << "not reference";
    }
    std::cout << std::endl;
}

/*

If given a reference to a reference (note reference is an encompassing term meaning both T& and T&&), 
we use the following rule to figure out the resulting type:

    "[given] a type TR that is a reference to a type T, an attempt to create the type 
    “lvalue reference to cv TR” creates the type “lvalue reference to T”, while an attempt 
    to create the type “rvalue reference to cv TR” creates the type TR."

Or in tabular form:

TR   R

T&   &  -> T&  // lvalue reference to cv TR -> lvalue reference to T
T&   && -> T&  // rvalue reference to cv TR -> TR (lvalue reference to T)
T&&  &  -> T&  // lvalue reference to cv TR -> lvalue reference to T
T&&  && -> T&& // rvalue reference to cv TR -> TR (rvalue reference to T)

Next, with template argument deduction: 
    if an argument is an lvalue A, we supply the template argument with an lvalue reference to A. 
    Otherwise, we deduce normally. 
This gives so-called universal references (the term forwarding reference is now the official one).

*/

void forwarded_func(int &x)
{
    x++;
}

template<class T>
void foo2(T&& t)
{
    //two possible deductions when called with different types of args
    std::cout << ">>> calling " << __PRETTY_FUNCTION__ << std::endl;
    
    //
    //  V can be const/volatile reference
    // 
    //  foo2(rvalue of type V)
    //  T       : V
    //  V&&  t  : rvalue reference
    //  
    //  foo2(lvalue of type V)
    //  T       : V&
    //  V& &&t  : rvalue reference(&&) to a lvalue reference type(V&) => lvalue reference(V&)
    //
    std::cout << ">>>              T:" << type_name<T>() << ";" << std::endl;
    std::cout << ">>>    decltype(t):" << type_name<decltype(t)>() << ";" << std::endl;
    
    
    //forwarded_func(t); 
    
    //if input is actually a rvalue-reference, this will work as if it's a lvalue 
    
    // though t is rvalue-reference, but it self is a lvalue (because it has a name, so it has address and can be changed, casted, 
    //   move semantics work correctly only on un-named invisible values)
    // 
    
    // static_cast<T&&>(t) will work by re-program the type information as follow:
    //                     1.if actual argument is lvalue of type V, T is V& and result type is [ V& &&=> V& ] lvalue-reference
    //                     2.if actual argument is rvalue of type V, T is V and result type is [V&&] rvalue-reference
    //
    // key is that the return value of forward() is rvalue because it has no name, but t is lvalue because it has name!
    // 
    
    //forwarded_func(std::forward<T>(t));
    
/*

finally if we want forwarding calling another function(may be template), do not directly pass t
because 


std::forward<A>(a);
// is the same as
static_cast<A&&>(a);    
*/
}

void modify_int(int &i){i ++;}
void its_ok_to_modify_rvalue_reference_because_its_temporary(int && i)
{
    std::cout << ">>> calling " << __PRETTY_FUNCTION__ << std::endl;
    std::cout << ">>>     before: " << i;
    //so its OK to access rvalue_ref as if it's a lvalue
    modify_int(i);
    std::cout << " after: " << i << std::endl;
}


int main()
{
    int a=1, b=2, c=3;
    const int ca=1;
    
    its_ok_to_modify_rvalue_reference_because_its_temporary(1);
    
    //1st failed try
    f(a,b,c);   //OK: a:1,b:2,c:3.
    //f(1,2,3); //invalid initialization of non-const reference of type ‘int&’ from an rvalue of type ‘int’
    
    
    //2nd try
    f2(1,2,3);  //OK: a:1,b:2,c:3.
    f2(a,b,c);

    std::cout << "foo1:" << std::endl;    
    //foo1(1);  //invalid: passing rvalue to lvalue reference
    foo1(a);    //
    foo1(ca);   //
    
    std::cout << "foo2:" << std::endl;    
    std::cout << "foo2(1) "<< std::endl;
    foo2(1);    
    std::cout << "foo2(a) "<< std::endl;
    foo2(a);
    std::cout << "foo2(ca) "<< std::endl;
    foo2(ca);
    
    std::cout << "const int ca=1, ca is ";
    if(std::is_const<decltype(ca)>::value)
        std::cout << "const" << std::endl;
    else
        std::cout << "non-const" << std::endl;
}

