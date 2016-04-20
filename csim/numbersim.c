//
// Example invocations:
//
//     numbersim
//
// If invoked with no arguments, arguments are read line-by-line from stdin.
// The corresponding output for each line is written to stdout immediately after
// each line is read.
//
// Arguments (all required):
//
//     1)    Name of file containing language data.
//     2)    First random seed (decimal integer between 0 and (2^64)-1 inclusive)
//     3)    Second reandom seed (decimal integer between 0 and (2^64)-1 inclusive)
//     4)    Language name
//     5)    beta (param to ztnbd, value is 0.6 in original exp; this value ignored unless arg 9 is "ztnbd")
//     6)    r (param to ztnbd, value is 3 in original exp; this value ignored unless arg 9 is "ztnbd")
//     7)    learning rate (typical value is 0.01)
//     8)    Maximum cue cardinality (7 in original experiment).
//     9)    Number of trials to run (decimal integer between 0 and (2^64)-1 inclusive)
//     10)   Output mode (either 'full' or 'summary')
//     11)   If output mode is "summary', quit after all markers have been correct
//           for at least this number of trials. If 0, never quit early.
//           This value is ignored for other output modes.
//
//     Argument (12) should be the first in a series of floating point
//     values. The length of this series must be identical to the maximum
//     cue cardinality. The values are intepreted as p values specifying a
//     probability distribution. The values need not sum to 1.0, since it is
//     assumed that any remaning probability mass is the probability for all
//     cardinalities greater than specified by argument (8).
//

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <parser.h>
#include <pcg_basic.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

typedef enum output_mode {
    OUTPUT_MODE_FULL,
    OUTPUT_MODE_SUMMARY
} output_mode_t;

typedef struct state {
    language_t language;
    uint_fast64_t n_trials;
    uint_fast32_t max_cue;
    double assocs[MAX_CARDINALITY][MAX_MARKERS];
    double compound_cue_assocs[MAX_CARDINALITY][MAX_MARKERS];
    double learning_rate;
    uint32_t thresholds[MAX_CARDINALITY];
    pcg32_random_t rand_state;
    output_mode_t output_mode;
    uint_fast64_t marker_has_been_correct_for_last[MAX_CARDINALITY];
    unsigned quit_after_n_correct;
} state_t;

static void update_state_helper(state_t *state, unsigned marker_index, uint_fast32_t cardinality, double l)
{
    double vax = 0;
    for (unsigned i = 0; i <= cardinality; ++i) {
        vax += state->assocs[i][marker_index];
    }

    double delta_v = state->learning_rate * (l - vax);

    for (unsigned i = 0; i <= cardinality; ++i) {
        //printf("ADDING card=%i, marker=%i, raw_index=%u, +%f [%f]\n", i, marker_index, (i * state->language.num_markers) + marker_index, delta_v, l);
        state->assocs[i][marker_index] += delta_v;
    }
}

static bool update_state(state_t *state, unsigned marker_index, uint_fast32_t cardinality)
{
    //printf("UPD marker=%u, card=%u\n", marker_index, cardinality);
    for (unsigned i = 0; i < state->language.num_markers; ++i) {
        update_state_helper(state, i, cardinality, i == marker_index ? 1.0 : 0.0);
    }

    for (unsigned i = 0; i < state->max_cue; ++i) {
        double max_sum = 0.0;
        unsigned max_sum_marker_index;
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            double sum = 0;
            for (unsigned k = 0; k <= i; ++k)
                sum += state->assocs[k][j];
            if (sum > max_sum) {
                max_sum = sum;
                max_sum_marker_index = j;
            }
            state->compound_cue_assocs[i][j] = sum;
        }
        if (max_sum_marker_index == state->language.n_to_marker[i])
            ++(state->marker_has_been_correct_for_last[i]);
        else
            state->marker_has_been_correct_for_last[i] = 0;
    }

    if (state->output_mode == OUTPUT_MODE_SUMMARY) {
        // Check if we should quit now.
        for (unsigned i = 0; i < state->max_cue; ++i) {
            if (state->marker_has_been_correct_for_last[i] < state->quit_after_n_correct)
                return true; // Continue running
        }

        return false; // Quit.
    }
    else {
        return true; // Continue running.
    }
}

static void output_headings(const state_t *state)
{
    printf("trial");
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            printf(",%i->%s", i+1, state->language.markers[j]);
        }
        printf(",%i_correct", i+1);
    }
    printf(",seed1,seed2\n");
}

