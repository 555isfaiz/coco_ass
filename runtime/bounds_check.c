#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

void __coco_check_bounds(long offset, long length) {
    if (offset < 0 || offset >= length)
    {
        errno = EINVAL;
        printf("Array index out of bound!");
        exit(1);
    }
}