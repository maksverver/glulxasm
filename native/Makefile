GLKLIBS=cheapglk/libcheapglk.a
GLKINC=cheapglk/

# For Gargoyle GLK:
#GLKLIBS=garglk/libgarglkmain.a garglk/libgarglk.so
#GLKINC=garglk/

# Note: for x86, should probably add -march=i686
CFLAGS=-Wall -Wextra -O2 -I$(GLKINC) -g
LDLIBS=$(GLKLIBS)

STORYFILE_CFLAGS=$(CFLAGS) -Wno-unused-variable -fno-unit-at-a-time

OBJS=binary_storyfile.o glkop.o main.o messages.o native.o native_io.o \
     native_search.o storyfile.o

all: story

storyfile.c: storyfile.ulx
	(cd .. && ./glulx-to-c.py) <storyfile.ulx >storyfile.c

storyfile.o: storyfile.c
	$(CC) $(STORYFILE_CFLAGS) -c storyfile.c

binary_storyfile.o: storyfile.ulx
	ld -m elf_x86_64 -r -b binary -o binary_storyfile.o storyfile.ulx

story: $(OBJS) $(GLKLIBS)
	$(CC) $(CFLAGS) -o story $(OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean:
	rm -f story storyfile.c

.PHONY: all clean distclean
