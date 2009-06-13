#include "storyfile.h"
#include "glkop.h"
#include "glkstart.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "xtoy.h"

glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    (void)data;
    return TRUE;
}

static uint32_t get_checksum()
{
    uint32_t *words = (uint32_t*)mem, sum = 0, n;
    for (n = 0; n < 8; ++n) sum += htonl(words[n]);
    for (n = 9; n < init_extstart/4; ++n) sum += htonl(words[n]);
    return sum;
}

static bool load_story(const char *path)
{
    uint32_t checksum;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "Story file \"%s\" could not be opened!", path);
        return false;
    }
    if (fread(mem, 1, init_extstart, fp) != init_extstart)
    {
        fprintf(stderr, "Story file \"%s\" could not be read!", path);
        return false;
    }
    fclose(fp);

    checksum = get_checksum();
    if (checksum != init_checksum)
    {
        fprintf(stderr, "Checksum mismatch for story file \"%s\"!\n"
                        "\tComputed value: %08x\n"
                        "\tExpected value: %08x\n",
                        path, checksum, init_checksum );
        return false;
    }

    memset(mem + init_extstart, 0, init_endmem - init_extstart);

    return true;
}

void glk_main()
{
    if (!init_dispatch()) return;
    if (!load_story("storyfile.ulx")) return;

    native_setrandom(0);
    native_setstringtbl(init_decoding_tbl);

    stack[0] = 0;
    func(init_start_func)(&stack[0]);
    native_quit();
}
