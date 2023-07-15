CC = gcc
CFLAGS = -Wall -Wextra -g

all: bgame

bgame: bgame.o message.o logging.o
	$(CC) $(CFLAGS) -o $@ $^

bgame.o: bgame.c message.h logging.h
	$(CC) $(CFLAGS) -c $< -o $@

message.o: message.c message.h
	$(CC) $(CFLAGS) -c $< -o $@

logging.o: logging.c logging.h message.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o bgame