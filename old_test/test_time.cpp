#include <iostream>
#include "Timestamp.h"

using yy::net::Timestamp;


using namespace std;
int main()
{
    Timestamp timestamp{0};
    timestamp.SetUnixTime(1696172759s, 745532us);
    cout << timestamp.ToString() << '\n';
    cout << timestamp.ToFormattedString() << '\n';

    timestamp.SetNow();

    cout << timestamp.ToString() << '\n';
    cout << timestamp.ToFormattedString() << '\n';

    return 0;
}