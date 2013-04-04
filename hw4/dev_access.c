#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

int main(int argc,char *argv[])
{
    long val = 0;
    char *endptr;

    /* Correct usage so perform actions*/

    if(val == 0)
    {

	int rw = strtol(argv[2],&endptr,10);
	int dev = strtol(argv[3],&endptr,10);
        char sleepydev[13];
        sprintf(sleepydev,"/dev/sleepy%d",dev);

	int fd;
	if(rw == 1)
	    fd = open(sleepydev,O_WRONLY);
	else
	    fd = open(sleepydev,O_RDONLY);
	if(fd < 0)
	{
	    fprintf(stderr, "Could not open sleepy%d\n",dev);
	    exit(EXIT_FAILURE);
	}
	int *buf = malloc(sizeof(int));
	*buf = (int)strtol(argv[1],&endptr,10);
	printf("buf is %d\n",*buf);
	int i;
	if(rw == 1)
	    i = write(fd,buf,sizeof(int));
	else
	    i = read(fd,buf,sizeof(int));
	    if(i == sizeof(int))
		printf("%i\n",i);
	    else
	    {
		fprintf(stderr, "Error wrtiing to sleepy%d got %d\n",dev,i);
		exit(EXIT_FAILURE);
	    }

	int retval = close(fd);
	if(retval != 0)
	{
	    fprintf(stderr, "Error closing sleepy%d\n",dev);
	    exit(EXIT_FAILURE);
	}
    }

    exit(EXIT_SUCCESS);
}


