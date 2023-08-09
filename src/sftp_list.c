#include <stdlib.h>

#include <libssh/sftp.h>

#include "sftp_list.h"
#include "debug.h"

ListT *
List_new(size_t length, size_t type_size) {
    ListT *self = malloc(sizeof *self);
    void **list = malloc(length * type_size);
    *self = (ListT){.list = list, .length = 0, .allocated = length};
    return self;
}

void
List_re_alloc(ListT *self, size_t new_size) {
    if (self->allocated >= new_size) {
        return;
    }

    /** Over-allocating the list to reduce calls to ``realloc``
     *
     * .. note:: For more information about the algrithm check
     *    `PyList resized-length algorithm`_.
     *
     * .. PyList resized-length algorithm::
     *
     *      https://github.com/python/cpython/blob/main/Objects/listobject.c#L62-#L72 */
    new_size = (new_size + (new_size >> 3) + 6) & ~(size_t)3;

    self->list = realloc(self->list, new_size * sizeof *self->list);
    if (self->list == NULL) {
        DBG_ERR("Couldn't reallocate memory for `ListT.dirs`, tried to allocate %zu size",
                new_size);
        return;
    }

    self->allocated = new_size;
}

void
List_push(ListT *self, void *attr) {
    List_re_alloc(self, self->length + 1);
    self->list[self->length++] = attr;
}

void *
List_pop(ListT *self) {
    if (!List_is_empty(self)) {
        return self->list[--self->length];
    }
    return NULL;
}

bool
List_is_empty(ListT *self) {
    return !self->length;
}

void
List_free(ListT *self) {
    free(self->list);
    free(self);
}