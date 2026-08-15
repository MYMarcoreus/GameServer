#include "SFINAE.h"

using namespace yy::util;

int main() {
    std::cout << std::boolalpha;

    std::vector<int> vec;
    std::list<double> lst;
    int arr[5];
    int integer;

    std::cout << "Is vector a container? " << is_iterable_container_v<decltype(vec)>     << std::endl;
    std::cout << "Is list a container? "   << is_iterable_container_v<decltype(lst)>     << std::endl;
    std::cout << "Is array a container? "  << is_iterable_container_v<decltype(arr)>     << std::endl;
    std::cout << "Is int a container? "    << is_iterable_container_v<decltype(integer)> << std::endl;




    return 0;
}