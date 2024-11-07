# Define compiler
CC = gcc
CFLAGS = -Wall

# Define the target executables
TARGETS = part1 part2 part3 part4 part5 cpubound iobound

# Default target to build all executables
all: $(TARGETS)

# Phony target to ensure cpubound and iobound are built first
cpubound iobound: ; $(MAKE) $@

# Rule for each part executable, depending on cpubound and iobound
part%: part%.c MCP.h stringParser.c cpubound iobound
	$(CC) $(CFLAGS) -o $@ $< stringParser.c

# Rule for cpubound executable
cpubound: cpubound.c
	$(CC) $(CFLAGS) -o cpubound cpubound.c

# Rule for iobound executable
iobound: iobound.c
	$(CC) $(CFLAGS) -o iobound iobound.c

# Clean up the generated executables
clean:
	rm -f $(TARGETS)
