CC = gcc
CFLAGS = -Wall -Os -g -MD
LDFLAGS = -Os -g
LIBS = -lxcb -lxcb-image -lxcb-shm

all: panel

panel: panel.o common.o clock.o cpuload.o battery.o netload.o mailbox.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

%: %.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f *.o *.d

-include *.d
