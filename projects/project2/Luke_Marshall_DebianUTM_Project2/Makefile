CC = gcc
CFLAGS = -Wall

TARGETS = part1 part2 part3 part4 part5 cpubound iobound

all: $(TARGETS)

part%: part%.c MCP.h stringParser.c
	$(CC) $(CFLAGS) -o $@ $< stringParser.c

cpubound: cpubound.c
	$(CC) $(CFLAGS) -o cpubound cpubound.c

iobound: iobound.c
	$(CC) $(CFLAGS) -o iobound iobound.c

clean:
	rm -f $(TARGETS)
