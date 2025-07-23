#include "yaml-cpp/yaml.h"
#include <iostream>
#include <sstream>

using namespace std;


int main()
{
    YAML::Node root = YAML::LoadFile("../test/test.yaml");
    std::stringstream ss;
    ss << root;
//    cout << ss.str();

    auto node = YAML::Load(ss.str());
    cout << node << endl;
}