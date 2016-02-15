#ifndef SERIAL_SERVER_SERIAL_H
#define SERIAL_SERVER_SERIAL_H

void serial_start(void);
void serial_port_change(const char *port);
void serial_write(const char *buf, int buf_len);

#endif
