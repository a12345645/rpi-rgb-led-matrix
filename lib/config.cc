#include "config.hpp"
#include<iostream>
#include <strstream>

Config::Config(const string path) {
    ifstream fop;
    fop.open(path,ios::in);
    if(!fop) {
        return;
    }
    config = new config_object();
    std::string line;
    while( std::getline(fop, line) )
    {
        if(line[0] == '#')
            continue;
        std::istringstream is_line(line);
        std::string key;
        if( std::getline(is_line, key, '=') )
        {
            std::string value;
            if( std::getline(is_line, value) ) {
                if (key.compare("TM") == 0) {
                    stringstream ss;
                    ss << value;
                    ss >> config->RTM;
                } else if (key.compare("green") == 0) {
                    config->green = value;
                } else if (key.compare("red") == 0) {
                    config->red = value;
                } else if (key.compare("warning") == 0) {
                    config->warning = value;
                }
            }
        }
    }
}


Config::~Config() {
    if (config != nullptr)
        delete config;
}