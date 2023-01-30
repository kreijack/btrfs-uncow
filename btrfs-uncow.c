#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

const size_t block_size = 1024*1024*1024;
const size_t copy_size = 32*1024*1024;
void *copy_buffer;

int copy(int dst, int src, size_t len)
{
	while (len > 0) {
		ssize_t in = read(src, copy_buffer, copy_size);
		//printf("in=%d", in);
		if (in < 0) {
			perror("read src");
			exit(EXIT_FAILURE);
		}
		else if (in == 0)
			return 0;

		ssize_t	out = write(dst, copy_buffer, in);
		//printf("out=%d", out);
		if (out < 0) {
			perror("write dst");
			exit(EXIT_FAILURE);
		}
		len -= out;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "%s <src> <dst>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int src, dst;
	if ((src = open(argv[1], O_DSYNC|O_RDWR)) == -1) {
		fprintf(stderr, "Failed to open source '%s': ", argv[1]);
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	// copy mode from src?
	if ((dst = open(argv[2], O_CREAT|O_DSYNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
		fprintf(stderr, "Failed to open destination '%s': ", argv[2]);
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	if ((copy_buffer = malloc(copy_size)) == NULL) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	off_t pos;
	if ((pos = lseek(dst, 0, SEEK_END)) == -1) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	if (pos == 0) {
		int attr = FS_NOCOW_FL;
		if (ioctl(dst, FS_IOC_SETFLAGS, &attr) == -1) {
			fprintf(stderr, "Failed to set NoCoW flag on '%s': ", argv[2]);
			perror(NULL);
			//exit(EXIT_FAILURE);
		}

		if (ftruncate(dst, lseek(src, 0, SEEK_END)) == -1) {
			perror("truncate dst");
			exit(EXIT_FAILURE);
		}
		printf("Created new file '%s'\n", argv[2]);
	}
	else {
		printf("Continuing with existing file '%s'\n", argv[2]);
	}

	if ((pos = lseek(src, 0, SEEK_END)) == -1) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	while (pos > 0) {
		pos = block_size > pos ? 0 : pos - block_size;
		printf("Copying from position %ld...   \n", pos);

		if (lseek(src, pos, SEEK_SET) == -1) {
			perror("seek src");
			exit(EXIT_FAILURE);
		}

		if (lseek(dst, pos, SEEK_SET) == -1) {
			perror("seek dst");
			exit(EXIT_FAILURE);
		}

		copy(dst, src, block_size);

		if (ftruncate(src, pos) == -1) {
			perror("truncate src");
			exit(EXIT_FAILURE);
		}
	}

	printf("Copying done! Removing '%s'.\n", argv[1]);
	remove(argv[1]);
	exit(EXIT_SUCCESS);
}