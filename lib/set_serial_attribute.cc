#include "set_serial_attribute.hpp"
#include <string.h>
#include <sys/ioctl.h>
#include <algorithm>

using namespace std;

serialPort::serialPort() {
    fd = -1;
}

bool serialPort::OpenPort(const char* dev) {
    fd = open(dev,O_RDWR);
    if (fd == -1) {
        perror("Can'y Open Serial Port");
        return false;
    }
    int DTR_flag;
    DTR_flag = TIOCM_DTR;
    ioctl(fd,TIOCMBIS,&DTR_flag);
    
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return true;
}

int serialPort::setup(int speed, int parity) {
    memset(&opt, 0, sizeof(opt));
    if (tcgetattr(fd, &opt) != 0) {
        printf("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&opt, speed);
    cfsetispeed(&opt, speed);

    opt.c_cflag = (opt.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    opt.c_iflag &= ~IGNBRK;  // disable break processing
    opt.c_lflag = 0;         // no signaling chars, no echo,
                             // no canonical processing
    opt.c_oflag = 0;         // no remapping, no delays
    opt.c_cc[VMIN] = 0;      // read doesn't block
    opt.c_cc[VTIME] = 5;     // 0.5 seconds read timeout

    opt.c_iflag &= ~(IXON | IXOFF | IXANY);  // shut off xon/xoff ctrl

    opt.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls,
                                        // enable reading
    opt.c_cflag &= ~(PARENB | PARODD);  // shut off parity
    opt.c_cflag |= parity;
    opt.c_cflag &= ~CSTOPB;
    opt.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &opt) != 0) {
        printf("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}

int serialPort::readBuffer(uint8_t* buffer,int size) {    
    // return read(fd, buffer, size);
    int s;
    do
    {
        s = read(fd, &tmp, sizeof(tmp));
    } while (s == sizeof(tmp));
    
    if (s >= 0) {
        for (int i = 0; i < s - 1; i++) {
            if (tmp[i] == LORA_HEADER_1 && tmp[i + 1] == LORA_HEADER_2 && (s - 1) >= LORA_PACKET_lEN) {
                if ((tmp[i + 2] == tmi ||  tmp[i + 2] == 0) && 
                    tmp[i + LORA_PACKET_lEN - 1] == general_cks(&tmp[i], LORA_PACKET_lEN - 1)) {
                    memcpy(buffer, &tmp[i], LORA_PACKET_lEN);
                    return LORA_PACKET_lEN;
                }
            }
        }
    }
    return -1;
}

void serialPort::ClosePort() {
    close(fd);
}

uint8_t serialPort::general_cks(uint8_t *data, int len)
{
    uint8_t sum = 0;

    for (int i = 0; i < len; i++)
        sum += data[i];
    return (int8_t) 0 - sum;
}