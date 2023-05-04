/***
 * opts
 * -----
 * Copyright (c)2011-2012 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of opts.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include <string.h>
#include "opts.h"
#include "alloc.h"

#include "parse.c"

struct _opt_t {
    const char *long_name; /*!< Long name of option. NULL means no long
                                name. */
    char short_name;       /*!< Short, one letter long, name of option. 0x0
                                means no short name */
    uint32_t type;         /*!< Type of the option's value. See {Types} section. */
    void *set;             /*!< If set non-NULL, the value of the option
                                will be assigned to it. The type of pointer
                                must correspond to the {.type} */
    void (*callback)(void);/*!< Callback called (if non-NULL) when option
                                detected. The type of the callback depends
                                on the {.type} */
    char *desc;            /*!< Description of the option */
};
typedef struct _opt_t opt_t;

static opt_t **opts_arr = NULL;
static size_t opts_len = 0;


static opt_t *findOpt(char *arg);
static opt_t *findOptLong(char *arg);
static opt_t *findOptShort(char arg);

static void optNoArg(opt_t *opt);
static int optArg(opt_t *opt, const char *arg);
static void invalidOptErr(const opt_t *opt);

static const char *strelend(const char *str);

int optsAdd(const char *long_name, char short_name,
            uint32_t type, void *set, void (*callback)())
{
    size_t i;

    if (!long_name && !short_name)
        return -1;

    opts_len += 1;
    opts_arr = REALLOC_ARR(opts_arr, opt_t *, opts_len);
    if (!opts_arr)
        return -1;

    i = opts_len - 1;
    opts_arr[i] = ALLOC(opt_t);
    if (!opts_arr[i])
        return -1;

    opts_arr[i]->long_name  = long_name;
    opts_arr[i]->short_name = short_name;
    opts_arr[i]->type       = type;
    opts_arr[i]->set        = set;
    opts_arr[i]->callback   = callback;
    opts_arr[i]->desc       = NULL;

    if (opts_arr[i]->type == OPTS_NONE && opts_arr[i]->set)
        *(int *)opts_arr[i]->set = 0;

    return i;
}

int optsAddDesc(const char *long_name, char short_name,
                uint32_t type, void *set, void (*callback)(),
                const char *desc)
{
    int id, desclen;
    opt_t *opt;
   
    id = optsAdd(long_name, short_name, type, set, callback);
    if (id < 0)
        return -1;

    opt = opts_arr[id];
    if (desc){
        desclen = strlen(desc);
        opt->desc = ALLOC_ARR(char, desclen + 1);
        if (!opt->desc)
            return -1;

        strcpy(opt->desc, desc);
    }

    return id;
}

void optsClear(void)
{
    size_t i;

    for (i = 0; i < opts_len; i++){
        if (opts_arr[i]->desc)
            FREE(opts_arr[i]->desc);
        FREE(opts_arr[i]);
    }
    FREE(opts_arr);
    opts_arr = NULL;
    opts_len = 0;
}

int opts(int *argc, char **argv)
{
    opt_t *opt;
    int args_remaining;
    int i, ok = 0;
   
    if (*argc <= 1)
        return 0;

    args_remaining = 1;

    for (i = 1; i < *argc; i++){
        opt = findOpt(argv[i]);

        if (opt){
            // found corresponding option
            if (opt->type == OPTS_NONE){
                // option has no argument
                optNoArg(opt);
            }else{
                // option has an argument
                if (i + 1 < *argc){
                    ++i;
                    ok = optArg(opt, argv[i]);
                }else{
                    invalidOptErr(opt);
                    ok = -1;
                }
            }
        }else{
            // no corresponding option
            argv[args_remaining] = argv[i];
            args_remaining += 1;
        }
    }

    *argc = args_remaining;

    return ok;
}

static opt_t *findOpt(char *_arg)
{
    char *arg = _arg;
    opt_t *opt;

    if (arg[0] == '-'){
        if (arg[1] == '-'){
            return findOptLong(arg + 2);
        }else{
            if (arg[1] == 0x0)
                return NULL;

            for (++arg; *arg != 0x0; ++arg){
                opt = findOptShort(*arg);
                if (arg[1] == 0x0){
                    return opt;
                }else if (opt->type == OPTS_NONE){
                    optNoArg(opt);
                }else{
                    fprintf(stderr, "Invalid option %s.\n", _arg);
                    return NULL;
                }
            }
            
        }
    }

    return NULL;
}

static opt_t *findOptLong(char *arg)
{
    size_t i;

    for (i = 0; i < opts_len; i++){
        if (opts_arr[i]->long_name && strcmp(opts_arr[i]->long_name, arg) == 0){
            return opts_arr[i];
        }
    }

    return NULL;
}

static opt_t *findOptShort(char arg)
{
    size_t i;

    for (i = 0; i < opts_len; i++){
        if (arg == opts_arr[i]->short_name){
            return opts_arr[i];
        }
    }

    return NULL;
}

static void optNoArg(opt_t *opt)
{
    void (*cb)(const char *, char);

    if (opt->set){
        *(int *)opt->set = 1;
    }

    if (opt->callback){
        cb = (void (*)(const char *, char))opt->callback;
        cb(opt->long_name, opt->short_name);
    }
}

