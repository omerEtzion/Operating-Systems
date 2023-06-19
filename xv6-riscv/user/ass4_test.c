#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void seektest(void)
{
	// create file
	int fd = open("a.txt", O_RDWR | O_CREATE);
	if (fd < 0)
	{
		printf("open() failed\n");
		exit(1);
	}

	char *str = "0123456789";
	write(fd, (void *)str, 10);

	char buff[2];
	buff[1] = '\0';

	// test SEEK_SET
	if (seek(fd, 3, SEEK_SET) < 0)
	{
		printf("seek(fd, 3, SEEK_SET) failed\n");
		exit(1);
	}

	read(fd, buff, 1);
	if (buff[0] == '3')
	{
		printf("SEEK_SET test passed\n");
	}
	else
	{
		printf("SEEK_SET test failed\n");
		exit(1);
	}

	// test SEEK_CUR
	if (seek(fd, 3, SEEK_CUR) < 0)
	{
		printf("seek(fd, 3, SEEK_CUR) failed\n");
		exit(1);
	}

	read(fd, buff, 1);
	if (buff[0] == '7')
	{
		printf("SEEK_CUR test passed\n");
	}
	else
	{
		printf("SEEK_CUR test failed\n");
		exit(1);
	}

	// test seeking before start of file
	if (seek(fd, -12, SEEK_CUR) < 0)
	{
		printf("seek(fd, -12, SEEK_CUR) failed\n");
		exit(1);
	}

	read(fd, buff, 1);
	if (buff[0] == '0')
	{
		printf("seeking before start of file test passed\n");
	}
	else
	{
		printf("seeking before start of file test failed\n");
		exit(1);
	}

	// test seeking after end of file
	if (seek(fd, 13, SEEK_CUR) < 0)
	{
		printf("seek(fd, 5, SEEK_CUR) failed\n");
		exit(1);
	}

	int n = read(fd, buff, 1);
	if (n == 0)
	{
		printf("seeking after end of file test passed\n");
	}
	else
	{
		printf("seeking after end of file test failed\n");
		exit(1);
	}

	printf("ALL SEEK TESTS PASSED\n");
	close(fd);
}

void randomtest(void)
{
	char buff[256];

	int fd = open("random", O_RDWR);

	// repeated chars test
	int n = read(fd, &buff, 255);
	if (n < 255)
	{
		printf("read failed: wanted %d bytes, read %d bytes\n", 255, n);
		exit(1);
	}

	int i = 0;
	while (i < n)
	{
		int j = i + 1;
		while (j < n)
		{
			if (buff[i] == buff[j])
			{
				printf("repeated char %x at indexes %d and %d\n", buff[i], i, j);
				exit(1);
			}
			j++;
		}
		i++;
	}
	printf("repeated char test passed\n");

	// period test
	read(fd, &buff[255], 1);
	if (buff[255] != buff[0])
	{
		printf("wrong period of random number generator\n");
		exit(1);
	}
	printf("period test passed\n");

	// write test
	char c = 0x1;
	if (write(fd, &c, 1) < 0)
	{
		printf("failed to write\n");
	}
	read(fd, buff, 256);
	if (buff[255] != c)
	{
		printf("write did not repeat, expected %x, got %x\n", c, buff[255]);
		exit(1);
	}
	printf("write test passed\n");

	// concurrency test
	c = 0x2A;
	write(fd, &c, 1);

	int fd1 = open("concurrency_test.txt", O_RDWR | O_CREATE);

	int pids[5];
	int pid;

	for(int i = 0; i < 5; i++) {
		// create 5 threads that concurrently read from the device
		pid = fork();
		if(pid == 0) {
			char buff1[51];
			read(fd, &buff1, 51);

			write(fd1, &buff1, 51);

			exit(0);
		} else {
			pids[i] = pid;
		}
	}

	for(int i = 0; i < 5; i++) {
		// wait for them to finish
		wait(&pids[i]);
	}

	char buffs[5][51];
	seek(fd1, 0, SEEK_SET);
	
	for(int i = 0; i < 5; i++) {
		read(fd1, &buffs[i], 51);
	}

	for(int i = 0; i < 5; i++) {
		for(int j = i+1; j < 5; j++) {
			for(int k = 0; k < 51; k++) {
				for(int l = k+1; l < 51; l++) {
					if(buffs[i][k] == buffs[j][l]) {
						printf("repeated char %x at index %d of buff %d and index %d of buff %d\n", buffs[i][k], k, i, l, j);
						exit(1);
					}
				}
			}
		}
	}

	printf("concurrency test passed\n");

	printf("ALL RANDOM TESTS PASSED\n");
	close(fd1);
	close(fd);
}

void main(void)
{
	seektest();
	printf("\n\n");
	randomtest();
	printf("\n\n");

	printf("ALL TESTS PASSED\n");
	exit(0);
}

