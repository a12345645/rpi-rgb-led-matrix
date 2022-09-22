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
#define LORA_PACKET_lEN 10
#define LORA_HEADER_1 0xab
#define LORA_HEADER_2 0x0a

class serialPort{
public:
    serialPort();
    bool OpenPort(const char* dev);
    int setup(int speed, int parity);
    int readBuffer(uint8_t *buffer,int size);
    void ClosePort();
    uint8_t tmi = 0;
private:
    int fd;
    struct termios opt;
    uint8_t tmp[256];
    int tmp_size = 0;

    uint8_t general_cks(uint8_t *data, int len);
};

#endif