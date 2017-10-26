#include <iostream>
#include <functional>
#include <string>
#include <chrono>


//The only best way to accept full-featured-lambda/functor as parameter
//but we also lost the way to judge if its valid or null ptr
template <typename F>
float calc1(F f) 
{ return -1.0f * f(3.3f) + 666.0f; }

inline float calc3(float (*f)(float) = NULL) 
{ return -1.0f * f(3.3f) + 666.0f; }


inline float calc2(const std::function<float(float)> &f) 
{ return -1.0f * f(3.3f) + 666.0f; }



#define dotest(f,p)\
{\
    using namespace std::chrono;\
    const auto tp1 = high_resolution_clock::now();\
    for (int i = 0; i < 1e7; ++i) {\
        f(p);\
    }\
    const auto tp2 = high_resolution_clock::now();\
    const auto d = duration_cast<milliseconds>(tp2 - tp1);  \
    std::cout << d.count() << std::endl;\
}




class Functor_f
{
public:
    float operator()(float arg){ return arg * 0.5f; };
};
static Functor_f functor_f;


int main() 
{
    const float coeff=0.5f;
    auto lambda_f = [coeff](float arg){ return arg * coeff; };
    
    dotest(calc1,lambda_f);    //54
    dotest(calc2,lambda_f);    //1064   **** do not pass lambda as std::function object! *****
    //dotest(calc3,lambda_f);    //135    lambda function can only convert to function pointer if no captures in there

    dotest(calc1,functor_f);    //55
    dotest(calc2,functor_f);    //1066   **** do not pass functor as std::function object! *****
    //dotest(calc3,functor_f);  //       **** functor cannot convert to function pointer
}

