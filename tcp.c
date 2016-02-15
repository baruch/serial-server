#include "tcp.h"
#include "serial.h"

#include "wire.h"
#include "wire_stack.h"
#include "wire_pool.h"
#include "wire_fd.h"

#include "vendor/libwire/test/utils.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

#define TCP_POOL_SIZE 64

static wire_t tcp_wire_listen;
static wire_pool_t tcp_pool;
static int tcp_fds[TCP_POOL_SIZE];

static void do_tcp_listen(void *arg);

void tcp_start(void)
{
	int i;

	for (i = 0; i < TCP_POOL_SIZE; i++)
		tcp_fds[i] = -1;

	wire_pool_init(&tcp_pool, NULL, TCP_POOL_SIZE, 4096);
	wire_init(&tcp_wire_listen, "tcp listen", do_tcp_listen, NULL, WIRE_STACK_ALLOC(4096));
}

void tcp_write(const char *buf, int buf_len)
{
	int i;

	for (i = 0; i < TCP_POOL_SIZE; i++) {
		int fd = tcp_fds[i];

		// We assume that TCP will be faster than serial and will never block,
		// if it does block we will lose the write to avoid blocking, this
		// saves us from implementing user-side buffering or blocking the
		// serial for unblocked tcp connections.
		if (fd > -1)
			write(fd, buf, buf_len);
	}
}

static void do_tcp_read(void *arg)
{
	int fd = (long)arg;
	int ret;
	wire_fd_state_t fd_state;
	int i;

	wire_fd_mode_init(&fd_state, fd);
	wire_fd_mode_read(&fd_state);

	set_nonblock(fd);

	for (i = 0; i < TCP_POOL_SIZE && tcp_fds[i] != -1; i++)
		;
	tcp_fds[i] = fd;

	while (1) {
		wire_fd_wait(&fd_state);

		char buf[32];
		ret = read(fd, buf, sizeof(buf));
		if (ret == 0) {
			break;
		} else if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			else
				break;
		}

		serial_write(buf, ret);
	}

	tcp_fds[i] = -1;

	wire_fd_mode_none(&fd_state);
	close(fd);
}

static void do_tcp_listen(void *arg)
{
	int fd;

	fd = socket_setup(9090);
	if (fd < 0)
		return;

	wire_fd_state_t fd_state;
	wire_fd_mode_init(&fd_state, fd);
	wire_fd_mode_read(&fd_state);

	while (1) {
		wire_fd_wait(&fd_state);

		int new_fd = accept(fd, NULL, NULL);
		if (new_fd >= 0) {
			wire_t *task = wire_pool_alloc(&tcp_pool, "echo", do_tcp_read, (void*)(long int)new_fd);
			if (!task) {
				close(new_fd);
			}
		} else {
			if (errno != EINTR && errno != EAGAIN) {
				break;
			}
		}
	}

	wire_fd_mode_none(&fd_state);
	close(fd);
}
