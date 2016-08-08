/*   
  Inline Headers is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
  Copyright (C) 2016 Levi Webb
    
  Inline Headers (iheaders) is a program to process C source files with inlined header
  information, generating a corresponding header file and stripping the source file of
  the iheaders syntax for compilation.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <assert.h>

#include <time.h>

#include <limits.h>

#include <errno.h>

#include <getopt.h>

#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#define IHEADERS_VERSION "1.1"
#define IHEADERS_SIGNATURE                          \
    "Inline Headers (iheaders) " IHEADERS_VERSION   \
    " -- Copyright (C) 2016 Levi Webb"

#define HELP_OPT_TAB 4
#define HELP_OPT_PARAGRAPH_INDENT 2

/* check for GCC version >= Major.miNor.Patchlevel */
#define GCC_VERSION_COMPARE(M, N, P)  __GNUC__ > M ||                   \
    (__GNUC__ == M && (__GNUC_MINOR__ > N ||                            \
                       (__GNUC_MINOR__ == N &&                          \
                        __GNUC_PATCHLEVEL__ > P)))

static const char* help_desc =
    "Usage: iheaders [OPTION]... [FILES]...\n"
    "Reads header blocks and information that is inlined in C source files.\n"
    "Generates a corresponding '.h' file for every '.c' input by default.\n\n"
    "Available arguments:\n";

/* '\1' defines the start of the description, '\2' indicates a new indented line. */
static const char* help_opts =
    "-h, --help\1show this help and exit\n"
    "-p, --strip\1strip header information from the source files, producing C sources\2"
    "that can be compiled.\n"
    "-v, --verbose\1show detailed information about inline header processing\n"
    "-t, --token=WORD\1sets the token for the processing rules to the specified string\n"
    "-d, --header-dir=PATH\1defines the directory for headers to be placed into\n"
    "-r, --root-dir=PATH\1when accompanied by the 'header-dir' option, this will place\2"
    "headers into the header directory with the same folder\2"
    "structure as their corresponding source files.\n"
    "-s, --single-output=PATH\1provide a file header path for all the provided sources\n"
    "-O, --stdout\1pipe the resulting header into stdout instead.\n"
    "-G, --include-gaurds\1place include gaurds in the resulting header file(s)\n"
    "-I, --tab-indent=SIZE\1defines the amount of spaces that a tab occupies, affecting how\2"
    "header block (@ { ... } syntax) indentation is copied to\2"
    "the resulting header file. Set to 0 to preserve all\2"
    "indentation, the default is 4.\n";

static const char* help_footer = "\n" /* padding from the option list */
    "There are three modes in which you can organize headers generation: directory mode\n"
    "('-r', and '-D' options) - which will organize headers for each source into a set\n"
    "of headers, single-header mode ('-s' option) - which will combine all sources into\n"
    "a single header, and pipe mode ('-O' option) - similar to single-header mode, except\n"
    "the resulting file is piped to stdout.\n\n";

static const char* opt_str = "hvps:t:d:r:I:OG";

static struct option p_opts[] = {
    {"help", no_argument, 0, 'h'},
    {"strip", no_argument, 0, 'p'},
    {"verbose", no_argument, 0, 'v'},
    {"token", required_argument, 0, 't'},
    {"header-dir", required_argument, 0, 'd'},
    {"root-dir", required_argument, 0, 'r'},
    {"include-gaurds", no_argument, 0, 'G'},
    {"single-output", required_argument, 0, 's'},
    {"tab-indent", required_argument, 0, 'I'},
    {"stdout", no_argument, 0, 'O'},
    {0, 0, 0, 0}
};

/* an overcomplicated way to indent opt flags, at least it only uses a single buffer w/o resizing */
static size_t indent_opts_labelsize(void);
static size_t indent_opts_bufsize(size_t max_size);
static char* indent_opts(size_t total_size, size_t max_size, char* buf);

static bool handle_target(char* buf);
static bool handle_open(char* source, char* dest);
static bool parse(FILE* source, FILE* dest, bool strip);

static bool handle_target_set(char** set, size_t nset);

static bool help_mode = false,  /* if true, the help will be displayed and iheaders will exit */
    verbose_mode = false,       /* if true, extra information will be displayed during processing */
    pipe_mode = false,          /* pipe the output will be piped to stdout */
    gaurd_mode = false,         /* place include gaurds in generated header files */
    merge_mode = false,         /* merge the results into one header */
    strip_mode = false;         /* strip mode, instead of extracting header code */

static const char* token = "@", /* token to use in processing */
    * header_dir = NULL,        /* output header directory */
    * root_dir = NULL,          /* root source directory */
    * single_target = NULL;     /* single output header file */

static size_t indent_tab_size = 4;

#define ANY_TWO(X, Y, Z) ((X && Y) || (X && Z) || (Z && Y))

