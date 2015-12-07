CC = gcc
CFLAGS = -Wall -Werror -Wextra -Wunused
CFLAGS += -g -O2 -std=gnu99
OBJS = networking.c

client: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o client

clean:
	rm -rvf *.o client
