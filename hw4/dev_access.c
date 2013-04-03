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
	int fd = open("/dev/sleepy0",O_WRONLY);
	if(fd < 0)
	{
	    fprintf(stderr, "Could not open sleepy0\n");
	    exit(EXIT_FAILURE);
	}
	int *buf = malloc(sizeof(int));
	*buf = (int)strtol(argv[1],&endptr,10);
	printf("buf is %d\n",*buf);
	    int i = write(fd,buf,sizeof(int));
	    if(i == sizeof(int))
		printf("%i\n",i);
	    else
	    {
		fprintf(stderr, "Error wrtiing to sleepy0 got %d\n",i);
		exit(EXIT_FAILURE);
	    }

	int retval = close(fd);
	if(retval != 0)
	{
	    fprintf(stderr, "Error closing sleepy0\n");
	    exit(EXIT_FAILURE);
	}
    }

    exit(EXIT_SUCCESS);
}


