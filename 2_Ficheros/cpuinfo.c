#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <string.h>

#define BUFSIZE 512

#ifndef SYS_read 
#define SYS_read          0 
#endif


int main(void)
{
    int read_chars=0;
    int fd=0, fderr = 2, fdout = 1;
    char buf[BUFSIZE+1];
    
    /* Open /proc entry in read-only mode */
    fd=syscall(SYS_openat,AT_FDCWD,"/proc/cpuinfo",O_RDONLY,0);
    
    if (fd<0){
        char* err = "Can't open the file\n";
        syscall(SYS_write,fderr,err,strlen(err));
        exit(1);
    }
    
    /* Loop that reads data from the file and prints its contents to stdout */
    while((read_chars=syscall(SYS_read,fd,buf,BUFSIZE))>0){
        buf[read_chars]='\0';
        syscall(SYS_write,fdout,buf,read_chars); 
    }
     
    if (read_chars<0){
        char* err = "Error while reading the file\n";
        syscall(SYS_write,fderr,err,strlen(err));
        exit(1);        
    }
    
    	
    /* Close the file and exit */ 
    syscall(SYS_close,fd);
    return 0;
}
