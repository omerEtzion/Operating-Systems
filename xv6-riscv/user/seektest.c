#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
	// create file
	int fd = open("a.txt", O_RDWR | O_CREATE);
	if(fd < 0) {
		printf("open() failed\n");
		exit(1);
	}

	char* str = "0123456789";
	int n = write(fd, (void*) str, 10);
	printf("wrote %d bytes to file\n", n);

	struct stat st;
	fstat(fd, &st);
	printf("file size: %d\n", st.size);

  char buff[2];
  buff[1] = '\0';

  // test SEEK_SET
	if(seek(fd, 3, SEEK_SET) < 0) {
    printf("seek(fd, 3, SEEK_SET) failed\n");
    exit(1);
  }

  read(fd, buff, 1);
	if(buff[0] == '3') {
		printf("SEEK_SET test passed\n");
	} else {
		printf("SEEK_SET test failed\n");
		exit(1);
	}
  // printf("expecting to see 3, got %s\n", buff);
	
	// test SEEK_CURR
  if(seek(fd, 3, SEEK_CURR) < 0) {
    printf("seek(fd, 3, SEEK_CURR) failed\n");
    exit(1);
  }

	read(fd, buff, 1);
  if(buff[0] == '7') {
		printf("SEEK_CURR test passed\n");
	} else {
		printf("SEEK_CURR test failed\n");
		exit(1);
	}
  // printf("expecting to see 7, got %s\n", buff);

	// test seeking before start of file
	if(seek(fd, -12, SEEK_CURR) < 0) {
    printf("seek(fd, -12, SEEK_CURR) failed\n");
    exit(1);
  }

	read(fd, buff, 1);
  if(buff[0] == '0') {
		printf("seeking before start of file test passed\n");
	} else {
		printf("seeking before start of file test failed\n");
		exit(1);
	}
  // printf("expecting to see 0, got %s\n", buff);

	// test seeking after end of file
	if(seek(fd, 13, SEEK_CURR) < 0) {
    printf("seek(fd, 5, SEEK_CURR) failed\n");
    exit(1);
  }
	
	n = read(fd, buff, 1);
  if(n == 0) {
		printf("seeking after end of file test passed\n");
	} else {
		printf("seeking after end of file test failed\n");
		exit(1);
	}
  // printf("expecting to read 0 bytes, got %d bytes\n", n);

	printf("ALL TESTS PASSED\n");
	exit(0);
}