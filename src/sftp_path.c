#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include "commands.h"
#include "debug.h"
#include "sftp_list.h"
#include "sftp_path.h"

/**
 * Split a path string into a list of path components.
 *
 * :param path_str:  NULL terminated string representing a path. The path can be absolute or relative.
 * :param start: [INCLUSIVE] Start index of the slice. It must be less than stop and length of ``path_str``.
 * :param stop: [EXCLUSIVE] Stop index of the slice. It must be greater than start.
 * :return: Sliced path string. It must be freed by the caller.
 */
char *
path_str_slice(const char *path_str, size_t start, size_t stop) {
    size_t length = stop - start;
    char *sliced_path_str;

    if (length < 1) {
        return NULL;
    }

    sliced_path_str = calloc(length, sizeof *sliced_path_str);
    for (size_t i = start; i < stop; i++) {
        sliced_path_str[i - start] = path_str[i];
    }
    sliced_path_str[stop - 1] = '\0';

    return sliced_path_str;
}

/**
 * Remove redundant prefixes from filesystem items (files and dirs).
 * Basically converts ``./this`` to ``this`` and ``////this`` to ``/this``
 */
char *
path_remove_prefix(char *path_str, size_t length) {
    char *cleaned_path_str = path_str;
    size_t num_slash_prefix = 0;
    uint8_t is_root_dir = 1;

    if (length < 2) {
        return path_str;
    }

    if (path_str[0] == '.' && path_str[1] == PATH_SEPARATOR) {
        cleaned_path_str = path_str_slice(path_str, 2, length);
        length -= 2;
        is_root_dir = 0;
    }

    for (size_t i = 0; i < length; i++) {
        /* Break as soon as the first non path separator is found. */
        if (cleaned_path_str[i] != PATH_SEPARATOR) {
            break;
        }

        num_slash_prefix++;
    }

    if (num_slash_prefix) {
        cleaned_path_str =
            path_str_slice(cleaned_path_str, num_slash_prefix - is_root_dir, length);
    }

    return cleaned_path_str;
}

/**
 * Remove redundant suffixes from filesystem items (files and dirs).
 * For example: ``this/`` to ``this`` and ``this////`` to ``this``
 */
char *
path_remove_suffix(char *path_str, size_t length) {
    char *cleaned_path_str = path_str;
    size_t num_slash_suffix = 0;

    if (length < 2) {
        return cleaned_path_str;
    }

    for (ssize_t i = (ssize_t)length - 1; i > -1; i++) {
        if (cleaned_path_str[i] != PATH_SEPARATOR) {
            break;
        }
        num_slash_suffix++;
    }

    if (num_slash_suffix) {
        cleaned_path_str = path_str_slice(path_str, 0, length - num_slash_suffix + 1);
    }

    return cleaned_path_str;
}

/**
 * Join multiple paths into a single path.
 *
 * :param num_paths: Number of paths to join.
 * :param ...: Variable number of paths to join.
 * :return: Joined path string. It must be freed by the caller.
 *
 * .. note:: Recommended to use ``FS_PATH_JOIN(...)` instead of this function.
 */
char *
path_join(size_t num_paths, ...) {
    va_list args;
    size_t fs_len;
    size_t path_len = 0;
    char *path_buf = calloc(BUF_SIZE_FS_PATH, (sizeof *path_buf));
    char *fs_name;

    va_start(args, num_paths);
    for (size_t i = 0; i < num_paths && path_len < BUF_SIZE_FS_PATH; i++) {
        fs_name = va_arg(args, char *);
        fs_len = strlen(fs_name);
        fs_name = path_remove_prefix(fs_name, fs_len);
        fs_len = strlen(fs_name);
        fs_name = path_remove_suffix(fs_name, fs_len);

        path_len += strlen(fs_name);
        strcat(path_buf, fs_name);

        if (i < num_paths - 1) {
            path_buf[path_len++] = PATH_SEPARATOR;
        }
    }
    va_end(args);

    return path_buf;
}

/** Clear a path buffer. */
void
path_buf_clear(char *path_buf, size_t length) {
    for (size_t i = 0; i < length; i++) {
        path_buf[i] = '\0';
    }
}

/**
 * Split a path string into a list of path components.
 * For example: ``/this/is/a/path`` to ``["this", "is", "a", "path"]``
 *
 * :param path_str:  NULL terminated string representing a path. The path can be absolute or relative.
 * :param length: Length of the path string.
 * :return: List of path components. It must be freed by the caller.
 */
ListT *
path_split(const char *path_str, size_t length) {
    ListT *path_list = List_new(0, sizeof(char *));
    char *path_buf = malloc(BUF_SIZE_FS_NAME * sizeof *path_buf);
    size_t path_buf_len = 0;

    for (size_t i = 0; i < length; i++) {
        if (path_str[i] != PATH_SEPARATOR) {
            path_buf[path_buf_len++] = path_str[i];
            continue;
        }
        path_buf[path_buf_len] = '\0';
        List_push(path_list, path_buf, path_buf_len + 1);
        path_buf_clear(path_buf, BUF_SIZE_FS_NAME);
        path_buf_len = 0;
    }
    List_push(path_list, path_buf, path_buf_len + 1);
    free(path_buf);

    return path_list;
}

/**
 * Check if a path is dotted.
 * For example: ``.`` and ``..`` are dotted paths.
 *
 * :param path_str:  NULL terminated string representing a path. The path can be absolute or relative.
 * :param length: Length of the path string.
 * :return: True if the path is dotted, False otherwise.
 */
bool
path_is_dotted(const char *path_str, size_t length) {
    if (length > 2) {
        return false;
    }
    return !strcmp(path_str, ".") || !strcmp(path_str, "..");
}

