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

#ifndef __OPTS_H__
#define __OPTS_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Parsing Command Line Options
 * =============================
 */

#define __OPTS_ARR(LEN, TYPE) \
    (((LEN) << 8) | TYPE)

/**
 * Example
 * --------
 * ~~~~~
 * #include <stdio.h>
 * #include "opts.h"
 *
 * static void helpcb(const char *l, char s)
 * {
 *     fprintf(stderr, "HelpCB: %s %c\n", l, s);
 *     fprintf(stderr, "\n");
 * }
 *
 * int main(int argc, char *argv[])
 * {
 *     float opt1;
 *     float optarr[3];
 *     int help;
 *     int i;
 *
 *     optarr[0] = optarr[1] = optarr[2] = 0.;
 *
 *     // define options
 *     optsAdd("opt1", 'o', OPTS_FLOAT, (void *)&opt1, NULL);
 *     optsAdd("help", 'h', OPTS_NONE, (void *)&help, OPTS_CB(helpcb));
 *     optsAdd("optarr", 'a', OPTS_FLOAT_ARR(3), (void *)optarr, NULL);
 *
 *     // parse options
 *     if (opts(&argc, argv) != 0)
 *         return -1;
 *
 *     // print some info
 *     fprintf(stdout, "help: %d\n", help);
 *     fprintf(stdout, "opt1: %f\n", (float)opt1);
 *     for (i = 0; i < 3; i++){
 *         fprintf(stdout, "optarr[%d]: %f\n", i, optarr[i]);
 *     }
 *
 *     // print the rest of options
 *     for (i = 0; i < argc; i++){
 *         fprintf(stdout, "[%02d]: `%s'\n", i, argv[i]);
 *     }
 *
 *     return 0;
 * }
 *
 * // If compiled to program called test:
 * // $ ./test
 * // > help: 0
 * // > opt1: 0.000000
 * // > optarr[0]: 0.000000
 * // > optarr[1]: 0.000000
 * // > optarr[2]: 0.000000
 * // > [00]: `./test'
 * //
 * // $ ./test --opt1 1.1 --opt
 * // > help: 0
 * // > opt1: 1.100000
 * // > optarr[0]: 0.000000
 * // > optarr[1]: 0.000000
 * // > optarr[2]: 0.000000
 * // > [00]: `./test'
 * // > [01]: `--opt'
 * //
 * // $ ./test -o 2.2 -h -a 2,3.1,11
 * // > HelpCB: help h
 * // > 
 * // > help: 1
 * // > opt1: 2.200000
 * // > optarr[0]: 2.000000
 * // > optarr[1]: 3.100000
 * // > optarr[2]: 11.000000
 * // > [00]: `./test'
 * ~~~~~
 *
 *
 * Types
 * ------
 */

/** vvvv */
/**
 * No argument.
 *     1. .set must be [int *] and is set to 1 if option was found and to 0
 *        if it wasn't.
 *     2. .callback must have type void (*)(const char *long_name, char short_name)
 */
#define OPTS_NONE 0x00

/**
 * Long type.
 *     1. .set must have type [long *]
 *     2. .callback must have type void (*)(const char *long_name, char short_name, long val)
 */
#define OPTS_LONG 0x01

/**
 * Int.
 *     1. .set - [int *]
 *     2. .callback - void (*)(const char *long_name, char short_name, int val)
 */
#define OPTS_INT 0x02

/**
 * Float.
 *     1. .set - [float *]
 *     2. .callback - void (*)(const char *long_name, char short_name, float val)
 */
#define OPTS_FLOAT 0x03

/**
 * Double.
 *     1. .set - [double *]
 *     2. .callback - void (*)(const char *long_name, char short_name, double val)
 */
#define OPTS_DOUBLE 0x04

/**
 * String.
 *     1. .set - [const char **]
 *     2. .callback - void (*)(const char *long_name, char short_name, const char *)
 */
#define OPTS_STR 0x05

/**
 * size_t.
 *     1. .set - [size_t *]
 *     2. .callback - void (*)(const char *long_name, char short_name, size_t)
 */
#define OPTS_SIZE_T 0x06

/**
 * Array of longs type.
 *     1. .set must have type [long *] and have at least LEN elements
 *     2. .callback must have type void (*)(const char *long_name, char short_name, long *vals)
 */
#define OPTS_LONG_ARR(LEN) __OPTS_ARR((LEN), OPTS_LONG)

/**
 * Int array.
 *     1. .set - [int *]
 *     2. .callback - void (*)(const char *long_name, char short_name, int *vals)
 */
#define OPTS_INT_ARR(LEN) __OPTS_ARR((LEN), OPTS_INT)

/**
 * Float array.
 *     1. .set - [float *]
 *     2. .callback - void (*)(const char *long_name, char short_name, float *vals)
 */
#define OPTS_FLOAT_ARR(LEN) __OPTS_ARR((LEN), OPTS_FLOAT)

/**
 * Double array.
 *     1. .set - [double *]
 *     2. .callback - void (*)(const char *long_name, char short_name, double *vals)
 */
#define OPTS_DOUBLE_ARR(LEN) __OPTS_ARR((LEN), OPTS_DOUBLE)

/**
 * size_t array.
 *     1. .set - [size_t *]
 *     2. .callback - void (*)(const char *long_name, char short_name, size_t *vals)
 */
#define OPTS_SIZE_T_ARR(LEN) __OPTS_ARR((LEN), OPTS_SIZE_T)

/** ^^^^ */

/**
 * Functions
 * ----------
 */

/**
 * Use this macro for passing callback to optsAdd().
 */
#define OPTS_CB(func) (void (*)(void))(func)
 
/**
 * Adds description of an option:
 *     1. {long_name}: Long name of option. NULL means no long name.
 *     2. {short_name}: Short, one letter long, name of option. 0x0 means no short name.
 *     3. {type}: Type of the option's value. See {Types} section.
 *     4. {set}: If set non-NULL, the value of the option will be assigned
 *        to it. The type of pointer must correspond to the {type}.
 *     5. {callback}: Callback called (if non-NULL) when option detected.
 *        The type of the callback depends on the {type}.
 *
 * Returns ID of the added option.
 */
int optsAdd(const char *long_name, char short_name,
            uint32_t type, void *set, void (*callback)());

/**
 * Same as {optsAdd()} but has additional parameter {desc} where can be
 * passed string description of the option
 */
int optsAddDesc(const char *long_name, char short_name,
                uint32_t type, void *set, void (*callback)(),
                const char *desc);

/**
 * Clears all options previously added
 */
void optsClear(void);

/**
 * Parses command line options.
 * The first item of {argv} array is skipped.
 * The arguments {argc} and {argv} are modified to contain only the rest of
 * the options that weren't parsed.
 * Returns 0 if all options were successfully parsed.
 */
int opts(int *argc, char **argv);


/**
 * Print list of all options
 */
void optsPrint(FILE *out, const char *lineprefix);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OPTS_H__ */
