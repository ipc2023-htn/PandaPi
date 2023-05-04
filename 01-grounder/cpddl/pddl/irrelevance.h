/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_IRRELEVANCE_H__
#define __PDDL_IRRELEVANCE_H__

#include <pddl/strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Performs irrelevance analysis and finds irrelevant fact, irrelevant
 * operators and also finds static facts (of course, static implies
 * irrelevant).
 * Facts in irrelevant_facts, and operators in irrelenvat_ops, are ignored
 * during the analysis.
 */
int pddlIrrelevanceAnalysis(const pddl_strips_t *strips,
                            bor_iset_t *irrelevant_facts,
                            bor_iset_t *irrelevant_ops,
                            bor_iset_t *static_facts,
                            bor_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_IRRELEVANCE_H__ */
