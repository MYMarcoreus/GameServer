#include <iostream>
#include "gameprotocol.pb.h"

using namespace std;



void fun()
{

}

int main()
{
    // fun2();
    yy::server::GameProtocol::SecurityBody sb{};
    google::protobuf::Message * message = &sb;
    // cout <<  << endl;


    return 0;
}