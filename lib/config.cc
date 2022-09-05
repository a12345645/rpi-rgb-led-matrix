#include "config.hpp"

Config::Config(){}

bool Config::OpenConfig(const char* path) {
    fop.open(path,ios::in);
    if(!fop) {
        return false;
    } else {
        return true;
    }
}

void Config::CloseConfig() {
    fop.close();
}

void Config::ReadConfig() {
    while(getline(fop,line)) {
        if(line[0] == '#') {
            continue;
        } else {
            istringstream iss(line);
            iss >> skip >> config.RTM; 
        }
    }
}

int Config::RTM_return() {
    return config.RTM;
}