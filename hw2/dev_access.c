#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

int main(int argc,char *argv[])
{
    /* Error checking code taken directly from the man page for strtol and adapted */
    char *endptr;
    long val;

    /* Error if more than one arguement*/

    if (argc != 2) 
    {
        fprintf(stderr, "Incorrect number of arguments\n");
        fprintf(stderr, "Usage: %s [0-2]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;    /* To distinguish success/failure after call */
    val = strtol(argv[1], &endptr, 10);

    /* Check for various possible errors */

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if argument is not 0, 1, or 2*/

    if (endptr == argv[1] || *endptr != '\0' || val < 0 || val > 2) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s [0-2]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Correct usage so perform actions*/

    if(val == 0)
    {
	int fd = open("/dev/input/mouse0",O_RDONLY);
	char ch = 'A';
	while(1)
	{	 
	    int i = read(fd,&ch,1);
	    if(i == 1)
		printf("%i\n",(int)ch);
	}
	close(fd);
    }
    else if(val == 1)
    {
	struct timeval tv1;
	struct timeval tv2;
	int fd_in = open("/dev/urandom",O_RDONLY);
	int fd_out = open("/dev/null",O_WRONLY);
	int count = 10000000;
	char *buf = malloc(10000000);
	gettimeofday(&tv1,NULL);
	while(count > 0)
	{
	    int i = read(fd_in,buf,count);
	    count = count - i;
	    i = write(fd_out,buf,i);
	}
	gettimeofday(&tv2,NULL);
	long int seconds = tv2.tv_sec-tv1.tv_sec;
	long int useconds = tv2.tv_usec-tv1.tv_usec;
	if(useconds < 0)
	{
	    seconds = seconds - 1;
	    useconds = -useconds;
	}
	printf("%ld.%ld seconds elapsed\n",seconds, useconds);
	close(fd_in);
	close(fd_out);
    }
    else if (val == 2)
    {
	printf("use ticket\n");
	int fd = open("/dev/ticket0",O_RDONLY);
	int i = 0;
	char buf[4];
	for(i=0;i<10;++i)
	{
	    int i = read(fd,buf,4);
	    if(i != 4)
		printf("error on ticket read\n");
	    else
		printf("ticket %d\n",(int)buf);
	}
    }

    exit(EXIT_SUCCESS);
}


