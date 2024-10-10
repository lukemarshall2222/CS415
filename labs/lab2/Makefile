all: main.o command.o
	gcc -o lab2 main.o command.o

main.o: main.c command.h
	gcc -c main.c

command.o: command.c command.h
	gcc -c command.c

clean:
	rm -f *.o lab2 output.txt
