CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpthread

LIB_SRC = froglink.c
LIB_HDR = froglink.h

EXAMPLES = examples/detect examples/listen examples/interactive

.PHONY: all clean

all: $(EXAMPLES)

examples/detect: examples/detect.c $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) -o $@ examples/detect.c $(LIB_SRC) $(LDFLAGS)

examples/listen: examples/listen.c $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) -o $@ examples/listen.c $(LIB_SRC) $(LDFLAGS)

examples/interactive: examples/interactive.c $(LIB_SRC) $(LIB_HDR)
	$(CC) $(CFLAGS) -o $@ examples/interactive.c $(LIB_SRC) $(LDFLAGS)

clean:
	rm -f $(EXAMPLES)
