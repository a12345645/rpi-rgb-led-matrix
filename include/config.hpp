#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <sstream>
#include <fstream>
#include <string>
#include "typedefine.hpp"

#define CONFIG_FILE_PATH "config/config.txt"

using namespace std;

class Config {
public:
    Config();
    bool OpenConfig(const char* path);
    void CloseConfig();
    void ReadConfig();
    int RTM_return();
private:
    config_object_t config;
    string skip,line;
    ifstream fop;
};

#endif