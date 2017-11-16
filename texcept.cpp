#include <iostream>
#include <stdexcept>

void myerr(const std::string &err)
{
    std::cout << err << std::endl;
}

int main()
{
    myerr("Error" + std::to_string(1) + "." + std::to_string(2));
    
    throw std::runtime_error(__FUNCTION__":" + std::to_string(__LINE__) + "Error" + std::to_string(100));
}
