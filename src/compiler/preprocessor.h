#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stdio.h>

void preprocessor_init(int (*getchar_cb)(void));
int preprocessor_getchar(void);
void preprocessor_cleanup(void);

// For backwards compatibility and desktop use
char* preprocess(const char* source);

#endif
