/***
 * opts
 * -----
 * Copyright (c)2012 Daniel Fiser <danfis@danfis.cz>
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

#include <stdio.h>
#include "opts.h"


static void nonecb(const char *longname, char shortname)
{
    printf("Callback [none]: %s %d\n", longname, (int)shortname);
}

static void intcb(const char *l, char s, int v)
{
    printf("Callback [int]: %s %d %d\n", l, (int)s, v);
}

static void fltcb(const char *l, char s, float v)
{
    printf("Callback [float]: %s %d %f\n", l, (int)s, v);
}

static void intarrcb(const char *l, char s, int *v)
{
    printf("Callback [int[]]: %s %d [0: %d, 1: %d]\n",
           l, (int)s, v[0], v[1]);
}

int main(int argc, char *argv[])
{
    int n1 = -1, n2 = -1;
    int i1 = -1, i2 = -1;
    long l1 = -2, l2 = -2;
    float f1 = 99.f, f2 = 99.f;
    double d1 = 999., d2 = 999.;
    const char *s1 = NULL;
    const char *s2 = NULL;
    size_t sz = 0;
    int iarr[2] = {0, 0};
    long larr[2] = {0, 0};
    float farr[3] = {0., 0., 0.};
    double darr[4] = {0., 0., 0., 0.};
    size_t sarr[1] = {0};

    optsAdd("none", 'n', OPTS_NONE, (void *)&n1, OPTS_CB(nonecb));
    optsAdd(NULL, 'm', OPTS_NONE, (void *)&n2, NULL);
    optsAdd("int", 'i', OPTS_INT, (void *)&i1, NULL);
    optsAdd("int2", 0x0, OPTS_INT, (void *)&i2, OPTS_CB(intcb));
    optsAdd("long", 'l', OPTS_LONG, (void *)&l1, NULL);
    optsAdd("long2", 0x0, OPTS_LONG, (void *)&l2, NULL);
    optsAdd("flt", 'f', OPTS_FLOAT, (void *)&f1, NULL);
    optsAdd(NULL, 'g', OPTS_FLOAT, (void *)&f2, OPTS_CB(fltcb));
    optsAdd("dbl", 0x0, OPTS_DOUBLE, (void *)&d1, NULL);
    optsAdd("dbl2", 0x0, OPTS_DOUBLE, (void *)&d2, NULL);
    optsAdd("str", 0x0, OPTS_STR, (void *)&s1, NULL);
    optsAdd("str2", 0x0, OPTS_STR, (void *)&s2, NULL);
    optsAdd("size_t", 0x0, OPTS_SIZE_T, (void *)&sz, NULL);
    optsAdd("iarr", 0x0, OPTS_INT_ARR(2), (void *)iarr, OPTS_CB(intarrcb));
    optsAdd("larr", 0x0, OPTS_LONG_ARR(2), (void *)larr, NULL);
    optsAdd("farr", 'a', OPTS_FLOAT_ARR(3), (void *)farr, NULL);
    optsAdd("darr", 0x0, OPTS_DOUBLE_ARR(4), (void *)darr, NULL);
    optsAdd("sarr", 0x0, OPTS_SIZE_T_ARR(1), (void *)sarr, NULL);

    opts(&argc, argv);

    printf("n1: %d\n", n1);
    printf("n2: %d\n", n2);
    printf("i1: %d\n", i1);
    printf("i2: %d\n", i2);
    printf("l1: %ld\n", l1);
    printf("l2: %ld\n", l2);
    printf("f1: %f\n", f1);
    printf("f2: %f\n", f2);
    printf("d1: %lf\n", d1);
    printf("d2: %lf\n", d2);
    printf("s1: `%s'\n", s1);
    printf("s2: `%s'\n", s2);
    printf("sz: %lu\n", (unsigned long)sz);
    printf("iarr[0]: %d\n", iarr[0]);
    printf("iarr[1]: %d\n", iarr[1]);
    printf("larr[0]: %ld\n", larr[0]);
    printf("larr[1]: %ld\n", larr[1]);
    printf("farr[0]: %f\n", farr[0]);
    printf("farr[1]: %f\n", farr[1]);
    printf("farr[2]: %f\n", farr[2]);
    printf("darr[0]: %lf\n", darr[0]);
    printf("darr[1]: %lf\n", darr[1]);
    printf("darr[2]: %lf\n", darr[2]);
    printf("darr[3]: %lf\n", darr[3]);
    printf("sarr[3]: %lu\n", (unsigned long)sarr[0]);

    optsClear();
    return 0;
}
