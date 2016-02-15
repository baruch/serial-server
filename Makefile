CFLAGS=-Ivendor/libwire/include
LDFLAGS=-lpthread

all: serial-server

serial-server: serial-server.o tcp.o serial.o vendor/libwire/test/utils.o vendor/libwire/libwire.a

vendor/libwire/libwire.a:
	cd vendor/libwire && ./configure && ninja

clean:
	-rm serial-server *.o vendor/libwire/libwire.a

.PHONY: clean all