static void output_line(const state_t *state, int marker_index, uint_fast32_t cardinality)
{
    printf("%s %i", (marker_index == - 1 ? "" : state->language.markers[marker_index]), cardinality+1);
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            printf(",%f", state->compound_cue_assocs[i][j]);
        }
        printf(",%llu", state->marker_has_been_correct_for_last[i]);
    }
    printf(",%llu,%llu\n", state->rand_state.state, state->rand_state.inc);
}

static void output_summary(const state_t *state)
{
    // Output the number of trials which it took to get the right marker
    // for each cardinality for a sequence of at least the specified number of
    // trials.
    for (unsigned i = 0; i < state->max_cue; ++i) {
        if (i != 0)
            printf(",");

        uint_fast64_t correct_for = state->marker_has_been_correct_for_last[i];
        if (correct_for < state->quit_after_n_correct)
            // This marker was never correct for the required number of trials.
            printf("-1");
        else
            printf("%llu", state->n_trials - correct_for + state->quit_after_n_correct);
    }

    // Output seed state for random number generator (so that subsequent runs
    // can use them as the starting point).
    printf(",%llu,%llu", state->rand_state.state, state->rand_state.inc);
}

static void run_trials(state_t *state, uint_fast64_t n)
{
    if (state->output_mode == OUTPUT_MODE_FULL)
        output_headings(state);

    uint_fast32_t card = 0;
    int marker_index = -1;
    for (; state->n_trials < n; ++(state->n_trials)) {
        if (state->output_mode == OUTPUT_MODE_FULL)
            output_line(state, marker_index, card);

        uint32_t r = pcg32_random_r(&(state->rand_state));

        // Determine the cardinality of the cue based on the random number.
        for (card = 0; card < state->max_cue && r >= state->thresholds[card]; ++card);

        // Get the appropriate marker for that cardinality.
        marker_index = state->language.n_to_marker[card];
        assert(marker_index >= 0);

        if (! update_state(state, marker_index, card))
            break;
    }

    if (state->output_mode == OUTPUT_MODE_SUMMARY)
        output_summary(state);

    fflush(stdout);
}

#define ARGS_STRING_MAX_LENGTH (1024*8)
#define MAX_ARGS 100

static unsigned string_to_arg_array(char *str, char *arg_array[])
{
    arg_array[0] = str;

    bool in_space = false;
    unsigned aai = 1;
    unsigned i;
    for (i = 0; str[i] && aai < MAX_ARGS; ++i) {
        if (in_space) {
            if (! isspace(str[i])) {
                arg_array[aai++] = str + i;
                in_space = false;
            }
        }
        else {
            if (isspace(str[i])) {
                str[i] = '\0';
                in_space = true;
            }
        }
    }

    if (str[i]) {
        fprintf(stderr, "Too many arguments (max is %u)\n", ARGS_STRING_MAX_LENGTH);
        exit(1);
    }

    return aai;
}

//
// If unsigned long long is smaller than uint64_t, the following array will be
// declared with a negative size, which will give rise to a compile-time error.
//
struct ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT_STRUCT {
    int ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT[(int)sizeof(unsigned long long) - (int)sizeof(uint64_t)];
};

// Guaranteed to be initialized to all zeroes (i.e. empty string).
static char name_of_last_language[LANGUAGE_NAME_MAX_LENGTH];

