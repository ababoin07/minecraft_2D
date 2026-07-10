#include <sys/stat.h>
#include <stdbool.h>
#include "files_api.h"

/**
 * @brief Check if a regular file exists.
 */
bool file_exists(const char *path)
{
    struct stat buffer;
    if (stat(path, &buffer) != 0)
        return false;
    return S_ISREG(buffer.st_mode);
}

/**
 * @brief Check if a directory exists.
 */
bool directory_exists(const char *path)
{
    struct stat buffer;
    if (stat(path, &buffer) != 0)
        return false;
    return S_ISDIR(buffer.st_mode);
}