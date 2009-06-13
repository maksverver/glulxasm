CFLAGS=-Wall -Wextra -O2 -Icheapglk/ -g
LDLIBS=cheapglk/libcheapglk.a

# Note: for x86, should probably add -march=i686 to speed up htons/htonl
STORYFILE_CFLAGS=-Wall -Wextra -Wno-unused-variable -O2 -fno-unit-at-a-time -g

OBJS=native.o storyfile.o glkop.o glulxe.o main.o

all: story

storyfile.c: storyfile.ulx
	(cd .. && ./glulx-to-c.py) <storyfile.ulx >storyfile.c

storyfile.o: storyfile.c
	$(CC) $(STORYFILE_CFLAGS) -c storyfile.c

story: $(OBJS)
	$(CC) $(CFLAGS) -o story $(OBJS) $(LDLIBS)

clean:
	rm -f *.o

distclean:
	rm -f story storyfile.c

.PHONY: all clean distclean
