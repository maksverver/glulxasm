#include "messages.h"
#include "native.h"
#include "xtoy.h"
#include "glkop.h"
#include "glkstart.h"
#include "gi_blorb.h"
#include "mxml.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#if 0  /* this is for later */
#if defined(__i386) && !defined(__i686)
#error Architecture i386 not supported; compile with -march=i686 instead!
#elif defined(__i686)
#elif defined(__x86_64)
#error Architecture x86_64 not yet implemented!
#else
#error Unknown architecture!
#endif
#endif

/* The loader supports two types of story files:
    - bare Glulx executable modules ("Glul...")
    - Blorb files ("FORM....IFRS") with a Glulx executable chunk

   These files can be loaded in two ways:
    - linked into the executable (and therefore mapped into memory)
    - loaded from a file ("storyfile.dat")

   In either case, on startup, the main method loads the Glulx executable chunk
   and makes it available through `glulx_data' (and its size in `glulx_size').
   The checksum is verified and its length will be at least 256 bytes. The
   native library can use this to reset memory, save games, and so on.
*/

/* The Glulx module */
const uint8_t *glulx_data = NULL;
size_t         glulx_size  = 0;

static void alloc_glulx_data(size_t size)
{
    glulx_data = malloc(size);
    if (glulx_data == NULL)
        fatal("could not allocate %u bytes for Glulx executable",
              (unsigned)size);
    glulx_size = size;
}

static void verify_glulx_data()
{
    size_t n;
    const uint32_t *words;
    uint32_t esum, csum;

    if (glulx_size < 256)
        fatal("Glulx executable too small");
    if (memcmp(glulx_data, "Glul", 4) != 0)
        fatal("Glulx executable has invalid signature");

    /* Verify checksum */
    words = (const uint32_t*)glulx_data;
    esum = htonl(words[8]);
    csum = -esum;
    for (n = 0; n < glulx_size/4; ++n)
        csum += htonl(words[n]);
    if (csum != esum)
        fatal("Glulx executable has invalid checksum (computed %08x, "
              "expected %08x)", csum, esum);
}

glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

#ifdef GARGLK

/* Retrieve the IF metadata as an XML string (must be freed by the caller). */
static char *get_ifmd_text(giblorb_map_t *map)
{
    char *str;
    giblorb_result_t res;
    if (giblorb_load_chunk_by_type(map, giblorb_method_Memory, &res,
            giblorb_make_id('I', 'F', 'm','d'), 0) != giblorb_err_None)
    {
        return NULL;
    }
    str = malloc(res.length + 1);
    if (str != NULL)
    {
        memcpy(str, res.data.ptr, res.length);
        str[res.length] = 0;
    }
    giblorb_unload_chunk(map, res.chunknum);
    return str;
}

static mxml_node_t *xml_find_node(mxml_node_t *root, ...)
{
    va_list ap;
    const char *tag;
    mxml_node_t *node;

    va_start(ap, root);
    node = root;
    while (node != NULL && (tag = va_arg(ap, const char*)) != NULL)
        node = mxmlFindElement(node, root, tag, NULL, NULL, MXML_DESCEND_FIRST);
    va_end(ap);

    return node;
}

/* Concatenate values of all direct text child nodes of `elem' and add spaces
   between them; the result must be freed by the caller. */
static char *xml_get_text(mxml_node_t *elem)
{
    char *str, *p;
    mxml_node_t *child;
    size_t num_nodes, total_size;

    num_nodes = 0;
    total_size = 0;
    for (child = elem->child; child != NULL; child = child->next)
    {
        if (child->type == MXML_TEXT)
        {
            num_nodes += 1;
            total_size += strlen(child->value.text.string);
        }
    }

    str = malloc(total_size + num_nodes);
    if (str != NULL)
    {
        p = str;
        for (child = elem->child; child != NULL; child = child->next)
        {
            if (child->type == MXML_TEXT)
            {
                strcpy(p, child->value.text.string);
                p += strlen(p) + 1;
                p[-1] = ' ';
            }
        }
        p[-1] = '\0';
    }
    return str;
}

