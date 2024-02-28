CC=clang
CSTAND=-std=gnu99
CFLAGS=-Weverything -lncurses -lpthread

main.o : main.c
	$(CC) $(CFLAGS) $(CSTAND) -c main.c -o $@

debug: main.c
	$(CC) $(CFLAGS) $(CSTAND)-g main.c -o main.o

clean:
	rm -f *.o *.out


