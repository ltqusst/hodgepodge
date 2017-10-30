
#include <iostream>
#include <chrono>

template<class V, class R>
std::ostream& operator<<(std::ostream& s, const std::chrono::duration<V,R> &d)
{
    s << "[" << d.count() << " of " << R::num << "/" << R::den << " seconds]";
}


template<class C>
void printClockData()
{
    std::cout << "precision: " << C::period::num << "/" << double(C::period::den) << std::endl;
}


std::string asString(const std::chrono::system_clock::time_point& tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::string ts = std::ctime(&t);
    ts.resize(ts.size()-1);
    return ts;
}

/* separate ticks & units(the ratio, period)
   so larger range can be represented by lifting the period
 */
typedef std::chrono::duration<double, std::ratio<3600*24*365,1> > Year365;
int main()
{
    std::chrono::seconds    sec20(20);
    std::chrono::hours      aDay(24);
    std::chrono::hours      aWeek(24*7);
    
    std::cout << "aDay:" << aDay << std::endl;
    std::cout << "sec20:" << sec20 << std::endl;
    
    std::cout << "++aDay:" << ++aDay<< std::endl;
    std::cout << "++sec20:" << ++sec20 << std::endl;
    
    std::cout << "aDay:" << aDay << std::endl;
    std::cout << "sec20:" << sec20 << std::endl;

    std::cout << "sec20 + aDay:" << sec20 + aDay << std::endl;
    std::cout << "aDay + sec20:" << aDay + sec20 << std::endl;
    
    std::cout << "aDay.max() " << aDay.max() << std::endl;
    std::cout << "sec20.max()" << sec20.max() << std::endl;
    
    std::cout << "max years chrono::hours can represent: " << std::chrono::duration_cast<Year365>(std::chrono::hours(aDay.max())).count() << std::endl;
    std::cout << "max years chrono::seconds can represent: " << std::chrono::duration_cast<Year365>(std::chrono::seconds(sec20.max())).count() << std::endl;
    
    std::cout << "system_clock         :";
    printClockData<std::chrono::system_clock>();
    std::cout << "high_resolution_clock:";
    printClockData<std::chrono::high_resolution_clock>();
    std::cout << "steady_clock         :";
    printClockData<std::chrono::steady_clock>();
    
    std::chrono::system_clock::time_point tp;
    std::cout << "Epoch:" << asString(tp) << std::endl;
    
    tp = std::chrono::system_clock::now();
    std::cout << "Now  :" << asString(tp) << std::endl;
    tp = std::chrono::system_clock::time_point::min();
    std::cout << "Min  :" << asString(tp) << std::endl;
    tp = std::chrono::system_clock::time_point::max();
    std::cout << "Max  :" << asString(tp) << std::endl;
    
    //std::cout << "aDay:" << aDay.count() << " x " << decltype(aDay)::period::num << "/" << decltype(aDay)::period::den << std::endl;
}

