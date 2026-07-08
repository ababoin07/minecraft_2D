#ifndef FILES_API_H
#define FILES_API_H

#include <sys/stat.h>
#include <stdbool.h>

bool file_exists(const char*);
bool directory_exists(const char*);

#endif // FILES_API_H