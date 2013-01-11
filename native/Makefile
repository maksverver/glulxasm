CC=gcc
CFLAGS=-Wall -Wextra -O0 -I$(GLKINC) -Imxml/ -g -m32 -march=i686
LDLIBS=$(GLKLIBS) mxml/libmxml.a
LDFLAGS=-Wl,--no-export-dynamic -Wl,--exclude-libs=ALL -Wl,--as-needed
#LDFLAGS+=-Tstory.lds

STORYFILE_CFLAGS=$(CFLAGS) -fno-unit-at-a-time \
	-Wno-unused-variable -Wno-unused-but-set-variable

OBJS=glkop.o main.o messages.o native.o native_io.o native_search.o \
     native_protect.o native_state.o native_rng.o storycode.o context.o

# For cheapglk:
GLKLIBS=cheapglk32/libcheapglk.a
GLKINC=cheapglk32/

# For Gargoyle GLK:
#GLKLIBS=garglk/libgarglkmain.a garglk/libgarglk.so
#GLKINC=garglk/
#CFLAGS+=-DGARGLK

# To embed the story file in the executable:
#OBJS+=storyfile.o
#CFLAGS+=-DNATIVE_EMBED_STORYDATA

all: story

context.o: context_i386.s
	$(CC) $(CFLAGS) -c -o $@ $<

storycode.c: storyfile.dat
	(cd .. && ./glulx-to-c.py) <storyfile.dat >storycode.c

storycode.o: storycode.c
	$(CC) $(STORYFILE_CFLAGS) -c storycode.c

# For embedding story data file into the executable:
storyfile.o: storyfile.dat
	ld -m elf_i386 -r -b binary -o $@ $<

story: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean:
	rm -f story storycode.c

.PHONY: all clean distclean
