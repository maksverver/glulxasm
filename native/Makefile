CFLAGS=-Wall -Wextra -O2 -Icheapglk/
LDLIBS=cheapglk/libcheapglk.a

OBJS=native.o storyfile.o glkop.o glulxe.o main.o

story: $(OBJS)
	$(CC) $(CFLAGS) -o story $(OBJS) $(LDLIBS)

all: story

clean:
	rm -f *.o

distclean:
	rm -f story

.PHONY: all clean distclean
