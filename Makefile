
all:
	gcc -O3 -Wall -Wextra pace.c -lpthread

clean:
	rm -f a.out

run: all
	./a.out 10

