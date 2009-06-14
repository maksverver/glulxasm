CFLAGS=-Wall -Wextra -O2 -Icheapglk/ -g -m32 -march=i686
#LDLIBS=garglk/libgarglkmain.a garglk/libgarglk.so
LDLIBS=cheapglk32/libcheapglk.a

# Note: for x86, should probably add -march=i686 to speed up htons/htonl
STORYFILE_CFLAGS=-Wall -Wextra -Wno-unused-variable -O2 -fno-unit-at-a-time -g -m32

OBJS=native.o storyfile.o glkop.o glulxe.o main.o binary_storyfile.o

all: story

storyfile.c: storyfile.ulx
	(cd .. && ./glulx-to-c.py) <storyfile.ulx >storyfile.c

storyfile.o: storyfile.c
	$(CC) $(STORYFILE_CFLAGS) -c storyfile.c

binary_storyfile.o: storyfile.ulx
	ld -m elf_i386 -r -b binary -o binary_storyfile.o storyfile.ulx

story: $(OBJS)
	$(CC) $(CFLAGS) -o story $(OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean:
	rm -f story storyfile.c

.PHONY: all clean distclean
