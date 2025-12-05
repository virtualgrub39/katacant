#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ERROR(msg) fprintf (stderr, "Error: %s\n", msg)
#define FERROR(fmt, ...) fprintf (stderr, "Error: " fmt "\n", __VA_ARGS__)
#define FATAL(msg)                                                                                                     \
    {                                                                                                                  \
        fprintf (stderr, "Fatal error: %s\n", msg);                                                                    \
        abort ();                                                                                                      \
    }

#define DA_INITIAL_CAP 32

#define DA(T)                                                                                                          \
    struct                                                                                                             \
    {                                                                                                                  \
        T *items;                                                                                                      \
        unsigned cap, len;                                                                                             \
    }

#define DA_RESERVE(da, new_cap)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((new_cap) > (da)->cap)                                                                                     \
        {                                                                                                              \
            if ((da)->cap == 0) (da)->cap = DA_INITIAL_CAP;                                                            \
            while ((new_cap) > (da)->cap) (da)->cap *= 2;                                                              \
            (da)->items = realloc ((da)->items, (da)->cap * sizeof (*(da)->items));                                    \
            if (!(da)->items) FATAL ("MALLOC FAILED");                                                                 \
        }                                                                                                              \
    } while (0)

#define DA_APPEND(da, v)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        DA_RESERVE ((da), (da)->len + 1);                                                                              \
        (da)->items[(da)->len++] = (v);                                                                                \
    } while (0)

#define DA_APPEND_MANY(da, vs, n)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        DA_RESERVE ((da), (da)->len + (n));                                                                            \
        memcpy ((da)->items + (da)->len, (vs), (n) * sizeof (*(da)->items));                                           \
        (da)->len += (n);                                                                                              \
    } while (0)

#define DA_FREE(da)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((da)->items) free ((da)->items);                                                                           \
        (da)->items = NULL;                                                                                            \
        (da)->cap = 0;                                                                                                 \
        (da)->len = 0;                                                                                                 \
    } while (0)

typedef DA (char) string;
typedef DA (string) string_array;

enum gamemode mode;
bool use_colors = true;

struct query
{
    string key;
    string_array values;
};

typedef DA (struct query) query_array;

void
query_append_value (struct query *q, string v)
{
    DA_APPEND (&q->values, v);
}

void
query_free (struct query q)
{
    DA_FREE (&q.key);
    for (unsigned i = 0; i < q.values.len; ++i) { DA_FREE (&q.values.items[i]); }
    DA_FREE (&q.values);
}

void
mod_bold (void)
{
    if (!use_colors) return;
    printf ("\e[1m");
    fflush (stdout);
}

void
mod_inverted (void)
{
    if (!use_colors) return;
    printf ("\e[7m");
    fflush (stdout);
}

void
mod_reset (void)
{
    if (!use_colors) return;
    printf ("\e[0m");
    fflush (stdout);
}

void
fg_set (int color)
{
    if (!use_colors) return;
    printf ("\e[38;5;%dm", color);
    fflush (stdout);
}

void
fg_reset (void)
{
    if (!use_colors) return;
    printf ("\e[39m");
    fflush (stdout);
}

void
cursor_up (int lines)
{
    if (!use_colors) return;
    printf ("\e[%dA", lines); // move up
    fflush (stdout);
}

bool
query_ask (struct query q)
{
    mod_bold ();
    printf (">> ");
    printf ("%s\n", q.key.items);
    printf ("<< ");
    mod_reset ();

    mod_inverted ();
    char inputbuf[256];
    if (!fgets (inputbuf, sizeof inputbuf, stdin)) return false;
    size_t L = strlen (inputbuf);
    if (L && inputbuf[L - 1] == '\n') inputbuf[L - 1] = '\0';
    mod_reset ();

    bool correct = false;
    for (unsigned i = 0; i < q.values.len; ++i)
    {
        if (strcasecmp (inputbuf, q.values.items[i].items) == 0)
        {
            correct = true;
            break;
        }
    }

    if (use_colors)
    {
        cursor_up (1);

        if (correct)
            fg_set (82);
        else
            fg_set (124);

        printf (">> %s\n", inputbuf);

        if (!correct)
        {
            fg_set (93);
            printf ("[ ");
            for (unsigned i = 0; i < q.values.len; ++i) printf ("%s ", q.values.items[i].items);
            printf ("]\n");
        }

        fg_reset ();
    }
    else
    {
        if (correct)
            printf ("CORRECT\n");
        else
            printf ("INCORRECT");
    }

    return correct;
}

