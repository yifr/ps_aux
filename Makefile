CC=gcc
CFLAGS = -g 
all: my_ps

my_ps:
	$(CC) $(CFLAGS) -o my_ps my_ps.c 

clean:
	rm -rf my_ps
