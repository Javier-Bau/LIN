#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>

#ifndef SYS_ledctl 
#define SYS_ledctl 451
#endif

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(stderr, "Usage: ./%s <ledmask>\n", argv[0]);
        exit(1);
    }
    if(syscall(SYS_ledctl,atoi(argv[1])) < 0){
        fprintf(stderr, "ledctl failed\n");
        exit(1);
    }
    return 0;
}
