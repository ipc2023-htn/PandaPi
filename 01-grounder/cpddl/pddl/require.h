/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_REQUIRE_H__
#define __PDDL_REQUIRE_H__

#include <boruvka/err.h>
#include <pddl/common.h>
#include <pddl/lisp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Requirements
 */
#define PDDL_REQUIRE_STRIPS                0x000001u
#define PDDL_REQUIRE_TYPING                0x000002u
#define PDDL_REQUIRE_NEGATIVE_PRE          0x000004u
#define PDDL_REQUIRE_DISJUNCTIVE_PRE       0x000008u
#define PDDL_REQUIRE_EQUALITY              0x000010u
#define PDDL_REQUIRE_EXISTENTIAL_PRE       0x000020u
#define PDDL_REQUIRE_UNIVERSAL_PRE         0x000040u
#define PDDL_REQUIRE_CONDITIONAL_EFF       0x000080u
#define PDDL_REQUIRE_NUMERIC_FLUENT        0x000100u
#define PDDL_REQUIRE_OBJECT_FLUENT         0x000200u
#define PDDL_REQUIRE_DURATIVE_ACTION       0x000400u
#define PDDL_REQUIRE_DURATION_INEQUALITY   0x000800u
#define PDDL_REQUIRE_CONTINUOUS_EFF        0x001000u
#define PDDL_REQUIRE_DERIVED_PRED          0x002000u
#define PDDL_REQUIRE_TIMED_INITIAL_LITERAL 0x004000u
#define PDDL_REQUIRE_PREFERENCE            0x008000u
#define PDDL_REQUIRE_CONSTRAINT            0x010000u
#define PDDL_REQUIRE_ACTION_COST           0x020000u
#define PDDL_REQUIRE_MULTI_AGENT           0x040000u
#define PDDL_REQUIRE_UNFACTORED_PRIVACY    0x080000u
#define PDDL_REQUIRE_FACTORED_PRIVACY      0x100000u

/**
 * Parses :requirements from domain pddl.
 */
int pddlRequireParse(pddl_t *pddl, bor_err_t *err);

/**
 * Print requirements in PDDL format.
 */
void pddlRequirePrintPDDL(unsigned require, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_REQUIRE_H__ */
