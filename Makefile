# Makefile adapted from TREE assignment, original written by Ryan Gambord.
CC     = gcc
CFLAGS = -std=gnu99

SRCS = UTL_smallsh.c smallsh.c
OBJS = $(SRCS:.c=.o)
EXE  = smallsh

DBDIR     = debug
DBEXE     = $(DBDIR)/$(EXE)
DBOBJS    = $(addprefix $(DBDIR)/, $(OBJS))
DBCFLAGS  = -g -O0 -DDEBUG

REDIR     = build
REEXE     = $(REDIR)/$(EXE)
REOBJS    = $(addprefix $(REDIR)/, $(OBJS))
RECFLAGS  = -O3 -Wall -Wextra -Wpedantic -Werror

.PHONY: all clean debug release prep

all: debug release smallsh

smallsh: $(REEXE)
	@cp $< $@

debug: prep_debug $(DBEXE)

$(DBEXE): $(DBOBJS)
	$(CC) $(CFLAGS) $(DBCFLAGS) -DTEST -o $@ $^

$(DBDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(DBCFLAGS) -o $@ $<


release: prep_release $(REEXE)

$(REEXE): $(REOBJS)
	$(CC) $(CFLAGS) $(RECFLAGS) -o $@ $^

$(REDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(RECFLAGS) -o $@ $<

prep_debug:
	@mkdir -p $(DBDIR)

prep_release:
	@mkdir -p $(REDIR)

clean:
	rm -rf smallsh junk junk2 $(DBDIR) $(REDIR)
