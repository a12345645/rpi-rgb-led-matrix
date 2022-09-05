#ifndef SET_SERIAL_ATTRIBUTE_HPP
#define SET_SERIAL_ATTRIBUTE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>
using namespace std;

#define PORT_FD "/dev/ttyUSB0"

class serialPort{
public:
    serialPort();
    bool OpenPort(const char* dev);
    int setup(int speed, int parity);
    int readBuffer(uint8_t *buffer,int size);
    void ClosePort();
private:
    int fd;
    struct termios opt;
};

#endif