#include <sys/stat.h>
#include <stdbool.h>
#include "files_api.h"

bool file_exists(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) != 0) {
        return false;
    }
    return S_ISREG(buffer.st_mode);
}

bool directory_exists(const char* path) {
    struct stat buffer;
    if (stat(path, &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
}