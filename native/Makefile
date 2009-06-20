# For cheapglk:
GLKLIBS=cheapglk/libcheapglk.a
GLKINC=cheapglk/

# For Gargoyle GLK:
#GLKLIBS=garglk/libgarglkmain.a garglk/libgarglk.so
#GLKINC=garglk/

# Note: for x86, should probably add -march=i686
CFLAGS=-Wall -Wextra -O2 -I$(GLKINC) -Imxml/ -g
LDLIBS=$(GLKLIBS) mxml/libmxml.a

STORYFILE_CFLAGS=$(CFLAGS) -Wno-unused-variable -fno-unit-at-a-time

OBJS=glkop.o main.o messages.o native.o native_io.o native_search.o \
     storycode.o

all: story

storycode.c: storyfile.dat
	(cd .. && ./glulx-to-c.py) <storyfile.dat >storycode.c

storycode.o: storycode.c
	$(CC) $(STORYFILE_CFLAGS) -c storycode.c

# For embedding story data file into the executable:
#storydata.o: storyfile.dat
#	ld -m elf_x86_64 -r -b binary -o storydata.o storyfile.dat

story: $(OBJS) $(GLKLIBS)
	$(CC) $(CFLAGS) -o story $(OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean:
	rm -f story storycode.c

.PHONY: all clean distclean
