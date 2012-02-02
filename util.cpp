#include <stdlib.h>
#include <string.h>

namespace fcgid
{

char * ap_getword(const char **line, char stop)
{
    const char *pos = *line;
    int len;
    char *res;

    while ((*pos != stop) && *pos) {
        ++pos;
    }

    len = pos - *line;
    res = (char *)malloc(len + 1);
    memcpy(res, *line, len);
    res[len] = 0;

    if (stop) {
        while (*pos == stop) {
            ++pos;
        }
    }
    *line = pos;
    return res;
}

}