#define BSTR(B) (B == 0 ? "false" : "true")
#define NSTR(B) (B == NULL ? "NULL" : B)

#define ERRNO_CHECK(S, V)                                   \
    do {                                                    \
        if (errno) {                                        \
            fprintf(stderr, S " '%s': %s\n",                \
                   V, strerror(errno));                     \
            exit(EXIT_FAILURE);                             \
        }                                                   \
    } while (false)

int main(int argc, char** argv) {
    
    /* option processing */
    int c, idx = 0, n = 0;
    while ((c = getopt_long(argc, argv, opt_str, p_opts, &idx)) != -1) {
        switch (c) {
        case 'v':
            verbose_mode = true;
            break;
        case 'p':
            strip_mode = true;
            break;
        case 't':
            token = optarg;
            break;
        case 'd':
            header_dir = optarg;
            break;
        case 'r':
            root_dir = optarg;
            break;
        case 'G':
            gaurd_mode = true;
            break;
        case 's':
            merge_mode = true;
            single_target = optarg;
            break;
        case 'I':
            indent_tab_size = atoi(optarg);
            break;
        case 'O':
            merge_mode = true;
            pipe_mode = true;
            break;
        case '?':
            exit(EXIT_FAILURE);
        default:
        case 'h':
            help_mode = true;
        }
        n++;
    }

    /* if two or more modes are enabled, complain and then exit. */
    if (ANY_TWO(single_target != NULL, (header_dir != NULL || root_dir != NULL), pipe_mode)) {
        fprintf(stderr, "error: the pipe mode ('-O' option), directory mode "
                "('-r', and '-d' options), and single-header mode ('-s' option) "
                "cannot be used together.\n");
        exit(EXIT_FAILURE);
    }

    /* if directory mode is being used and we were supplied with a root source directory,
       we need a header directory too. */
    if (root_dir != NULL && header_dir == NULL) {
        fprintf(stderr, "error: header directory ('-d' option) must be specified "
                "with the root source directory\n");
        exit(EXIT_FAILURE);
    }

    /* if no arguments were provided, assume help mode. */
    if (argc == 1) {
        help_mode = true;
    }

    /* if no target files were provided, complain and exit. */
    if (argc - optind == 0 && !help_mode) {
        fprintf(stderr, "error: no source files provided\n");
        exit(EXIT_FAILURE);
    }
    
    if (verbose_mode) {
        printf("options (%d) -> help_mode=%s, verbose_mode=%s, pipe_mode=%s, "
               "token=%s, header_dir=%s, root_dir=%s, merge_mode=%s, "
               "strip_mode=%s\n",
               n, BSTR(help_mode), BSTR(verbose_mode), BSTR(pipe_mode),
               token, NSTR(header_dir), NSTR(root_dir), BSTR(merge_mode),
               BSTR(strip_mode));
    }

    /* display help */
    if (help_mode) {
        /* basic description */
        fputs(help_desc, stdout);
        /* calculate the maximum 'label' size of the argument list */
        size_t max_size = indent_opts_labelsize();
        /* calculate the buffer size needed for the processed argument list */
        size_t total_size = indent_opts_bufsize(max_size);
        /* create stack buffer */
        char buf[total_size + 1];
        /* process argument list and copy into buffer */
        indent_opts(total_size, max_size, buf);
        /* display formatted argument list */
        fputs(buf, stdout);
        /* display help footer */
        fputs(help_footer, stdout);
        /* display help signature */
        fputs(IHEADERS_SIGNATURE "\n", stdout);
        /* flush and exit */
        fflush(stdout);
        exit(EXIT_SUCCESS);
    }

    /* select target files from arguments normally and process them */
    if (!merge_mode) {
        size_t t;
        for (t = optind; t < argc; t++) {
            if (strlen(argv[t]) > 0 && argv[t][0] != '-') {
                if (verbose_mode) {
                    printf("processing: %s\n", argv[t]);
                }
                if (!handle_target(argv[t])) {
                    fprintf(stderr, "failed to process target: '%s'\n", argv[t]);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    /* select all target files to be merged into a single header */
    else {
        if (!handle_target_set(&argv[optind], argc - optind)) {
            fprintf(stderr, "error while processing target set, exiting.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* checks if a stream is valid, obtains information about the stream if errno is set,
   then complains to stderr with strerror() and exits. */
static void check_stream(FILE* stream) {
    int fd = fileno(stream);
    if (fd == -1) {
        fprintf(stderr, "error while reading from stream: invalid stream (%p)", stream);
        exit(EXIT_FAILURE);
    }
    if (errno != 0) {
        char fbuf[PATH_MAX];
        char pbuf[PATH_MAX];
        snprintf(pbuf, PATH_MAX, "/proc/self/fd/%d", fd);
        readlink(pbuf, fbuf, PATH_MAX);
        ERRNO_CHECK("error while reading from stream", fbuf);
    }
}

static void get_file_desc(FILE* stream, char* fbuf) {
    int fd = fileno(stream);
    if (fd == -1) {
        memcpy(fbuf, "<invalid>", 9 * sizeof(char));
        return;
    }
    char pbuf[PATH_MAX];
    snprintf(pbuf, PATH_MAX, "/proc/self/fd/%d", fd);
    ssize_t t = readlink(pbuf, fbuf, PATH_MAX);
    fbuf[t] = '\0';
    ERRNO_CHECK("error while reading from stream", fbuf);
}

static void emit_line(FILE* stream, int line, const char* file) {
    char lineb[24 + strlen(file)];
    snprintf(lineb, sizeof(lineb) / sizeof(char), "#line %d \"%s\"\n", line, file);
    fputs(lineb, stream);
}

#define PARSE_UNKNOWN 0
#define PARSE_HEADER_PREFIX 1
#define PARSE_SOURCE_PREFIX 2
#define PARSE_BLOCK 3
#define PARSE_MEMBER 4

#define ALIGN_LINES() \
    do { if (!strip) { emit_line(dest, l, source_name); } } while (false)

/* local to process and strip functions */
#define PARSE_ERR(V, ...) fprintf(stderr, "syntax error [%d:%d] - " V "\n", line, col, ##__VA_ARGS__)
#define PARSE_INFO(V, ...)                                          \
    do {                                                            \
        if (verbose_mode) {                                         \
            printf("[PARSE][%d:%d] " V "\n", line, col, ##__VA_ARGS__);  \
        }                                                           \
    } while (false)

/* GCC punishes my monolithic parsing functions by complaining about
   potentially uninitialized variables. This fixes that. */
#if GCC_VERSION_COMPARE(4, 6, 4)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

/* process the given source file stream, and pipe the resulting header information into 'dest' */
static bool parse(FILE* source, FILE* dest, bool strip) {

    /*
      If you can follow the control flow of this function, you are lying.
     */

    char source_name[PATH_MAX];
    get_file_desc(source, source_name);
    
    if (verbose_mode) {
        char dest_name[PATH_MAX];
        get_file_desc(dest, dest_name);
        printf("[PARSE] starting parse for %s -> %s\n", source_name, dest_name);
    }
    
    char buf[128];           /* input buffer */
    bool line_start = true,  /* while searching for a token, this is set to true if the index
                               is the start of a line */
        parse_mode = false,  /* true if a token is being parsed, false if searching for token */
        prefix_set = false,  /* if the header prefix was already set for a token */
        using_attrs = false, /* if ma_buf (allocated buffer) is being used for attributes */
        b_a;                 /* multi-purpose flags */
    size_t t,                /* index in 'buf' */
        token_size = strlen(token),
        a, b, c,             /* multi-purpose variables (usually indexes) used while parsing */
        l;                   /* recorded line position for emitting #line directives */
    
    uint8_t parse_mode_flag = 0;   /* while parsing a token, this is set to the parse state */
    char m_buf[512];               /* multi-purpose buffer */
    char* ma_buf = NULL;           /* multi-purpose allocated buffer, used to store header and attr blocks */
    size_t ma_size = 0;
    char set_prefix_buf[128];      /* the universal header prefix for this source file */
    char set_source_buf[128];      /* the universal source prefix for this source file */
    char prefix_buf[128];          /* prefix set for a specific member */
    char source_buf[128];
    set_prefix_buf[0] = '\0';
    set_source_buf[0] = '\0';
    char* prefix = set_prefix_buf; /* pointer to which prefix buffer to use for a token */
    char* sprefix = set_source_buf; /* pointer to which source prefix buffer to use */

    bool copying = true, skip_char = false;

    /* emit #line directive */
    if (strip) {
        /*
          Due to how stripping works, line breaks are placed where
          iheader syntax originally was in the destination stream,
          so all we have to do is place a single #line directive.
         */
        emit_line(dest, 1, source_name);
    } /* for non-strip parse scenarios, we add the directives while parsing */
    
    int line = 1, col = 1;
    size_t read_chars, token_read_idx = 0;
    while (!feof(source)) {
        read_chars = fread(buf, sizeof(char), 128, source);
        check_stream(source);
        
        for (t = 0; t < read_chars; ++t) {
            
            /* keep track of line number and characters regardless of parsing state */
            if (buf[t] == '\n') {
                col = 0;
                ++line;
            }
            else ++col;
            
            /* looking for a new token */
            if (!parse_mode) {
                /* if at the start of a line, or currently comparing a token, compare characters */
                if (line_start || token_read_idx > 0) {
                    if (buf[t] == token[token_read_idx]) {
                        ++token_read_idx;
                        copying = false;
                    }
                    else {
                        token_read_idx = 0;
                        copying = true;
                    }
                    
                    if (token_read_idx == token_size) {
                        PARSE_INFO("parsing token");
                        parse_mode = true;
                        parse_mode_flag = PARSE_UNKNOWN;
                    }
                }
            }
            /* currently parsing after a token */
            else {
                switch (parse_mode_flag) {
                case PARSE_UNKNOWN: /* unknown state, expecting a block, prefix, suffix, or member */
                    switch (buf[t]) {
                    case '{':
                        PARSE_INFO("starting header block");
                        parse_mode_flag = PARSE_BLOCK;
                        b = 0;     /* indentation level starts at 0 */
                        c = 0;     /* index for ma_buf */
                        /* allocate ma_buf */
                        if (ma_buf != NULL) {
                            free(ma_buf); /* free previous */
                        }
                        ma_buf = malloc(sizeof(char) * 64);
                        ma_size = 64;
                        /* flag that no characters have yet followed the '{' */
                        b_a = false;
                        break;
                    case '(':
                        /* set 'a' to 1, used for tracking paren levels (...) */
                        a = 1;
                        goto pre;
                    case '[':
                        /* set 'a' to 0, we don't track levels for square brackets */
                        a = 0;
                    pre:
                        if (prefix_set) {
                            PARSE_INFO("reading source prefix");
                            parse_mode_flag = PARSE_SOURCE_PREFIX;
                        }
                        else {
                            PARSE_INFO("reading header prefix");
                            parse_mode_flag = PARSE_HEADER_PREFIX;
                            prefix_set = true;
                        }
                        /* set 'b' to 0, used to index m_buf */
                        b = 0;
                        break;
                    case '\t':
                    case ' ':
                        break; /* ignore spacing and tabbing after token */
                    case '=':
                    case ';':/* these usually determine the end of a member declaration, we
                                shouldn't be seeing these at this point. */
                    case ')':
                    case ']':
                    case '}': /* unexpected closing token, shouldn't be here. */
                        
                        PARSE_ERR("expected '{', '[', '(', or start of member after '%s' token", token);
                        return false;
                    case '\n':
                        if (!prefix_set) {
                            /* special case, if there's a newline right after the token, treat as if it
                               doesn't exist. */
                            break;
                        }
                        else {
                            PARSE_INFO("setting global header and source prefixes");
                            /* token followed by prefix/suffix setting(s), set permenently. */
                            memcpy(set_prefix_buf, prefix_buf, 128);
                            memcpy(set_source_buf, source_buf, 128);
                            parse_mode = false;
                        }
                        break;
                    default:
                        /* when we hit any other character, we assume it's the start of a member */
                        if (!strip) {
                            /* set 'b' to 1, used to index m_buf (0 is the current character) */
                            b = 1;
                            /* copy over the first character */
                            m_buf[0] = buf[t];
                            
                            parse_mode_flag = PARSE_MEMBER;
                            l = line;
                        }
                        else { /* we don't need to read into the declaration to strip it */
                            
                            /* write source prefix */
                            if (sprefix != NULL && *sprefix != '\0') {
                                fputs(sprefix, dest);
                                fputc(' ', dest);
                            }
                            parse_mode = false;
                        }
                    }
                    break;
                case PARSE_SOURCE_PREFIX: /* parsing @[][...] data */
                case PARSE_HEADER_PREFIX: /* parsing @[...][] data */
                                          /* can be @(...)(...)    */
                    switch (buf[t]) {
                    case ')':
                        /* closing paren */
                        if (a == 1) {
                            goto end_pre;
                        }
                        /* if not 0 (to not track levels), deincrement and copy */
                        else if (a > 0) {
                            a--;
                            goto copy_pre;
                        }
                    case ']':
                        /* closing square bracket during (...), just copy */
                        if (a > 0) {
                            goto copy_pre;
                        }
                    end_pre:;
                        const bool is_header = parse_mode_flag == PARSE_HEADER_PREFIX;
                        char* obuf = (is_header ? prefix_buf : source_buf),
                            * m_buf_ptr = m_buf;

                        /* parse out :attr,...: syntax */

                        if (strip || !is_header)
                            goto after_parse;
                        
                        if (ma_buf != NULL) {
                            free(ma_buf);
                            ma_size = 0;
                            ma_buf = NULL;
                        }
                        
                        char* pc, * last_pc;
                        bool parsing_attribute = false, even = true;
                        for (pc = m_buf; pc < m_buf + b; ++pc) {
                            char ac = *pc;
                            if (parsing_attribute) {
                                switch (ac) {
                                case '\0':
                                    break;
                                case ':':
                                    even = true;
                                    m_buf_ptr = pc + 1;
                                case ',':
                                    /* append to ma_buf, split on \1, terminated on \0 */
                                    if (last_pc != pc) {
                                        
                                        size_t l = (pc - last_pc) * sizeof(char) + 1;
                                        if (ma_buf == NULL) ma_buf = malloc(l);
                                        else ma_buf = realloc(ma_buf, l + ma_size);
                                        
                                        if (ma_size > 0) /* overwrite last \0 to \1 */
                                            ma_buf[ma_size - 1] = '\1';

                                        /* copy over attribute to the end of the buffer */
                                        memcpy(ma_buf + ma_size, last_pc, l - 1);
                                        /* update buffer size */
                                        ma_size += l;
                                        /* null-terminate */
                                        ma_buf[ma_size - 1] = '\0';
                                        
                                        PARSE_INFO("appended '%.*s' to ma_buf for __attribute__",
                                                   (int) (l - 1), last_pc);
                                    }
                                    if (even) goto after_parse;
                                    last_pc = pc + 1;
                                    break;
                                }
                            } else if (ac == ':') {
                                even = false;
                                parsing_attribute = true;
                                last_pc = pc + 1;
                                continue;
                            }
                        }

                        if (!even)
                            PARSE_ERR("expected ':' before end of header prefix while parsing attribute");
                        
                    after_parse:
                        using_attrs = ma_buf != NULL;
                        /* copy over data from m_buf */
                        size_t nb = b - (m_buf_ptr - m_buf);
                        memcpy(obuf, m_buf_ptr, nb);
                        obuf[nb] = '\0';
                        *(is_header ? &prefix : &sprefix) = obuf;
                        PARSE_INFO("copied %s prefix '%s'", (is_header ? "header" : "source"), obuf);
                        parse_mode_flag = PARSE_UNKNOWN;
                        break;
                        
                    case '(':
                        /* if parsing (...), track levels */
                        if (a > 0) {
                            ++a;
                        }
                        /* copy regardless */
                        goto copy_pre;
                    case '[':
                        /* opening square bracket during (...), copy */
                        if (a > 0) {
                            goto copy_pre;
                        }
                        /* unexpected start of square brackets */
                        PARSE_ERR("unexpected '[' while parsing prefixes");
                        return false;
                    case '\n':
                        /* newline occurred inside of square brackets */
                        PARSE_ERR("unexpected newline while parsing prefixes");
                        return false;
                    default:
                    copy_pre:
                        /* detect overflow (one less, since we need a null-terminating character) */
                        if (b == 126) {
                            PARSE_ERR("prefix's content too large [max: 126 characters]");
                            return false;
                        }
                        /* copy character to m_buf */
                        m_buf[b] = buf[t];
                        ++b;
                    }
                    break;
                case PARSE_BLOCK: /* parsing @ { ... } block */
                    switch (buf[t]) {
                    case '{':
                        /* increase indentation level */
                        ++b;
                        /* flag that character has followed the first '{' */
                        if (!b_a) {
                            l = line;
                            b_a = true;
                        }
                        goto cpy_char;
                    case '}':
                        /* closing indentation, end of block -- write everything to the header */
                        if (b == 0) {
                            
                            PARSE_INFO("end of header block");
                            
                            /* if we're stripping, just ignore the entire block */
                            if (strip) {
                                
                                /* copy newlines from the block, to keep spacing */
                                size_t idx;
                                for (idx = 0; idx < c; ++idx) {
                                    if (ma_buf[idx] == '\n') {
                                        fputc('\n', dest);
                                    }
                                }
                                parse_mode = false;
                                skip_char = true; /* skip the final } character */
                                break;
                            }
                            
                            /* find the lowest amount of indentation that precedes a line */
                            size_t least_num_spaces = 0, idx;
                            if (indent_tab_size > 0) {
                                size_t num_spaces = 0;
                                bool reading_start = true, measure_start = true;
                                for (idx = 0; idx < c; ++idx) {
                                    if (reading_start) {
                                        switch (ma_buf[idx]) {
                                        case ' ':
                                            ++num_spaces;
                                            break;
                                        case '\t':
                                            num_spaces += 4;
                                            break;
                                        case '\n':
                                            goto advance;
                                        default:
                                            reading_start = false;
                                        }
                                    }
                                    else if (ma_buf[idx] == '\n') {
                                    advance: {
                                            /* record the amount of spacing */
                                            if (least_num_spaces > num_spaces || measure_start) {
                                                least_num_spaces = num_spaces;
                                                measure_start = false;
                                            }
                                            num_spaces = 0;
                                            reading_start = true;
                                        }
                                    }
                                }
                            }
                            ALIGN_LINES();
                            /* copy to header */
                            if (least_num_spaces == 0) { /* we don't need to trim indentation */
                                fwrite(ma_buf, sizeof(char), c, dest);
                            }
                            else { /* trim indentation */
                                size_t indent_off, idx_off, line_start;
                                for (idx = 0; idx < c;) {
                                    idx_off = 0;
                                    indent_off = 0;
                                    line_start = idx;
                                    /* parse through line, counting tabs and spaces */
                                    while (ma_buf[idx] != '\n' && ma_buf[idx] != '\0') {
                                        if (indent_off < least_num_spaces) {
                                            switch (ma_buf[idx]) {
                                            case ' ':
                                                ++indent_off;
                                                ++idx_off;
                                                break;
                                            case '\t':
                                                indent_off += 4;
                                                ++idx_off;
                                                break;
                                            }
                                        }
                                        ++idx;
                                    }
                                    /* end of line */
                                    if (idx != 0) {
                                        /* write section of the recorded line, trimming indentation */
                                        size_t trim_start = line_start + idx_off;
                                        fwrite(&ma_buf[trim_start], sizeof(char), idx - trim_start, dest);
                                        fputc('\n', dest);
                                    }
                                    /* increment to character after newline */
                                    ++idx;
                                }
                            }
                            fputc('\n', dest);
                            /* end of parsing for this token */
                            parse_mode = false;
                        }
                        /* decrease indent level */
                        else {
                            --b;
                            goto cpy_char;
                        }
                        break;
                    case ' ':
                    case '\t':
                    case '\n':
                        /* ignore spacing that immediately follows the first '{' */
                        if (b_a) {
                            goto cpy_char;
                        }
                        /* if there's a newline, stop ignoring spacing for the following lines */
                        if (buf[t] == '\n') {
                            if (strip) goto cpy_char; /* edge case: copy all newlines for stripping */
                            l = line;  /* record next line */
                            b_a = true;
                        }
                        break;
                    cpy_char:
                    default:
                        /* check for overflow */
                        if (c == ma_size) {
                            ma_size *= 2;
                            ma_buf = realloc(ma_buf, ma_size);
                        }
                        /* copy character */
                        ma_buf[c] = buf[t];
                        ++c;
                        /* start copying whitespace */
                        if (!b_a) {
                            l = line;  /* record line */
                            b_a = true;
                        }
                    }
                    break;
                case PARSE_MEMBER: /* parsing a declaration or definition */
                    {
                        /* inline function, GCC extension */
                        void emit_attrs(void) {
                            if (using_attrs) {
                                bool will_return = false;
                                char* ac, * attr_start = ma_buf;
                                for (ac = ma_buf; ac < ma_buf + ma_size; ++ac) {
                                    switch (*ac) {
                                    case '\0':
                                        will_return = true;
                                    case '\1':
                                        fprintf(dest, " __attribute__((__%.*s__))",
                                                (int) (ac - attr_start), attr_start);
                                        if (will_return) return;
                                        attr_start = ac + 1;
                                    }
                                }
                            }
                        }
                        
                        switch (buf[t]) {
                        case ';':
                            /* write everything up to this point */

                            ALIGN_LINES();
                            /* write header prefix */
                            if (prefix != NULL && *prefix != '\0') {
                                fputs(prefix, dest);
                                fputc(' ', dest);
                            }
                            fwrite(m_buf, sizeof(char), b, dest);

                            emit_attrs();
                        
                            fputs(";\n", dest);
                            parse_mode = false;
                            PARSE_INFO("end of member");
                            break;
                        case '{':
                        case '=':
                            {
                                
                                /* trim spacing before '{' or '=' */
                                size_t offset = 0;
                                int idx;
                                for (idx = b - 1; idx >= 0; idx--) {
                                    char at = m_buf[idx];
                                    if (at == ' ' || at == '\t' || at == '\n') {
                                        ++offset;
                                    }
                                    else break;
                                }

                                ALIGN_LINES();
                                /* write header prefix */
                                if (prefix != NULL && *prefix != '\0') {
                                    fputs(prefix, dest);
                                    fputc(' ', dest);
                                }
                                /* write declaration to header */
                                fwrite(m_buf, sizeof(char), b - offset, dest);
                                
                                emit_attrs();
                                
                                fputs(";\n", dest);
                                parse_mode = false;
                                PARSE_INFO("end of member");
                                break;
                            }
                        default:
                            /* detect overflow */
                            if (b == 512) {
                                PARSE_ERR("member declaration too large [max: 512 characters]");
                                return false;
                            }
                            m_buf[b] = buf[t];
                            ++b;
                        }
                        break;
                    }
                }
                
                /* cleanup after exiting parse mode for a token */
                if (!parse_mode) {
                    prefix = set_prefix_buf;
                    prefix_set = false;
                    copying = true;
                }
            }
            
            /* if the character is a newline, mark the next read index as the first in a new line */
            line_start = buf[t] == '\n';
            
            /* if stripping, copy characters over */
            if (copying && strip && !skip_char) {
                fputc(buf[t], dest);
            }
            skip_char = false;
        }
    }
    if (ma_buf != NULL) {
        free(ma_buf);
    }
    return true;
}

/* pop warning ignore */
#if GCC_VERSION_COMPARE(4, 6, 4)
#pragma GCC diagnostic pop
#endif

/* local to process and strip functions */
#undef PARSE_ERR
#undef PARSE_INFO

#undef ALIGN_LINES

#define FOPEN_CHECK(V) ERRNO_CHECK("error when attempting to open file", V)

/* call process with the respective file descriptors after error checking */
static bool handle_open(char* source, char* dest) {
    if (verbose_mode) {
        printf("generating '%s', directory mode\n", dest);
    }
    FILE* fsource = fopen(source, "r");
    FOPEN_CHECK(source);
    FILE* fdest = fopen(dest, "w");
    FOPEN_CHECK(dest);
    if (gaurd_mode && !strip_mode) {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        fprintf(fdest, "\n#ifndef gen_%d_%ld\n#define gen_%d_%ld\n",
                (int) spec.tv_sec, spec.tv_nsec, (int) spec.tv_sec, spec.tv_nsec);
    }
    bool ret = parse(fsource, fdest, strip_mode);
    if (gaurd_mode && !strip_mode) fputs("\n#endif\n", fdest);
    fclose(fsource);
    fclose(fdest);
    return ret;
}

/* create non-existent parent directories for a file, path should not end with '/'. */
static void create_parents(char* path) {
    size_t len = strlen(path), n = 0;
    int t;
    for (t = len - 1; t >= 0 && t != SIZE_MAX; t--) {
        if (path[t] != '/') {
            ++n;
        }
        else break;
    }
    for (t = 1; t < len - n; t++) {
        if (path[t] == '/') {
            char buf[t + 1];
            memcpy(buf, path, t);
            buf[t] = '\0';
            mkdir(buf, S_IRWXU);
            if (errno != EEXIST) {
                ERRNO_CHECK("error when creating parent directory", buf);
            }
            else if (errno != 0) {
                errno = 0;
                /* it's possible for the file to exist and actually be a directory */
                struct stat st;
                stat(buf, &st);
                ERRNO_CHECK("failed to obtain st_mode", buf);
                if (!S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "error when creating parent directories: "
                            "expected '%s' to be a directory\n", buf);
                    exit(EXIT_FAILURE);
                }
            }
            else if (verbose_mode) {
                printf("creating directory: '%s'\n", buf);
            }
        }
    }
}

/* call handle_open, with the destination path as .h, and create parent directories. */
static bool handle_extension(char* source, char* dest) {
    if (!strip_mode) {
        size_t len = strlen(dest), n = 0;
        int t;
        for (t = len - 1; t >= 0 && t != SIZE_MAX; t--) {
            if (dest[t] == '.') {
                ++n;
                break;
            }
            /* if there's no extension on the source file for some reason */
            else if (dest[t] == '/') {
                n = 0;
                break;
            }
            else ++n;
        }
        size_t newlen = (len + 2) - n;
        char buf[newlen + 1];
        buf[newlen - 2] = '.';
        buf[newlen - 1] = 'h';
        buf[newlen] = '\0';
        memcpy(buf, dest, len - n);
        create_parents(buf);
        return handle_open(source, buf);
    }
    else {
        create_parents(dest);
        return handle_open(source, dest);
    }
}

#define REALPATH_CHECK(V) ERRNO_CHECK("error when resolving path", V)

static bool handle_target_set(char** set, size_t nset) {

    FILE* target = NULL;
    bool close_after = true;

    if (pipe_mode) {
        target = stdout;
        close_after = false;
    }
    else {
        target = fopen(single_target, "r");
        FOPEN_CHECK(single_target);
    }
    
    if (gaurd_mode && !strip_mode) {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        fprintf(target, "\n#ifndef gen_%d_%ld\n#define gen_%d_%ld\n",
                (int) spec.tv_sec, spec.tv_nsec, (int) spec.tv_sec, spec.tv_nsec);
    }
    
    size_t t;
    for (t = 0; t < nset; ++t) {

        if (verbose_mode) {
            printf("handling target from set: %s, idx: %d\n", set[t], (int) t);
        }
        
        FILE* fsource = fopen(set[t], "r");
        FOPEN_CHECK(set[t]);

        bool ret = parse(fsource, target, strip_mode);
        fputc('\n', target);
        
        fclose(fsource);
        if (!ret) break;
    }
    if (gaurd_mode && !strip_mode) {
        fputs("\n#endif\n", target);
    }
    if (close_after) {
        fclose(target);
    }
    return true;
}

static bool handle_target(char* buf) {
    /* mimic the source folder structure in the header directory with the generated header */
    if (header_dir && root_dir) {
        char real_path[PATH_MAX];
        realpath(buf, real_path);
        REALPATH_CHECK(buf);
        char real_root_dir[PATH_MAX];
        realpath(root_dir, real_root_dir);
        REALPATH_CHECK(root_dir);
        char real_header_dir[PATH_MAX];
        realpath(header_dir, real_header_dir);
        REALPATH_CHECK(header_dir);
        size_t root_len = strlen(real_root_dir);
        size_t path_len = strlen(real_path);
        if (strncmp(real_path, real_root_dir, root_len) == 0) {
            size_t header_len = strlen(real_header_dir);
            size_t blen = (path_len - root_len) + header_len;
            char target_path[blen + 1];
            target_path[blen] = '\0';
            memcpy(target_path, real_header_dir, header_len);
            memcpy(&target_path[header_len], &real_path[root_len], path_len - root_len);
            if (verbose_mode) {
                printf("building header directories for file: '%s'\n", target_path);
            }
            return handle_extension(real_path, target_path);
        }
        else {
            printf("target '%s' is not a member of the root directory '%s'", real_path, real_root_dir);
            return false;
        }
    }
    /* just plop the generated header into the header directory, no folders */
    else if (header_dir) {
        char real_path[PATH_MAX];
        realpath(buf, real_path);
        REALPATH_CHECK(buf);
        char target_path[PATH_MAX];
        realpath(header_dir, target_path);
        REALPATH_CHECK(header_dir);
        
        size_t n = 0, len = strlen(real_path);
        int t;
        for (t = len - 1; t >= 0; t--) {
            if (real_path[t] == '/') {
                n = t + 1;
            }
            else break;
        }
        size_t first_size = strlen(target_path);
        memcpy(&target_path[first_size], &real_path[n], len - n);
        target_path[first_size + (len - n)] = '\0';
        return handle_extension(real_path, target_path);
    }
    /* pipe the resulting header to stdout */
    else if (pipe_mode) {
        FILE* fsource = fopen(buf, "r");
        FOPEN_CHECK(buf);
        bool ret = parse(fsource, stdout, strip_mode);
        fclose(fsource);
        return ret;
    }
    /* create or overwrite a header file in the same location as the source file */
    else {
        char real_path[PATH_MAX];
        realpath(buf, real_path);
        REALPATH_CHECK(buf);
        return handle_extension(real_path, real_path);
    }
    return false;
}

static size_t indent_opts_labelsize(void) {
    // first pass, we determine the maximum label size
    size_t max_size = 0, current_size = 0, t;
    // 'false' for reading label, 'true' for desc
    bool mode = false;
    for (t = 0; help_opts[t] != '\0'; t++) {
        switch (help_opts[t]) {
        case '\1':
            if (current_size > max_size) {
                max_size = current_size;
            }
            mode = true;
            current_size = 0;
            break;
        case '\n':
            mode = false;
            break;
        default:
            if (!mode) {
                ++current_size;
            }
        }
    }
    return max_size;
}
static size_t indent_opts_bufsize(size_t max_size) {
    size_t current_size = 0, total_size = 0, t;
    bool mode = false;
    for (t = 0; help_opts[t] != '\0'; t++) {
        switch (help_opts[t]) {
        case '\1':
            total_size += (max_size - current_size) + HELP_OPT_TAB; // tabulation
            current_size = 0;
            mode = true;
            break;
        case '\2':
            /* next-line tab, one more for newline */
            total_size += max_size + HELP_OPT_TAB + HELP_OPT_PARAGRAPH_INDENT + 1;
            break;
        case '\n':
            mode = false;
            ++total_size;
            break;
        default:
            if (!mode) {
                ++current_size;
            }
            ++total_size;
        }
    }
    return total_size;
}

static char* indent_opts(size_t total_size, size_t max_size, char* buf) {
    buf[total_size] = '\0';
    size_t current_size = 0, idx = 0, gap, t;
    bool mode = false;
    for (t = 0; help_opts[t] != '\0'; t++) {
        switch (help_opts[t]) {
        case '\1':
            gap = (max_size - current_size) + HELP_OPT_TAB;
            assert(gap < max_size + HELP_OPT_TAB);
            memset(&buf[idx], ' ', gap);
            current_size = 0;
            idx += gap;
            mode = true;
            break;
        case '\2':
            gap = max_size + HELP_OPT_TAB + HELP_OPT_PARAGRAPH_INDENT;
            buf[idx] = '\n';
            ++idx;
            memset(&buf[idx], ' ', gap);
            idx += gap;
            break;
        case '\n':
            mode = false;
            buf[idx] = '\n';
            idx++;
            break;
        default:
            if (!mode) {
                ++current_size;
            }
            buf[idx] = help_opts[t];
            idx++;
        }
    }
    return buf;
}
