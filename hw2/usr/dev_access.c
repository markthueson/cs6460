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

    if (endptr == argv[1] || *endptr != '\0' || val < 0 || val > 3) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s [0-2]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Correct usage so perform actions*/

    if(val == 0)
    {
	int fd = open("/dev/input/mouse0",O_RDONLY);
	if(fd < 0)
	{
	    fprintf(stderr, "Could not open mouse0\n");
	    exit(EXIT_FAILURE);
	}
	char ch = 'A';
	while(1)
	{	 
	    int i = read(fd,&ch,1);
	    if(i == 1)
		printf("%i\n",(int)ch);
	    else
	    {
		fprintf(stderr, "Error reading from mouse0\n");
		exit(EXIT_FAILURE);
	    }
	}
	int retval = close(fd);
	if(retval != 0)
	{
	    fprintf(stderr, "Error closing mouse0\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if(val == 1)
    {
	struct timeval tv1;
	struct timeval tv2;
	int fd_in = open("/dev/urandom",O_RDONLY);
	if(fd_in < 0)
	{
	    fprintf(stderr, "Could not open urandom\n");
	    exit(EXIT_FAILURE);
	}
	int fd_out = open("/dev/null",O_WRONLY);
	if(fd_out < 0)
	{
	    fprintf(stderr, "Could not open null\n");
	    exit(EXIT_FAILURE);
	}
	int count = 10000000;
	char *buf = malloc(10000000);
	if(buf == NULL)
	{
	    fprintf(stderr, "Could not allocate buffer\n");
	    exit(EXIT_FAILURE);
	}
	gettimeofday(&tv1,NULL);
	while(count > 0)
	{
	    int i = read(fd_in,buf,count);
	    if(i < 0)
	    {
		fprintf(stderr, "Error reading from urandom\n");
		exit(EXIT_FAILURE);
	    }

	    count = count - i;
	    i = write(fd_out,buf,i);
	    if(i < 0)	    
	    {
		fprintf(stderr, "Error writing to null\n");
		exit(EXIT_FAILURE);
	    }
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
	int retval = close(fd_in);
	if(retval != 0)
	{
	    fprintf(stderr, "Error closing mouse0\n");
	    exit(EXIT_FAILURE);
	}
	retval = close(fd_out);
	if(retval != 0)
	{
	    fprintf(stderr, "Error closing mouse0\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if (val == 2)
    {
	int fd = open("/dev/ticket0",O_RDONLY);
	if(fd < 0)
	{
	    fprintf(stderr, "Could not open ticket0\n");
	    exit(EXIT_FAILURE);
	}
	int i = 0;
	unsigned char buf[4];
	for(i=0;i<10;++i)
	{
	    int j = read(fd,buf,4);
	    if(j != 4)
	    {
		fprintf(stderr,"Error reading from ticket0\n");
		exit(EXIT_FAILURE);
	    }
	    else
	    {
		unsigned int ticket = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
		printf("ticket is %u\n",ticket);
		//printf("ticket %x\n",(int)*buf);
		//printf("ticket %d unsigned %u\n",(int)*buf,(int)*buf);
	    }
	    sleep(1);
	}
	int retval = close(fd);
	if(retval < 0)
	{
	    fprintf(stderr,"Could not close ticket0\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if (val == 3)
    {
	unsigned int i = 1000;
	unsigned char buf[4];
	buf[0] = (i >> 24) & 0xFF;
	buf[1] = (i >> 16) & 0xFF;
	buf[2] = (i >> 8) & 0xFF;
	buf[3] = i & 0xFF;
	printf("%02x%02x%02x%02x\n",buf[0],buf[1],buf[2],buf[3]);
	unsigned int j = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	printf("j is %u hex %x\n",j,j);
    }

    exit(EXIT_SUCCESS);
}


