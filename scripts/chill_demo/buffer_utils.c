#include <stdlib.h>
#include <string.h>

/* crit: NULL deref, overflow and leak */
char *copy_name(const char *src)
{
    char *dst = malloc(8);
    strcpy(dst, src);          /* overflow for names > 7 chars, unchecked malloc */
    return dst;
}

int get_item(int *items, int idx)
{
    return items[idx];         /* no NULL or bounds check */
}

void process(void)
{
    char *p = copy_name("a_very_long_device_name");
    free(p);
    free(p);                   /* double free */
}

/* trivial stuff that should be ignored */
int  Foo_bar(int x){int unused;return x*1000;}   // TODO later
