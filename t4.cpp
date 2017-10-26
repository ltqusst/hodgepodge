
#include <cstdio>


class copyable
{
public:
	copyable(){printf(">>>> copyable::default constructor\n");}
	copyable(const copyable & c){printf(">>>> copyable::copy constructor\n");}
};

class foo: public copyable
{
private:
	//copyable c;
	int c;
};

int main()
{
	foo f;
	foo f2 = f;
}