static void run_given_arguments(int num_args, char **args)
{
    if (num_args < 12) {
        fprintf(stderr, "Not enough arguments\n");
        exit(2);
    }

    static state_t state;
    state.n_trials = 0;

    const char *language_file_name = args[0];

    // Get random seed from first and second arguments.
    uint64_t seed1, seed2;
    if (sscanf(args[1], "%llu", &seed1) < 1) {
        fprintf(stderr, "Error parsing first random seed '%s' (second argument)\n", args[1]);
        exit(3);
    }
    if (sscanf(args[2], "%llu", &seed2) < 1) {
        fprintf(stderr, "Error parsing second random seed '%s' (third argument)\n", args[2]);
        exit(4);
    }

    seed2 |= 1; // Ensure that seed2 is odd, as required by pcg library.
    pcg32_srandom_r(&(state.rand_state), seed1, seed2);

    const char *language_name = args[3];

    double beta, r;
    if (sscanf(args[4], "%lf", &beta) < 1) {
        fprintf(stderr, "Error parsing beta (fifth argument)\n");
        exit(5);
    }
    if (sscanf(args[5], "%lf", &r) < 1) {
        fprintf(stderr, "Error parsing r (sixth argument)\n");
        exit(6);
    }
    if (sscanf(args[6], "%lf", &(state.learning_rate)) < 1) {
        fprintf(stderr, "Error parsing learning_rate (seventh argument)\n");
        exit(7);
    }
    if (state.learning_rate <= 0) {
        fprintf(stderr, "Bad value for learning_rate (must be > 0)\n");
        exit(8);
    }

    if (sscanf(args[7], "%u", &(state.max_cue)) < 1) {
        fprintf(stderr, "Error parsing max_cue (eighth argument)\n");
        exit(9);
    }
    if (state.max_cue == 0) {
        fprintf(stderr, "max_cue (eighth argument) must be greater than 0\n");
        exit(10);
    }
    if (state.max_cue > MAX_CARDINALITY) {
        fprintf(stderr, "Value of max_cue (eighth argument) is too big.\n");
        exit(11);
    }

    uint_fast64_t num_trials;
    if (sscanf(args[8], "%llu", &num_trials) < 1) {
        fprintf(stderr, "Error parsing number of trials (ninth argument)\n");
        exit(12);
    }

    const char *output_mode_string = args[9];
    if (! strcmp(output_mode_string, "full")) {
        state.output_mode = OUTPUT_MODE_FULL;
    }
    else if (! strcmp(output_mode_string, "summary")) {
        state.output_mode = OUTPUT_MODE_SUMMARY;
    }
    else {
        fprintf(stderr, "Bad value for output_mode (tenth argument, should be \"summary\" or \"full\")");
        exit(13);
    }

    if (sscanf(args[10], "%u", &(state.quit_after_n_correct)) < 1) {
        fprintf(stderr, "Bad value for quit_after_n_correct (eleventh argument)\n");
        exit(14);
    }

    const unsigned DIST_ARGI = 11;

    if (num_args != DIST_ARGI + state.max_cue) {
        fprintf(stderr, "Incorrect number of p values for probability distribution (%u given, %u required)\n", num_args-DIST_ARGI, state.max_cue);
        exit(16);
    }
    double total = 0;
    for (unsigned i = 0; i < 0 + state.max_cue; ++i) {
        double p;
        if (sscanf(args[i+DIST_ARGI], "%lf", &p) < 1) {
            fprintf(stderr, "Error parsing probability value.\n");
            exit(17);
        }
        total += p;
        state.thresholds[i] = (uint32_t)(UINT32_MAX * p);
        if (i > 0) {
            state.thresholds[i] += state.thresholds[i-1];
        }
    }

    if (strcmp(name_of_last_language, language_name)) {
        // Will be terminated by language with empty string as name.
        static language_t languages[MAX_LANGUAGES];
        get_languages(language_file_name, languages);
        //test_print_languages(languages);

        // Find the specified language.
        language_t *lang = NULL;
        for (language_t *l = languages; l->name[0] != '\0'; ++l) {
            if (! strcmp(l->name, language_name)) {
                lang = l;
                break;
            }
        }
        if (! lang) {
            fprintf(stderr, "Could not find language %s\n", language_name);
            exit(19);
        }

        memcpy(&state.language, lang, sizeof(language_t));
    }

    double *as = (double *)(state.assocs);
    for (unsigned i = 0; i < sizeof(state.assocs)/sizeof(state.assocs[0][0]); ++i)
        as[i] = 0.0;
    double *cas = (double *)state.compound_cue_assocs;
    for (unsigned i = 0; i < sizeof(state.compound_cue_assocs)/sizeof(state.compound_cue_assocs[0][0]); ++i)
        cas[i] = 0.0;
    memset(state.marker_has_been_correct_for_last, 0, sizeof(state.marker_has_been_correct_for_last));

    run_trials(&state, num_trials);
}

int main(int argc, char *argv[])
{
    // No need to free this as it is used until process exits.
    char *buf = malloc(ARGS_STRING_MAX_LENGTH * sizeof(char));

    if (argc > 1) {
        run_given_arguments(argc - 1, argv + 1);
        printf("\n");
        fflush(stdout);
    }
    else {
        for (;;) {
            size_t sz = ARGS_STRING_MAX_LENGTH * sizeof(char);
            int bytes_read = getline(&buf, &sz, stdin);
            if (bytes_read < 0) {
                if (feof(stdin)) {
                    exit(0);
                }
                else {
                    fprintf(stderr, "Read error\n");
                    exit(20);
                }
            }
            else if (bytes_read > 0) {
                static char *args[MAX_ARGS];
                unsigned num_args = string_to_arg_array(buf, args);
                run_given_arguments(num_args, args);
                printf("\n");
                fflush(stdout);
            }
        }
    }

    return 0;
}
