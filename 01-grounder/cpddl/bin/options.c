/***
 * maplan
 * -------
 * Copyright (c)2015 Daniel Fiser <danfis@danfis.cz>,
 * Agent Technology Center, Department of Computer Science,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of maplan.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#include <strings.h>
#include <string.h>
#include <boruvka/alloc.h>
#include <opts.h>

#include "options.h"

static options_t _opts = {
    0,
    0,
    NULL,
    NULL,
    NULL,
    OUTPUT_FD,
    NULL,
    NULL,
    NULL,
    NULL,
    PDDL_CONFIG_INIT,
};

static void outputFD(const char *l, char s)
{
    _opts.output_type = OUTPUT_FD;
}

static void outputStrips(const char *l, char s)
{
    _opts.output_type = OUTPUT_STRIPS;
}

static void outputPy(const char *l, char s)
{
    _opts.output_type = OUTPUT_PY;
}

static int readOpts(int argc, char *argv[])
{
    options_t *o = &_opts;

    optsAddDesc("help", 'h', OPTS_NONE, &o->help, NULL,
                "Print this help.");
    optsAddDesc("quiet", 'q', OPTS_NONE, &o->quiet, NULL,
                "Disable logging output.");
    optsAddDesc("output", 'o', OPTS_STR, &o->output, NULL,
                "Output file.");
    optsAddDesc("fd", 0x0, OPTS_NONE, NULL, OPTS_CB(outputFD),
                "Output fast-downward format.");
    optsAddDesc("strips", 0x0, OPTS_NONE, NULL, OPTS_CB(outputStrips),
                "Output STRIPS text format.");
    optsAddDesc("py", 0x0, OPTS_NONE, NULL, OPTS_CB(outputPy),
                "Output python format.");
    optsAddDesc("output-pddl-domain", 0x0, OPTS_STR, &o->output_pddl_domain,
                NULL, "Output file for PDDL domain.");
    optsAddDesc("output-pddl-problem", 0x0, OPTS_STR, &o->output_pddl_problem,
                NULL, "Output file for PDDL problem.");
    optsAddDesc("output-strips-pddl-domain", 0x0, OPTS_STR,
                &o->output_strips_pddl_domain, NULL,
                "Output file for PDDL domain.");
    optsAddDesc("output-strips-pddl-problem", 0x0, OPTS_STR,
                &o->output_strips_pddl_problem, NULL,
                "Output file for PDDL problem.");

    if (opts(&argc, argv) != 0){
        return -1;
    }
    if (argc != 3)
        return -1;

    o->domain_pddl = argv[1];
    o->problem_pddl = argv[2];

    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
    fprintf(stderr, "  OPTIONS:\n");
    optsPrint(stderr, "    ");
    fprintf(stderr, "\n");
}

options_t *options(int argc, char *argv[])
{
    options_t *o = &_opts;

    if (readOpts(argc, argv) != 0 || o->help){
        usage(argv[0]);
        return NULL;
    }

    return o;
}

void optionsFree(void)
{
    optsClear();
}
