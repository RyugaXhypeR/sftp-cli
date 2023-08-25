/** A very minimal logger to handle basic logging for the project */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#ifdef D
#define DBG_STATUS 1
#else
#define DBG_STATUS 0
#endif  // !D

#include <stdio.h>

#include "sftp_ansi_colors.h"

enum DBG_LEVELS {
    DBG_LEVEL_DEBUG,
    DBG_LEVEL_INFO,
    DBG_LEVEL_CRITICAL,
};

/** Simple logger macro
 *
 * .. note:: Currently requires you to pass in variadic args to it
 * */
#define LOG(level, prompt, ...)                                                          \
    do {                                                                                 \
        if (DBG_STATUS) {                                                                \
            fprintf(stderr, "[%s]:%s:%d:%s: " prompt "\n", dbg_level_to_str(level),      \
                    __FILE__, __LINE__, __func__, __VA_ARGS__);                          \
        }                                                                                \
        if (level == DBG_LEVEL_CRITICAL) {                                               \
            fprintf(stderr, "[%s]::" prompt "\n", dbg_level_to_str(level), __VA_ARGS__); \
        }                                                                                \
    } while (0)

#define DBG_ERR(prompt, ...) LOG(DBG_LEVEL_CRITICAL, prompt, __VA_ARGS__)
#define DBG_INFO(prompt, ...) LOG(DBG_LEVEL_INFO, prompt, __VA_ARGS__)
#define DBG_DEBUG(prompt, ...) LOG(DBG_LEVEL_DEBUG, prompt, __VA_ARGS__)

static inline char*
dbg_level_to_str(enum DBG_LEVELS level) {
    switch (level) {
        case DBG_LEVEL_DEBUG:
            return ANSI_FG_GREEN "DEBUG" ANSI_RESET;
        case DBG_LEVEL_INFO:
            return ANSI_FG_CYAN "INFO" ANSI_RESET;
        case DBG_LEVEL_CRITICAL:
            return ANSI_FG_RED "CRITICAL" ANSI_RESET;
    }

    return "";
}

/** Log the size and *malloc* memory */
static inline void*
dbg_malloc(size_t size) {
    DBG_DEBUG("Allocating %zu bytes of memory...", size);
    void* ptr = malloc(size);

    if (ptr == NULL) {
        DBG_ERR("Unable to allocate %zu bytes of memory", size);
        return NULL;
    }

    DBG_INFO("Allocated %zu bytes of memory", size);

    return ptr;
}

/** Log the size and *calloc* memory */
static inline void*
dbg_calloc(size_t num_bytes, size_t type_size) {
    void* ptr = calloc(num_bytes, type_size);

    if (ptr == NULL) {
        DBG_ERR("Unable to allocate %zu bytes of memory", num_bytes * type_size);
        return NULL;
    }

    DBG_INFO("Allocated %zu bytes of memory", num_bytes * type_size);

    return ptr;
}

/** Log the size and *relloc* memory for the pointer */
static inline void*
dbg_realloc(void *ptr, size_t new_size) {
    void *new_allocted = realloc(ptr, new_size);

    if (new_allocted == NULL) {
        DBG_ERR("Unable to realloct memory for pointer %p", ptr);
        return NULL;
    }

    DBG_INFO("Reallocated %zu bytes of memory for pointer: %p", new_size, ptr);

    return new_allocted;
}

/** Free if pointer is not NULL and log the result */
static inline void
dbg_safe_free(void* ptr) {
    if (ptr == NULL) {
        DBG_INFO("Pointer is null, not freeing %s", "");
        return;
    }

    free(ptr);
    DBG_INFO("Freed pointer: %p", ptr);
}

#endif /* DEBUG_H */
