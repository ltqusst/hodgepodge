#include <cstdio>
#include <iostream>
#include  <algorithm>

template <typename Item>
struct Iter
{
	//--------------------------------------------------------------------
    // Iterator traits - typedefs and types required to be STL compliant
    typedef std::ptrdiff_t          difference_type;
    typedef Item               value_type;
    typedef Item*              pointer;
    typedef Item&              reference;
    typedef std::forward_iterator_tag iterator_category;
    //--------------------------------------------------------------------

	Item* ptr;
	Iter(Item *p=0) : ptr(p) {} //default a (null) Iterator (with ptr=0) is constructed
	Item& operator*() const { return *ptr; }
	
	//Item* operator->() const { return ptr; }
	
	// (++a)
	Iter& operator++(){
		ptr++; 
		return *this;
	} 
	
	// (a++)
	/*
	Iter operator++(int){
		Iter temp(*this);
		ptr++; 
		return temp;
	}
	*/
	
	// Iterator only compare with Iterator
	//bool operator==(const Iter &i) const { return ptr == i.ptr; }
	bool operator!=(const Iter &i) const { return ptr != i.ptr; }
};


/*
namespace std {
    template <typename Item>
    struct iterator_traits<Iter<Item> > {
        typedef ptrdiff_t          difference_type;
        typedef Item               value_type;
        typedef Item*              pointer;
        typedef Item&              reference;
        typedef forward_iterator_tag iterator_category;
    };
};
*/



class X
{
public:
	typedef int trait_001;
};

template<class T>
struct my_traits
{
	typedef typename T::trait_001 type001;
};

template<>
struct my_traits<int>
{
	typedef int type001;
};



template<class T>
typename my_traits<T>::type001 func()
{
	return 1;
}

int main()
{
	int a[9]={1,2,3,4,5,6,7,-1,-2};

	std::cout << "func() = " <<	func<int>() << std::endl;
	
	Iter<int> begin(a);
	Iter<int> end(a+9);
		
	for(int i=-10;i<10;i++)
	{
		Iter<int> iter = std::find(begin, end, i);
		if (iter != end)
			std::cout<< "found " << i << std::endl;
		else
			std::cout<< "not found " << i  << std::endl;
	}
}


