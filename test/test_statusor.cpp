#include "status/StatusOr.hpp"
#include<iostream>

using namespace yy::util;
using namespace std;

int main()
{
    StatusOr<int> S ;

    if( isOk(S) ) {
        auto && val = getValue(S);
        cout << val;
    } else {
        auto && status = getStatus(S);
        cout << status.ToString() << endl;
    }
}