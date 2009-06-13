#ifndef GLKOP_H
#define GLKOP_H

#include "glk.h"

int init_dispatch(void);
glui32 perform_glk(glui32 funcnum, glui32 numargs, glui32 *arglist);

#endif /* ndef GLKOP_H */