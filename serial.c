#include "serial.h"
#include "tcp.h"

#include "wire.h"
#include "wire_stack.h"
#include "wire_fd.h"
#include "wire_lock.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char port[256];
static wire_t serial_wire_read;
static int serial_suspended;
static int serial_fd;
static wire_lock_t write_lock;

#define MSG_SERIAL_DOWN "Serial down\n"
#define MSG_SERIAL_UP "Serial up\n"

static void do_serial_wire_read(void *arg);

void serial_start(void)
{
	serial_fd = -1;
	serial_suspended = 0;
	wire_lock_init(&write_lock);
	wire_init(&serial_wire_read, "serial read", do_serial_wire_read, NULL, WIRE_STACK_ALLOC(4096));
}

static void serial_wire_read_suspend(void)
{
	serial_suspended = 1;
	wire_suspend();
	serial_suspended = 0;
}

static void do_serial_wire_read(void *arg)
{
	(void)arg;
	int ret;
	char buf[64];
	wire_fd_state_t fdstate;

	while (1) {
		if (port[0] == 0)
			serial_wire_read_suspend();

		serial_fd = open(port, O_RDWR|O_NONBLOCK);
		if (serial_fd < 0) {
			//LOG: failed to open serial port 'port'
			wire_fd_wait_msec(1000);
			continue;
		}
		tcp_write(MSG_SERIAL_UP, strlen(MSG_SERIAL_UP));

		wire_fd_mode_init(&fdstate, serial_fd);
		wire_fd_mode_read(&fdstate);

		while (1) {
			wire_fd_wait(&fdstate);

			ret = read(serial_fd, buf, sizeof(buf));
			if (ret <= 0) {
				if (ret < 0 && (errno == EINTR || errno == EAGAIN))
					continue;

				// Error condition or EOF, stop reading
				wire_fd_mode_none(&fdstate);
				close(serial_fd);
				tcp_write(MSG_SERIAL_DOWN, strlen(MSG_SERIAL_DOWN));
				break;
			} else {
				tcp_write(buf, ret);
			}
		}
	}
}

void serial_port_change(const char *newport)
{
	strncpy(port, newport, sizeof(port));
	port[sizeof(port)-1] = 0;

	if (serial_suspended)
		wire_resume(&serial_wire_read);
}

static void serial_write_locked(const char *buf, int buf_len)
{
	int written_len = 0;

	while (written_len < buf_len) {
		if (serial_fd < 0)
			return;

		int ret = write(serial_fd, buf + written_len, buf_len - written_len);
		if (ret <= 0) {
			if (ret < 0 && (errno == EINTR || errno == EAGAIN)) {
				// Write is blocked, loop back to try to write again, give time to others to do their work
				// This is a busy wait
				wire_yield();
				continue;
			}

			// Error in writing, let the reader figure the problem, discard the data for now
			return;
		} else  {
			written_len += ret;
		}
	}
}

void serial_write(const char *buf, int buf_len)
{
	wire_lock_take(&write_lock);
	serial_write_locked(buf, buf_len);
	wire_lock_release(&write_lock);
}
