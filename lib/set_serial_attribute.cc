#include "set_serial_attribute.hpp"
#include <string.h>
#include <sys/ioctl.h>

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
    return read(fd,buffer,size);
}

void serialPort::ClosePort() {
    close(fd);
}