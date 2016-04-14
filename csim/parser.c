#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <parser.h>
#include <config.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

void get_languages(const char *filename, language_t *languages)
{
    FILE *f = fopen(filename, "r");
    if (! f) {
        fprintf(stderr, "Error opening %s\n", filename);
        exit(1);
    }

    unsigned current_languages_index = 0;
    language_t *current_language = languages;
    for (unsigned i = 0; i < MAX_CARDINALITY; ++i)
        current_language->n_to_marker[i] = -1;
    current_language->default_marker_index = -1;
    unsigned current_name_index = 0;
    char *current_marker = current_language->markers[0];
    unsigned current_marker_index = 0;
    unsigned markers_index = 0;
    char current_num[10]; // Should be plenty of digits for any sensible value of MAX_CARDINALITY.
                          // (Program will quit with error if number doens't fit.)
    unsigned current_num_index = 0;
    char state = 'i';
    unsigned line = 1;
    unsigned col = 1;
    for (;;) {
        if (current_languages_index >= MAX_LANGUAGES) {
            fprintf(stderr, "Too many languages\n");
            goto err;
        }

        int ci = fgetc(f);
        if (ci != EOF && ci < 0) {
            fprintf(stderr, "Error reading %s\n", filename);
            goto err;
        }
        char c = (char)ci;
        ++col;

        if (ci == EOF)
            break; // We check the final state outside the loop to see
                   // if the end of the file was unexpected.

        ++col;
        if (c == '\n') {
            ++line;
            col = 0;
        }

        if (state == 'r') {
            if (current_language->default_marker_index == -1) {
                fprintf(stderr, "[0] No default marker set for language %s\n", current_language->name);
                goto err;
            }
            for (unsigned i = 0; i < MAX_CARDINALITY; ++i) {
                if (current_language->n_to_marker[i] == -1)
                    current_language->n_to_marker[i] = current_language->default_marker_index;
            }

            current_language->num_markers = markers_index;
            ++current_languages_index;
            current_language = languages + current_languages_index;
            for (unsigned i = 0; i < MAX_CARDINALITY; ++i)
                current_language->n_to_marker[i] = -1;

            state = 'i';
            current_name_index = 0;
            current_marker_index = 0;
            markers_index = 0;
            current_marker = current_language->markers[0];
            current_num_index = 0;
        }

        if (state == 'i' && isalpha(c)) {
            if (current_name_index + 1 >= LANGUAGE_NAME_MAX_LENGTH) {
                fprintf(stderr, "[1] Language name too long in %s\n, line %i col %i", filename, line, col);
                goto err;
            }
            current_language->name[current_name_index++] = c;
        }
        else if (state == 'i' && isspace(c)) {
            current_language->name[current_name_index] = '\0';
            state = 's';
        }
        else if (state == 's' && c == '\n') {
            state = 'r';
        }
        else if (state == 's' && isspace(c)) {
            ;
        }
        else if (state == 's' && isalpha(c)) {
            current_marker[current_marker_index++] = c;
            state = 'm';
        }
        else if (state == 's') {
            fprintf(stderr, "[2] Unexpected character '%c' in %s, line %i col %i\n", c, filename, line, col);
            goto err;
        }
        else if (state == 'm' && isalpha(c)) {
            if (current_marker_index + 1 >= MARKER_MAX_LENGTH) {
                fprintf(stderr, "[3] Marker name too long in %s, line %i col %i\n", filename, line, col);
                goto err;
            }
            current_marker[current_marker_index++] = c;
        }
        else if (state == 'm' && isspace(c)) {
            current_marker[current_marker_index] = '\0';
            current_marker_index = 0;
            ++markers_index;
            current_marker = current_language->markers[markers_index];
            state = 't';
        }
        else if (state == 'm') {
            fprintf(stderr, "[4] Unexpected character in %s, line %i col %i\n", filename, line, col);
            goto err;
        }
        else if (state == 't' && isspace(c)) {
            ;
        }
        else if (state == 't' && isdigit(c)) {
            current_num[current_num_index++] = c;
            state = 'n';
        }
        else if (state == 't' && isalpha(c)) {
            current_marker[current_marker_index++] = c;
            state = 'm';
        }
        else if (state == 't' && c == '*') {
            current_language->default_marker_index = markers_index-1;
            state = 's';
        }
        else if (state == 't') {
            fprintf(stderr, "[5] Unexpected character '%c' in %s, line %i col %i\n", c, filename, line, col);
            goto err;
        }
        else if (state == 'n' && isdigit(c)) {
            if (current_num_index + 1 > sizeof(current_num)/sizeof(char)) {
                fprintf(stderr, "[6] Cardinality too big in %s, line %i col %i\n", filename, line, col);
                goto err;
            }
            current_num[current_num_index++] = c;
        }
        else if (state == 'n' && (isspace(c) || c == '\n')) {
            current_num[current_num_index] = '\0';
            int n = atoi(current_num) - 1;
            current_language->n_to_marker[n] = markers_index-1;
            current_num_index = 0;
            if (c == '\n')
                state = 'r';
            else
                state = 't';
        }
    }

    if (state != 's' && state != 'r' && state != 't' && (! (state == 'i' && line == 1 && col == 0))) {
        fprintf(stderr, "[8] Unexpected end of file (%c)\n", state);
        goto err;
    }

    if (current_languages_index > 0) {
        current_language->num_markers = markers_index;
        if (current_language->default_marker_index == -1) {
            fprintf(stderr, "[9] No default marker set for language %s\n", current_language->name);
            goto err;
        }
        for (unsigned i = 0; i < MAX_CARDINALITY; ++i) {
            if (current_language->n_to_marker[i] == -1)
                current_language->n_to_marker[i] = current_language->default_marker_index;
        }
    }

    languages[current_languages_index+1].name[0] = '\0';

    return;

err:
    fclose(f);
    exit(1);
}

void test_print_languages(language_t *languages)
{
    for (unsigned i = 0; languages[i].name[0]; ++i) {
        printf("%s [%i] (def = %s) ", languages[i].name, languages[i].num_markers, languages[i].markers[languages[i].default_marker_index]);
        for (unsigned j = 0; j < languages[i].num_markers; ++j) {
            printf("%s ", languages[i].markers[j]);
        }
        printf ("> ");
        for (unsigned j = 0; j < MAX_CARDINALITY; ++j) {
            int marker_index = languages[i].n_to_marker[j];
            assert(marker_index >= 0);
            printf("%s ", languages[i].markers[languages[i].n_to_marker[j]]);
        }
        printf("\n");
    }
}
