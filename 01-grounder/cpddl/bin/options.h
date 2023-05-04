/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include <pddl/pddl.h>

#define OUTPUT_FD 0
#define OUTPUT_STRIPS 1
#define OUTPUT_PY 2

struct _options_t {
    int help;
    int quiet;
    char *domain_pddl;
    char *problem_pddl;
    char *output;
    int output_type;
    char *output_pddl_domain;
    char *output_pddl_problem;
    char *output_strips_pddl_domain;
    char *output_strips_pddl_problem;

    pddl_config_t cfg;
};
typedef struct _options_t options_t;

options_t *options(int argc, char *argv[]);
void optionsFree(void);

#endif /* OPTIONS_H */