unsigned
ask_queries (query_array q, unsigned n)
{
    if (q.len == 0 || n == 0) return 0;

    unsigned sum = 0;
    unsigned len = q.len;
    unsigned k = (n < len) ? n : len;

    unsigned *idx = malloc (len * sizeof *idx);
    if (!idx) return 0;

    for (unsigned i = 0; i < len; ++i) idx[i] = i;

    for (unsigned i = 0; i < k; ++i)
    {
        unsigned j = i + (unsigned)(rand () % (len - i));
        unsigned tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }

    for (unsigned i = 0; i < k; ++i) { sum += query_ask (q.items[idx[i]]) ? 1 : 0; }

    if (n > len)
    {
        unsigned extra = n - len;
        for (unsigned i = 0; i < extra; ++i)
        {
            unsigned j = (unsigned)(rand () % len);
            sum += query_ask (q.items[j]) ? 1 : 0;
        }
    }

    free (idx);
    return sum;
}

enum gamemode
{
    MODE_INVALID = 0,
    MODE_KATAKANA_SHRIMPLE,
    MODE_KATAKANA_FANCY,
    MODE_KATAKANA_TRANSCRIPTION,

    MODE_COUNT,
};

static const struct
{
    const char *name;
    const char *description;
    const char *datapath;
} gamemode_description[] = {
    [MODE_KATAKANA_SHRIMPLE] = { "katakana", "basic katakana quiz", "data/katakana-simple.quiz" },
    [MODE_KATAKANA_FANCY] = { "katakana-fancy", "fancy katakana quiz", "data/katakana-fancy.quiz" },
    [MODE_KATAKANA_TRANSCRIPTION]
    = { "katakana-transcription", "katakana to romaji transcription quiz", "data/katakana-transcription.csv" },
};

static enum gamemode
gamemode_from_string (const char *smode)
{
    for (unsigned i = 1; i < MODE_COUNT; ++i)
    {
        if (strcasecmp (smode, gamemode_description[i].name) == 0) return i;
    }

    return MODE_INVALID;
}

#define TOKEN_KINDS                                                                                                    \
    X (STRING)                                                                                                         \
    X (COLON)                                                                                                          \
    X (COMMA)                                                                                                          \
    X (SEMICOLON)

#define X(t) TOKEN_##t,
enum token_kind
{
    TOKEN_KINDS
};
#undef X

#define X(t) #t,
static const char *token_strings[] = { TOKEN_KINDS };
#undef X

struct token
{
    enum token_kind kind;
    string s;
    unsigned line, column;
};

typedef DA (struct token) token_array;

bool
gamedata_tokenize (FILE *data, token_array *out_tokens)
{
    unsigned line = 1, column = 1;

    for (;;)
    {
        int c = fgetc (data);
        if (c == EOF) break;

        if (isspace (c))
        {
            if (c == '\n')
            {
                line += 1;
                column = 1;
            }
            else
                column += 1;

            continue;
        }

        struct token t = { .line = line, .column = column, .s = { 0 } };

        switch (c)
        {
        case ',':
            t.kind = TOKEN_COMMA;
            column += 1;
            break;
        case ';':
            t.kind = TOKEN_SEMICOLON;
            column += 1;
            break;
        case ':':
            t.kind = TOKEN_COLON;
            column += 1;
            break;
        default: {
            string s = { 0 };

            while (c != EOF && c != ',' && c != ';' && c != ':')
            {
                column += 1;
                DA_APPEND (&s, c);
                c = fgetc (data);
            }
            if (c != EOF) ungetc (c, data);

            c = 0;
            DA_APPEND (&s, c);

            t.kind = TOKEN_STRING;
            t.s = s;

            break;
        }
        }
        DA_APPEND (out_tokens, t);
    }
    return true;
}

bool
token_expect (token_array tokens, unsigned *index, enum token_kind kind, struct token *out_token)
{
    if (*index >= tokens.len) return false;
    if (tokens.items[*index].kind != kind)
    {
        FERROR ("Unexpected token in gamemode data at %s:%u:%u: Expected %s, got %s\n",
                gamemode_description[mode].datapath, tokens.items[*index].line, tokens.items[*index].column,
                token_strings[kind], token_strings[tokens.items[*index].kind]);
        return false;
    }
    *index += 1;
    *out_token = tokens.items[*index - 1];
    return true;
}

bool
token_accept (token_array tokens, unsigned *index, enum token_kind kind, struct token *out_token)
{
    if (*index >= tokens.len) return false;
    if (tokens.items[*index].kind != kind) return false;
    *index += 1;
    *out_token = tokens.items[*index - 1];
    return true;
}

