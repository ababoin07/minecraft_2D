#ifndef FILES_API_H
#define FILES_API_H

#include "includes.h"
#include <sys/stat.h>
#include <stdbool.h>

/**
 * @brief Check if a regular file exists.
 * @param path File path.
 * @return true if the file exists and is a regular file, false otherwise.
 */
bool file_exists(const char *);

/**
 * @brief Check if a directory exists.
 * @param path Directory path.
 * @return true if the directory exists, false otherwise.
 */
bool directory_exists(const char *);

#endif // FILES_API_H