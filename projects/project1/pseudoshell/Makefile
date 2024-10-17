all: pseudo-shell

pseudo-shell: main.o string_parser.o command.o
	gcc -Wall -g -o pseudo-shell main.o string_parser.o command.o

main.o: main.c string_parser.h command.h
	gcc -Wall -g -c main.c

string_parser.o: string_parser.c string_parser.h
	gcc -Wall -g -c string_parser.c

command.o: command.c command.h
	gcc -Wall -g -c command.c

clean:
	rm -f main.o string_parser.o command.o pseudo-shell
