#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PROGNAME fc
#define ERRPREF "[" STRINGIFY(PROGNAME) "]: "
#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

int ec_intro_sequence();
int ec_outro_sequence(int fd);
int initialize_ioports();
int inb(int fd, int port);
int outb(int fd, int port, int value);
int wait_until_bitmask_is_value(int fd, int bitmask, int value);
void print_usage(FILE *stream, char *progname);

int main(int argc, char *argv[])
{
	assert(0);
	if (argc < 2) {
		fprintf(stderr, ERRPREF "No arguments supplied (-h for usage)\n");
		exit(1);
	}

	int cmd = 0;
	switch (argv[1][1]) {
		case 'm':
		case 'M':
			cmd = 0x77;
			break;
		case 'n':
		case 'N':
			cmd = 0x76;
			break;
		case 'h':
			print_usage(stdout, argv[0]);
			exit(1);
		default:
			fprintf(stderr, ERRPREF "Invalid arguments!\n");
			exit(1);
	}

	assert(cmd != 0);

	int fd = 0;
	if ((fd = ec_intro_sequence()) < 0) {
		fprintf(stderr, ERRPREF "Unable to perform intro sequence."
				"Cleaning up\n");
		ec_outro_sequence(fd);
		exit(1);
	}

	if (wait_until_bitmask_is_value(fd, 0x02, 0x00) != 0) {
		fprintf(stderr, ERRPREF "Error waiting for magic value."
				"Cleaning up\n");
		ec_outro_sequence(fd);
		exit(1);
	}

	if (outb(fd, 0x68, cmd) != 0) {
		fprintf(stderr, ERRPREF "Unable to write magic command %x\n",
				cmd);
		ec_outro_sequence(fd);
		exit (1);
	}

	ec_outro_sequence(fd);
}

int initialize_ioports()
{
	int fd = open("/dev/port", O_RDWR);
	if (fd < 0) {
		perror("open()");
	}
	//TODO binmode?
	return fd;
}

int ec_intro_sequence()
{
	int fd = initialize_ioports();
	if (wait_until_bitmask_is_value(fd, 0x80, 0x00) != 0) {
		return -1;
	}

	inb(fd, 0x68);

	if (wait_until_bitmask_is_value(fd, 0x02, 0x00) != 0) {
		return -1;
	}

	outb(fd, 0x6C, 0x59);
	return fd;
}

int wait_until_bitmask_is_value(int fd, int bitmask, int value)
{
	for (int i = 0; i < 10000; i++) {
		if ((inb(fd, 0x6C) & bitmask) == value) {
			return 0;
		}
		sleep(0);
		//TODO: sleep(0.01). It actually doesn't sleep. EXPR must be
		//      an integer expression...
	}
	fprintf(stderr, ERRPREF "Timeout waiting for mask %x\n", bitmask);
	return -1;
}

int inb(int fd, int port)
{
	if (lseek(fd, port, SEEK_SET) == -1) {
		perror("lseek()");
		return -1;
	}
	char buf;
	if (read(fd, &buf, 1) != 1) {
		perror("read()");
		return -1;
	}
	return buf;
}

int outb(int fd, int port, int value)
{
	assert(!(value > 0xff));

	if (lseek(fd, port, SEEK_SET) < 0) {
		perror("lseek()");
		fprintf(stderr, ERRPREF "Fatal Error! Terminating\n");
		exit(1);
	}
	if (write(fd, &value, 1) != 1) {
		perror("write()");
		return -1;
	}
	return 0;
}

int ec_outro_sequence(int fd)
{
	inb(fd, 0x68);
	if (wait_until_bitmask_is_value(fd, 0x02, 0x00) != 0) {
		return -1;
	}
	outb(fd, 0x6C, 0xFF);
	if (close(fd) != 0) {
		perror("close()");
		return -1;
	}

	return 0;
}

void print_usage(FILE *stream, char *progname)
{
	fprintf(stream, "Usage: %s [options]\n"
			"Options:\n"
			" -m\tSet fan to max setting\n"
			" -n\tSet fan to normal setting\n"
			" -h\tPrint this help message\n", progname);
}