/**
 * Check if a path is hidden.
 * For example: ``.hidden`` and ``.hidden/`` are hidden paths.
 *
 * :param path_str:  NULL terminated string representing a path. The path can be absolute or relative.
 * :param length: Length of the path string.
 * :return: True if the path is hidden, False otherwise.
 */
bool
path_is_hidden(const char *path_str, size_t length) {
    if (!length) {
        return false;
    }

    return *path_str == '.';
}

/**
 * Replace the head (grandparent) of a path with a new head.
 * For example: ``/this/is/a/path`` to ``/new/head/is/a/path``
 *
 * :param path_str:  NULL terminated string representing a path. The path can be absolute or relative.
 * :param length: Length of the path string.
 * :param grandparent: New head of the path.
 */
char *
path_replace_grandparent(char *path_str, size_t length_str, char *grandparent) {
    size_t slash_index = 0;
    char *replaced_head = path_str;

    if (length_str < 3) {
        return false;
    }

    for (; slash_index < length_str; slash_index++) {
        if (path_str[slash_index] == PATH_SEPARATOR) {
            break;
        }
    }

    if (slash_index == length_str) {
        return replaced_head;
    }

    replaced_head = path_str_slice(replaced_head, slash_index + 1, length_str);
    replaced_head = FS_JOIN_PATH(grandparent, replaced_head);

    return replaced_head;
}

/** Create parent directories of the given path if they don't exist. */
uint8_t
path_mkdir_parents(char *path_str, size_t length) {
    struct stat _dir_stats;
    int8_t result;
    char *path_buf;
    ListT *path_list = path_split(path_str, length);
    path_buf = List_get(path_list, 0);

    for (size_t i = 0; i < path_list->length; i++) {
        if (i) {
            path_buf = FS_JOIN_PATH(path_buf, List_get(path_list, i));
        }

        if (stat(path_buf, &_dir_stats) == -1) {
            /* Directory does not exist */

            if ((result = mkdir(path_buf, FS_CREATE_PERM))) {
                DBG_ERR("Couldn't create directory %s: Error code: %d\n", path_buf,
                        result);
                return 0;
            }
        }
    }

    List_free(path_list);
    return 1;
}

/** Create a new file system object from a path. */
FileSystemT *
FileSystem_from_path(char *path, uint8_t type) {
    FileSystemT *file_system = malloc(sizeof *file_system);
    ListT *split;

    *file_system = (FileSystemT){
        .name = malloc(BUF_SIZE_FS_NAME),
        .relative_path = malloc(BUF_SIZE_FS_PATH),
        .absolute_path = malloc(BUF_SIZE_FS_PATH),
        .grandparent_path = malloc(BUF_SIZE_FS_PATH),
        .parent_path = malloc(BUF_SIZE_FS_PATH),
        .type = type,
    };

    if (*path == PATH_SEPARATOR) {
        strcpy(file_system->absolute_path, path);
    } else {
        strcpy(file_system->relative_path, path);
    }

    split = path_split(path, strlen(path));
    strcpy(file_system->name, List_pop(split));

    return file_system;
}

/**
 * Free a file system object.
 *
 * .. note:: Deep frees all attributes, so any references to them will be invalid.
 * */
void
FileSystem_free(FileSystemT *self) {
    free(self->name);
    free(self->absolute_path);
    free(self->relative_path);
    free(self->grandparent_path);
    free(self->parent_path);
    free(self);
}

void
FileSystem_copy(FileSystemT *self, FileSystemT *dest) {
    strcpy(dest->name, self->name);
    strcpy(dest->relative_path, self->relative_path);
}

void
FileSystem_list_push(ListT *self, FileSystemT *fs) {
    List_re_alloc(self, self->length + 1);
    self->list[self->length] = FileSystem_from_path(fs->relative_path, fs->type);
    FileSystem_copy(fs, self->list[self->length++]);
}

/**
 * Read the contents of a local directory and return a list of file system objects.
 *
 * :param path: Path to the directory.
 * :return: List of file system objects.
 */
ListT *
path_read_remote_dir(ssh_session session_ssh, sftp_session session_sftp, char *path) {
    sftp_dir dir;
    uint8_t result;
    sftp_attributes attr;
    FileTypesT file_system_type;
    FileSystemT *file_system;
    char *attr_relative_path;
    ListT *path_content_list = List_new(1, sizeof(FileSystemT *));

    dir = sftp_opendir(session_sftp, path);
    if (dir == NULL) {
        DBG_ERR("Couldn't open remote directory `%s`: %s\n", path,
                ssh_get_error(session_ssh));
        return NULL;
    }

    while ((attr = sftp_readdir(session_sftp, dir)) != NULL) {
        attr_relative_path = FS_JOIN_PATH(path, attr->name);

        switch (attr->type) {
            case SSH_FILEXFER_TYPE_REGULAR:
                file_system_type = FS_REG_FILE;
                break;
            case SSH_FILEXFER_TYPE_DIRECTORY:
                file_system_type = FS_DIRECTORY;
                break;
            default:
                DBG_INFO("Ignoring filetype %d\n", attr->type);
                continue;
        }
        file_system = FileSystem_from_path(attr_relative_path, file_system_type);
        FileSystem_list_push(path_content_list, file_system);
        FileSystem_free(file_system);
    }

    result = sftp_closedir(dir);
    if (result != SSH_FX_OK) {
        DBG_ERR("Couldn't close directory %s: %s\n", dir->name,
                ssh_get_error(session_ssh));
        return NULL;
    }

    return path_content_list;
}
