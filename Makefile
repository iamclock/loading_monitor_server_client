CC=gcc

CFLAGS = -Wall -Wextra -Wstrict-prototypes \
		 -Wdeclaration-after-statement -Werror -O3 -D_DEFAULT_SOURCE
CFLAGSHEADERS = -Wmissing-declarations
DBGFLAGS = -ggdb -g3

ifdef DEBUG
	CFLAGS += $(DBGFLAGS)
endif

LDFLAGS = -lrt -lpthread

TARGETS = server client logger_utility

all: $(TARGETS)


server: server.o main_server.o
	$(CC) $(CFLAGS) $(CFLAGSHEADERS) $^ -o $@ $(LDFLAGS)


client: client.o main_client.o
	$(CC) $(CFLAGS) $(CFLAGSHEADERS) $^ -o $@ $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CFLAGSHEADERS) -c $< -o $@

main_server.o: main_server.c server.h
	$(CC) $(CFLAGS) -c $< -o $@

main_client.o: main_client.c client.h
	$(CC) $(CFLAGS) -c $< -o $@

logger_utility: logger_utility.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

logger_utility.o: logger_utility.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)