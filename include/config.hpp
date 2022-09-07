#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <sstream>
#include <fstream>
#include <string>

#define CONFIG_FILE_PATH "config/config.txt"

using namespace std;

class Config {
private:
    typedef struct config_object {
        int RTM;
        string green;
        string red;
        string warning;
    }config_object_t;
public:
    Config(const string path);
    ~Config();
    Config::config_object_t* config;
};

#endif