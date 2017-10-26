#include <cstdio>
#include <iostream>
#include  <algorithm>

template <typename T>
class ListItem
{
public:
	ListItem(const T & v, ListItem * pnext = NULL):
		value(v),
		next(pnext)
	{}
	
	T 			 value;
	ListItem	* next;
	
	bool operator==(const T & v){ return value == v; }
};


template <typename Item>
struct ListIter
{
	//===================================================================
    // Iterator traits - typedefs and types required to be STL compliant
    typedef std::ptrdiff_t     difference_type;
    typedef Item               value_type;
    typedef Item*              pointer;
    typedef Item&              reference;
    typedef std::forward_iterator_tag iterator_category;
    //===================================================================

	Item* ptr;
	
	//default a (null) Iterator (with ptr=0) is constructed
	ListIter(Item *p=0) : ptr(p) {}
	
	Item& operator*() const { return *ptr; }
	Item* operator->() const { return ptr; }
	// (++a)
	ListIter& operator++(){ ptr = ptr->next; return *this;} 
	// (a++)
	ListIter operator++(int){
		ListIter temp(*this);
		ptr = ptr->next; 
		return temp;
	}
	// Iterator only compare with Iterator
	bool operator==(const ListIter &i) const { return ptr == i.ptr; }
	bool operator!=(const ListIter &i) const { return ptr != i.ptr; }
};



/*
namespace std {
    template <typename Item>
    struct iterator_traits<ListIter<Item> > {
        typedef ptrdiff_t          difference_type;
        typedef Item               value_type;
        typedef Item*              pointer;
        typedef Item&              reference;
        typedef forward_iterator_tag iterator_category;
    };
}
*/


template <typename T>
class List
{
public:
	List():_front(NULL), _end(NULL) {}
	
	void insert_after(ListItem<T> * ele, const T &v){
		ListItem<T> *newele = new ListItem<T>(v, ele->next);
		ele->next = newele;
	}
	
	void insert_front(const T &v){
		ListItem<T> *newele = new ListItem<T>(v, _front);
		_front = newele;
	}

	ListIter<ListItem<T> > begin(){
		return ListIter<ListItem<T> >(_front);
	}
	ListIter<ListItem<T> > end(){
		return ListIter<ListItem<T> >(0);
	}
	
	void show(std::ostream &os = std::cout) const{
		ListItem<T> * p = _front;
		os << "List:" << std::endl;
		while(p){
			os << "[" << p->value << "]-->";
			p = p->next;
		}
		os << "(end)" << std::endl;
	}
private:
	ListItem<T> * _front;
	ListItem<T> * _end;
};





int main()
{
	List<int> l;
	l.insert_front(5);
	l.insert_front(4);
	l.insert_front(3);	
	l.insert_front(2);	
	l.insert_front(1);	
	l.insert_front(0);
	
	l.show();
	
	ListIter<ListItem<int> > iter;
	
	for(int i=-10;i<10;i++)
	{
		iter = std::find(l.begin(), l.end(), i);
		if (iter == l.end())
			std::cout<< "not found " << i  << std::endl;
		else
			std::cout<< "found " << i << std::endl;
	}
}


