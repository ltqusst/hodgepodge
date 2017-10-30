#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

class Person
{
public:
    Person(std::string n, int i):name(n),id(i){}
    std::string name;
    int id;
};

int main()
{
    std::vector<Person> vall;
    
    vall.push_back(Person("Li",1));
    vall.push_back(Person("Wang",2));
    vall.push_back(Person("Qin",3));
    vall.push_back(Person("Zheng",2));
    vall.push_back(Person("Qiao",5));
    
    vall.erase(std::remove_if(vall.begin(), vall.end(), [](const Person &p){ return p.id == 2; }),
               vall.end());
    
    auto print = [](const Person& n) { std::cout << " " << n.name << ":" << n.id << std::endl; };
    
    std::for_each(vall.begin(), vall.end(), print);
}

