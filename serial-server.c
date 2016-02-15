#include <stdio.h>

#include "serial.h"
#include "tcp.h"

#include "wire.h"
#include "wire_fd.h"
#include "wire_stack.h"
#include "wire_io.h"

static wire_thread_t wt;
static wire_t init_wire;

static int usage(const char *name)
{
	printf("Usage: %s <port path>\n", name);
	return 1;
}

static void do_init_wire(void *arg)
{
	const char *port = arg;

	serial_start();
	serial_port_change(port);

	tcp_start();
}

int main(int argc, char **argv)
{
	if (argc != 2)
		return usage(argv[0]);

	wire_thread_init(&wt);
	wire_stack_fault_detector_install();
	wire_fd_init();
	wire_io_init(16);

	wire_init(&init_wire, "init", do_init_wire, argv[1], WIRE_STACK_ALLOC(4096));

	wire_thread_run();

	return 0;
}
