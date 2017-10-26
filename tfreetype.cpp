#include <cstdio>
#include "ft2wrap.h"

ft2wrap ft2lib;

int main()
{
    ft2lib.init(NULL);
    ft2lib.selfTest();
}

