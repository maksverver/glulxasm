CC=gcc
PYTHON=python
COMMON_CFLAGS=-Wall -Wextra -O2 -m32 -march=pentium4 -mtune=generic
CFLAGS=$(COMMON_CFLAGS) -I$(GLK_INC) -I$(MXML_INC)
STORY_CFLAGS=$(COMMON_CFLAGS) -Wno-unused-variable -Wno-unused-but-set-variable
LDLIBS=$(GLK_LIBS) $(MXML_LIBS) -lm
LDFLAGS=-Wl,--no-export-dynamic -Wl,--exclude-libs=ALL -Wl,--as-needed

OBJS=glkop.o main.o messages.o native.o native_float.o native_io.o \
	native_protect.o native_search.o native_state.o native_rng.o \
	storycode.o context.o bss_call_stack.o bss_data_stack.o bss_mem.o

# For cheapglk:
#GLK_INC=cheapglk32/
#GLK_LIBS=cheapglk32/libcheapglk.a

# For Gargoyle GLK:
#GLK_INC=garglk/
#GLK_LIBS=garglk/libgarglkmain.a garglk/libgarglk.so
#CFLAGS+=-DGARGLK

# For Mini-XML:
MXML_INC=mxml/
MXML_LIBS=mxml/libmxml.a

# To link story code, interpreter memory and call/data stack at fixed addresses:
# (Note: this seems to work on Linux only!)
#LDFLAGS+=-Tstory.lds

# To embed the story file in the executable:
#OBJS+=storyfile.o
#CFLAGS+=-DNATIVE_EMBED_STORYDATA

all: story

context.o: context_i386.S
	$(CC) $(CFLAGS) -c -o $@ $<

storycode.c: storyfile.dat
	(cd .. && $(PYTHON) -u glulx-to-c.py) <storyfile.dat >storycode.c

storycode.o: storycode.c
	$(CC) $(STORY_CFLAGS) -c storycode.c

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
