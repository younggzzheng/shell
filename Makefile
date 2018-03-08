CFLAGS = -g
CFLAGS += -D_GNU_SOURCE -std=gnu99
CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
#CFLAGS += -pedantic -Werror

CC = gcc
EXECS = 33sh 33noprompt
PROMPT = -DPROMPT
.PHONY: all clean

all: $(EXECS)

33sh: sh.c jobs.c
	$(CC) $< $(CFLAGS) $(PROMPT) -o $@

33noprompt: sh.c jobs.c
	$(CC) $< $(CFLAGS) -o $@
clean:
	rm -f $(EXECS)
