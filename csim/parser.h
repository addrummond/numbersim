#ifndef PARSER_H
#define PARSER_H

#include <config.h>

typedef struct language {
    char name[LANGUAGE_NAME_MAX_LENGTH+1];
    char markers[MARKER_MAX_LENGTH+1][MAX_MARKERS+1];
    unsigned num_markers;
    int n_to_marker[MAX_CARDINALITY];
    // Used if n_to_marker[][] is -1
    unsigned default_marker_index;
} language_t;

void get_languages(const char *filename, language_t *languages);
void test_print_languages(language_t *languages);

#endif