bool
token_peek (token_array tokens, unsigned index, struct token *out_token)
{
    if (index >= tokens.len) return false;
    *out_token = tokens.items[index];
    return true;
}

bool
gamedata_parse_row (token_array tokens, unsigned *i, query_array *out_queries) // key: value [, value]* ;
{
    struct token t;
    if (!token_expect (tokens, i, TOKEN_STRING, &t)) return false;

    struct query q = { .key = t.s, .values = { 0 } };

    if (!token_expect (tokens, i, TOKEN_COLON, &t)) return false;

    if (!token_expect (tokens, i, TOKEN_STRING, &t)) return false;

    query_append_value (&q, t.s);

    for (;;)
    {
        if (!token_accept (tokens, i, TOKEN_COMMA, &t)) break;
        if (!token_expect (tokens, i, TOKEN_STRING, &t))
        {
            query_free (q);
            return false;
        }
        query_append_value (&q, t.s);
    }

    if (!token_expect (tokens, i, TOKEN_SEMICOLON, &t))
    {
        query_free (q);
        return false;
    }

    DA_APPEND (out_queries, q);
    return true;
}

bool
gamedata_parse (token_array tokens, query_array *out_queries) // [row]*
{
    unsigned i = 0;

    while (i < tokens.len)
    {
        struct token t;
        token_peek (tokens, i, &t);

        if (t.kind != TOKEN_STRING)
        {
            FERROR ("Unexpected token in gamemode data at %s:%u:%u: Expected %s, got %s\n",
                    gamemode_description[mode].datapath, t.line, t.column, token_strings[TOKEN_STRING],
                    token_strings[t.kind]);
            return false;
        }

        if (!gamedata_parse_row (tokens, &i, out_queries)) return false;
    }

    return true;
}

bool
gamemode_load (query_array *out_queries)
{
    FILE *data = fopen (gamemode_description[mode].datapath, "rb");
    if (!data)
    {
        ERROR ("Failed to open gamemode data.");
        perror (NULL);
        return false;
    }

    token_array tokens = { 0 };
    if (!gamedata_tokenize (data, &tokens))
    {
        fclose (data);
        return false;
    }

    fclose (data);

    if (!gamedata_parse (tokens, out_queries))
    {
        DA_FREE (&tokens);
        return false;
    }

    DA_FREE (&tokens);

    return true;
}

void
gamemode_free (query_array *queries)
{
    for (unsigned i = 0; i < queries->len; ++i) { query_free (queries->items[i]); }
    DA_FREE (queries);
}

static void
print_usage (const char *progname)
{
    printf ("katacan't - katakana learning program :3. Copyright 2025 virtualgrub39\n");
    printf ("USAGE:\n");
    printf ("\t%s MODE <OPTIONS>\n", progname);
    printf ("MODE:\n");
    for (unsigned i = 1; i < MODE_COUNT; ++i)
        printf ("\t%-25s - %s\n", gamemode_description[i].name, gamemode_description[i].description);
    printf ("OPTIONS:\n");
    printf ("\t-n UINT - specify number of questions you want to be asked\n");
    printf ("\t-p      - plain mode - disable color output and fancy text formatting using escape codes\n");
    printf ("\t-s UINT - specify seed for srand()\n");
}

int
main (int argc, char *argv[])
{
    if (!isatty (fileno (stdout))) use_colors = false;
    srand (time (NULL));

    if (argc == 1)
    {
        print_usage (argv[0]);
        return 1;
    }

    if (strcasecmp ("help", argv[1]) == 0)
    {
        print_usage (argv[0]);
        return 0;
    }

    mode = gamemode_from_string (argv[1]);
    if (mode == MODE_INVALID)
    {
        ERROR ("Invalid game mode. Use `help` to display available modes.");
        return 1;
    }

    int n_queries = -1;
    int opt;

    while ((opt = getopt (argc - 1, argv + 1, "n:ps:")) != -1)
    {
        switch (opt)
        {
        case 'n': n_queries = atoi (optarg); break;
        case 'p': use_colors = false; break;
        case 's': srand (atoi (optarg)); break;
        default: /* '?' */ print_usage (argv[0]); return 1;
        }
    }

    query_array queries = { 0 };
    if (!gamemode_load (&queries)) { return 1; }

    n_queries = n_queries > 0 ? n_queries : (int)queries.len;
    int correct = ask_queries (queries, n_queries);

    printf ("Summary: %u/%u correct\n", correct, n_queries);
    if (correct == n_queries) printf ("You're a REAL gamer :3\n");

    gamemode_free (&queries);
    return 0;
}
