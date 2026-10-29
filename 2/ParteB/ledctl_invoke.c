#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#ifndef SYS_ledctl 
#define SYS_ledctl 451
#endif

int main(int argc, char *argv[])
{
    int led;
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <ledmask>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if((sscanf(argv[1], "%i", &led) != 1) || led > 0x7 || led < 0){
        fprintf(stderr, "ledctl failed: Invalid argument\n");
        exit(EXIT_FAILURE);
    }
    if(syscall(SYS_ledctl,led) < 0){
        fprintf(stderr, "ledctl failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
