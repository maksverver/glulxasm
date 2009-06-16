#include "glulxe.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

char *make_temp_string(glui32 addr)
{
    char *res;
    assert(mem[addr] == 0xe0);
    res = strdup((char*)&mem[addr + 1]);
    assert(res != NULL);
    return res;
}

glui32 *make_temp_ustring(glui32 addr)
{
    glui32 *res;
    uint32_t *i, *j;

    assert(mem[addr] == 0xe2);
    i = j = (uint32_t*)&mem[addr + 4];
    while (*j) ++j;
    res = malloc(sizeof(glui32)*(j - i + 1));
    while (i <= j) *res++ = ntohl(*i++);
    assert(res != NULL);
    return res;
}

void free_temp_string(char *str)
{
    free(str);
}

void free_temp_ustring(glui32 *str)
{
    free(str);
}
