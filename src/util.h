#pragma once

#include <stdbool.h>

unsigned strhash(const char *str);

bool strequal(const void *str1, const void *str2);

unsigned int ptrhash(const void *ptr);