static void invalidOptErr(const opt_t *opt)
{
    if (opt->long_name && opt->short_name){
        fprintf(stderr, "Invalid argument of -%c/--%s option.\n",
                (opt)->short_name, (opt)->long_name);
    }else if (opt->long_name){
        fprintf(stderr, "Invalid argument of --%s option.\n", (opt)->long_name);
    }else{
        fprintf(stderr, "Invalid argument of -%c option.\n", (opt)->short_name);
    }
}

#define OPTARG_NAME      optArgLong
#define OPTARG_TYPE      long
#define OPTARG_BASETYPE  long
#define OPTARG_PARSEFUNC parseLong
#include "optarg.c"

#define OPTARG_NAME      optArgInt
#define OPTARG_TYPE      int
#define OPTARG_BASETYPE  long
#define OPTARG_PARSEFUNC parseLong
#include "optarg.c"

#define OPTARG_NAME      optArgSizeT
#define OPTARG_TYPE      size_t
#define OPTARG_BASETYPE  long
#define OPTARG_PARSEFUNC parseLong
#include "optarg.c"


#define OPTARG_NAME      optArgFloat
#define OPTARG_TYPE      float
#define OPTARG_BASETYPE  double
#define OPTARG_PARSEFUNC parseDouble
#include "optarg.c"

#define OPTARG_NAME      optArgDouble
#define OPTARG_TYPE      double
#define OPTARG_BASETYPE  double
#define OPTARG_PARSEFUNC parseDouble
#include "optarg.c"


static int optArgStr(opt_t *opt, const char *arg, int len)
{
    void (*cb)(const char *, char, const char *);

    if (opt->set){
        *(const char **)opt->set = arg;
    }

    if (opt->callback){
        cb = (void (*)(const char *, char, const char *))opt->callback;
        cb(opt->long_name, opt->short_name, arg);
    }

    return 0;
}

static int optArg(opt_t *opt, const char *arg)
{
    int type;
    int len = 0;

    type = opt->type & 0xff;
    len  = opt->type >> 8;
    switch(type){
        case OPTS_LONG:
            return optArgLong(opt, arg, len);
        case OPTS_INT:
            return optArgInt(opt, arg, len);
        case OPTS_FLOAT:
            return optArgFloat(opt, arg, len);
        case OPTS_DOUBLE:
            return optArgDouble(opt, arg, len);
        case OPTS_STR:
            return optArgStr(opt, arg, len);
        case OPTS_SIZE_T:
            return optArgSizeT(opt, arg, len);
        default:
            return -1;
    }

    return 0;
}

static const char *strelend(const char *str)
{
    const char *s = str;
    while (*s != 0x0 && *s != ',' && *s != ';')
        ++s;
    return s;
}


/** Returns maximal length of name parts and 0/1 via {has_short} whether
 *  any short option is specified. */
static size_t optsNameLen(int *has_short)
{
    size_t i, len, name_len;

    *has_short = 0;
    for (i = 0; i < opts_len; i++){
        if (opts_arr[i]->short_name){
            *has_short = 1;
            break;
        }
    }

    name_len = 0;
    for (i = 0; i < opts_len; i++){
        len = 0;

        if (opts_arr[i]->long_name){
            len = (*has_short ? 7 : 2);
            len += strlen(opts_arr[i]->long_name);
        }else{
            len = 2;
        }

        if (len > name_len)
            name_len = len;
    }

    return name_len;
}

static void printName(opt_t *opt, size_t len, int has_short, FILE *out)
{
    size_t l = 0;

    if (opt->short_name){
        fprintf(out, "-%c / ", opt->short_name);
        l = 5;
    }else if (has_short){
        fprintf(out, "     ");
        l = 5;
    }

    if (opt->long_name){
        fprintf(out, "--%s", opt->long_name);
        l += 2 + strlen(opt->long_name);
    }

    for (; l < len; l++){
        fprintf(out, " ");
    }
}

static void printType(opt_t *opt, FILE *out)
{
    int type;
    int len = 0;

    type = opt->type & 0xff;
    len  = opt->type >> 8;
    switch(type){
        case OPTS_NONE:
            fprintf(out, "       ");
            break;
        case OPTS_LONG:
        case OPTS_INT:
            if (len > 0){
                fprintf(out, "int[]  ");
            }else{
                fprintf(out, "int    ");
            }
            break;
        case OPTS_FLOAT:
        case OPTS_DOUBLE:
            if (len > 0){
                fprintf(out, "float[]");
            }else{
                fprintf(out, "float  ");
            }
            break;
        case OPTS_STR:
            if (len > 0){
                fprintf(out, "str[]  ");
            }else{
                fprintf(out, "str    ");
            }
            break;
        case OPTS_SIZE_T:
            if (len > 0){
                fprintf(out, "uint[] ");
            }else{
                fprintf(out, "uint   ");
            }
            break;
    }
}

static void printDesc(opt_t *opt, FILE *out)
{
    if (!opt->desc)
        return;

    fprintf(out, "%s", opt->desc);
}

void optsPrint(FILE *out, const char *lineprefix)
{
    size_t i;
    size_t name_len;
    int has_short;

    name_len = optsNameLen(&has_short);

    // print option descriptions
    for (i = 0; i < opts_len; i++){
        // print line prefix first
        fprintf(out, "%s", lineprefix);

        // then print name
        printName(opts_arr[i], name_len, has_short, out);
        fprintf(out, "  ");

        // print type
        printType(opts_arr[i], out);
        fprintf(out, "  ");

        // print description
        printDesc(opts_arr[i], out);

        fprintf(out, "\n");
    }
}
