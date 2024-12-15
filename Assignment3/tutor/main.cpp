#include <stdio.h>
#include <iostream>
#include "cuda/foo.cuh"

int main()
{
    std::cout<<"Hello C++"<<std::endl;
    useCUDA();
    return 0;
}
