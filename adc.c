#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "wiringPi.h"

#define odAinNode0 "/sys/class/saradc/saradc_ch0"
#define odAinNode1 "/sys/class/saradc/saradc_ch1"

static int adcFds [2] = {
    -1, -1,
};

int analogRead (int pin) {
    struct wiringPiNodeStruct *node = wiringPiNodes;
    unsigned char value[5] = {0,};

    if(pin < 2)   {
        if (adcFds [pin] == -1)
            return 0;
        lseek (adcFds [pin], 0L, SEEK_SET);
        read  (adcFds [pin], &value[0], 4);
        return  atoi(value);
    }

    if ((node = wiringPiFindNode (pin)) == NULL)
        return 0;
    else
        return node->analogRead (node, pin);
}


int main(int argc, char *argv[]) {
    adcFds[0] = open(piAinNode0, O_RDONLY);
    adcFds[1] = open(piAinNode1, O_RDONLY);


    return 0;
}
