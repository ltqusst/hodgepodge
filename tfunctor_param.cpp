#include <iostream>
#include <functional>
#include <string>
#include <chrono>


//The only best way to accept full-featured-lambda/functor as parameter
//but we also lost the way to judge if its valid or null ptr
template <typename F>
float calc1(F f) 
{ return -1.0f * f(3.3f) + 666.0f; }

//using std::function() as a general call-back method to accept all lambda/bind/(normal function pointer)
//but its introduces 10x much call-consumption, so only do this for "BIG" callbacks
inline float calc2(const std::function<float(float)> &f) 
{ return -1.0f * f(3.3f) + 666.0f; }

//C-style callback is very limited, only support C-style function pointer
inline float calc3(float (*f)(float) = NULL) 
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
    std::cout << d.count() << " milliseconds" << std::endl;\
}




class Functor_f
{
public:
    float operator()(float arg){ return arg * 0.5f; };
};
static Functor_f functor_f;


//==============================================================
// can std::bind() generate a function pointer type?
//   --- NO, better use template type to represent the return value of std::bind()
typedef int (*FunctionPtr)(int a);

int true_func(int a, int b){
    return a + b;
}

void test(FunctionPtr func, int a)
{
    std::cout << "func(" << a << ")=" << func(a) << "\n";
}
template<class F>
void test2(F func, int a)
{
    std::cout << "func(" << a << ")=" << func(a) << "\n";
}

//==============================================================
// can static function be template-based?
class D
{
public:
    void call_back1(int a){
        std::cout << "D::call_back1(" << a << ")" << std::endl;
    }
    void call_back2(int a){
        std::cout << "D::call_back2(" << a << ")" << std::endl;
    }

    template<class F>
    static void call_back(int a);
};

template<class F>
void D::call_back(int a)
{
    (F) (a);
}

void test_call_back(FunctionPtr func)
{
    func(10);
}

int main() 
{
    const float coeff=0.5f;
    auto lambda_f = [coeff](float arg){ return arg * coeff; };
    
    auto func = std::bind(true_func, std::placeholders::_1, 10);
    std::cout << "func(1)=" << func(1) << "\n";

/*    
    D d0;
    auto cb = std::bind(&D::call_back1, d0, std::placeholders::_1);
    auto cb2 = D::call_back<cb>;
    test_call_back(cb2);
*/
    
    //test(func, 2);      // compile error: std::bind cannot return a function-pointer type
    test2(func, 2);       // OK, template is more general then function-pointer type
    
    printf("lambda-callback by template       :"); dotest(calc1,lambda_f);    //54
    printf("lambda-callback by std::function  :"); dotest(calc2,lambda_f);    //1064   **** pass lambda as std::function object is 10x slower than using template *****
    //dotest(calc3,lambda_f);    //135    lambda function can only convert to function pointer if no captures in there

    printf("functor-callback by template      :");dotest(calc1,functor_f);    //55
    printf("functor-callback by std::function :");dotest(calc2,functor_f);    //1066   **** do not pass functor as std::function object! *****
    //dotest(calc3,functor_f);  //       **** functor cannot convert to function pointer
    
    
}