static void set_garglk_info(giblorb_map_t *map)
{
    char *xml_str, *text;
    mxml_node_t *root, *node;

    xml_str = get_ifmd_text(map);
    if (xml_str == NULL) return;

    root = mxmlLoadString(NULL, xml_str, MXML_NO_CALLBACK);
    free(xml_str);
    xml_str = NULL;

    if (root == NULL) return;

    if ((node = xml_find_node(root, "ifindex", "story", "bibliographic",
                                    "title", NULL)) != NULL &&
        (text = xml_get_text(node)) != NULL)
    {
        garglk_set_program_name(text);
        free(text);
    }

    if ((node = xml_find_node(root, "ifindex", "story", "bibliographic",
                                    "headline", NULL)) != NULL &&
        (text = xml_get_text(node)) != NULL)
    {
        garglk_set_story_name(text);
        free(text);
    }

    mxmlDelete(root);
}
#endif /* def GARGLK */

int glkunix_startup_code(glkunix_startup_t *data)
{
    strid_t str;

#if NATIVE_EMBED_STORYDATA
    extern const uint8_t _binary_storyfile_dat_start[];
    extern void _binary_storyfile_dat_size;
    const uint8_t *storyfile_data = &_binary_storyfile_dat_start[0];
    const size_t  storyfile_size  = (size_t)&_binary_storyfile_dat_size;
    str = glk_stream_open_memory(
        (void*)storyfile_data, storyfile_size, filemode_Read, 0 );
#else
    frefid_t fileref = glk_fileref_create_by_name(
        fileusage_Data|fileusage_BinaryMode, "storyfile.dat", 0);
    str = glk_stream_open_file(fileref, filemode_Read, 0);
    glk_fileref_destroy(fileref);
#endif

    /* Try to open as Blorb file */
    {
        giblorb_err_t err = giblorb_set_resource_map(str);
        if (err == giblorb_err_None)
        {
            giblorb_map_t *map = giblorb_get_resource_map();
            giblorb_result_t res;

            if (giblorb_load_resource(map, giblorb_method_FilePos,
                    &res, giblorb_ID_Exec, 0) != giblorb_err_None)
            {
                fatal("couldn't load executable chunk from Blorb file");
                return FALSE;
            }

            alloc_glulx_data(res.length);
            glk_stream_set_position(str, res.data.startpos, seekmode_Start);
            if (glk_get_buffer_stream(str, (char*)glulx_data, glulx_size)
                    != glulx_size)
            {
                fatal("couldn't read (all) Glulx data from Blorb file");
                return FALSE;
            }
            verify_glulx_data();
            /* do NOT close the stream here, as the Glk library owns it now */

#ifdef GARGLK
            /* Get addition metadata from Blorb file */
            set_garglk_info(map);
#endif
            return TRUE;
        }
    }

    /* Alternatively, try to open as a bare Glulx file */
    {
        char id[4];
        glk_stream_set_position(str, 0, seekmode_Start);
        if (glk_get_buffer_stream(str, id, 4) == 4 &&
            memcmp(id, "Glul", 4) == 0)
        {
            glk_stream_set_position(str, 0, seekmode_End);
            alloc_glulx_data(glk_stream_get_position(str));
            glk_stream_set_position(str, 0, seekmode_Start);
            if (glk_get_buffer_stream(str, (char*)glulx_data, glulx_size)
                    != glulx_size)
            {
                fatal("couldn't read (all) Glulx data from file");
                return FALSE;
            }
            verify_glulx_data();
            glk_stream_close(str, NULL);
            return TRUE;
        }
    }

    fatal("couldn't load Glulx module data");
    return FALSE;

    (void)data;  /* unused */
}

void glk_main()
{
    if (!init_dispatch()) return;

    native_start();
}
