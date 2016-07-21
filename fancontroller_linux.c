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

#define MAX 0x77
#define NORMAL 0x76
#define IOPORTS "/dev/port"

/**
 * Sets up IOPORTS interface.
 *
 * @return filedescriptor for interface
 */
int ec_intro_sequence();

/**
 * Cleans up IOPORTS.
 *
 * @param filedescriptor to use
 * @return 0 on success, -1 on failure
 */
int ec_outro_sequence(int fd);

/**
 * Reads one byte from IOPORTS interface.
 *
 * @param filedescriptor to use
 * @port port / position to read from
 * @return value read on success, -1 on failure
 */
int inb(int fd, int port);

/**
 * Writes one byte to IOPORTS interface.
 *
 * *Might exit program early*.
 *
 * @param filedescriptor to use
 * @param port to write to
 * @param value to write. Must be positive.
 * @return 0 on success, -1 on failure
 */
int outb(int fd, int port, int value);

/**
 * Waits for a bitmask to appear
 *
 * Loops for some time until the given port on the filedescriptor has value
 * if for specified bitmask.
 *
 * @param filedescriptor to use
 * @param port to read from
 * @param bitmask to apply
 * @param value to check. Must be positive.
 * @return 0 when bitmask appears, -1 if it doesn't
 */
int wait_until_bitmask_is_value(int fd, int port, int bitmask, int value);

/**
 * Prints usage.
 *
 * @param stream to use (eg. stdout/stderr)
 * @param progname to display (usually argv[0])
 */
void print_usage(FILE *stream, char *progname);


int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, ERRPREF "No arguments supplied (-h for usage)\n");
		exit(1);
	}

	int cmd = 0;
	switch (argv[1][1]) {
		case 'm':
		case 'M':
			cmd = MAX;
			break;
		case 'n':
		case 'N':
			cmd = NORMAL;
			break;
		case 'h':
			print_usage(stdout, argv[0]);
			exit(1);
		default:
			fprintf(stderr, ERRPREF "Invalid arguments!\n");
			exit(1);
	}

	assert(cmd == MAX || cmd == NORMAL);

	int fd = 0;
	if ((fd = ec_intro_sequence()) < 0) {
		fprintf(stderr, ERRPREF "Unable to perform intro sequence. "
				"Cleaning up\n");
		if (fd >= 0) ec_outro_sequence(fd);
		exit(1);
	}

	if (wait_until_bitmask_is_value(fd, 0x6C, 0x02, 0x00) != 0) {
		fprintf(stderr, ERRPREF "Error waiting for magic value. "
				"Cleaning up\n");
		if (fd >= 0) ec_outro_sequence(fd);
		exit(1);
	}

	if (outb(fd, 0x68, cmd) != 0) {
		fprintf(stderr, ERRPREF "Unable to write magic command %x\n",
				cmd);
		if (fd >= 0) ec_outro_sequence(fd);
		exit (1);
	}

	ec_outro_sequence(fd);
}

int ec_intro_sequence()
{
	int fd = open(IOPORTS, O_RDWR);
	if (fd < 0) {
		perror("open()");
	}

	if (wait_until_bitmask_is_value(fd, 0x6C, 0x80, 0x00) != 0) {
		return -1;
	}
	if (inb(fd, 0x68) == -1) return -1;

	if (wait_until_bitmask_is_value(fd, 0x6C, 0x02, 0x00) != 0) {
		return -1;
	}

	if (outb(fd, 0x6C, 0x59) == -1) return -1;
	return fd;
}

int ec_outro_sequence(int fd)
{
	if (inb(fd, 0x68) < 0) return -1;
	if (wait_until_bitmask_is_value(fd, 0x6C, 0x02, 0x00) != 0) {
		return -1;
	}
	if (outb(fd, 0x6C, 0xFF) < 0) return -1;

	if (close(fd) != 0) {
		perror("close()");
		return -1;
	}

	return 0;
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
	assert(value > 0);
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

int wait_until_bitmask_is_value(int fd, int port, int bitmask, int value)
{
	assert(value > 0);

	for (int i = 0; i < 10000; i++) {
		if ((inb(fd, port) & bitmask) == value) {
			return 0;
		}
		sleep(0);
	}
	fprintf(stderr, ERRPREF "Timeout waiting for mask %x with value %x on port %x\n",
			bitmask, port);
	return -1;
}

void print_usage(FILE *stream, char *progname)
{
	fprintf(stream, "Usage: %s [options]\n"
			"Options:\n"
			" -m\tSet fan to max setting\n"
			" -n\tSet fan to normal setting\n"
			" -h\tPrint this help message\n", progname);
}
