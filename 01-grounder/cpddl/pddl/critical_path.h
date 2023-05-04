/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_CRITICAL_PATH_H__
#define __PDDL_CRITICAL_PATH_H__

#include <pddl/common.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_strips;

/**
 * TODO
 */
int pddlH2(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *m,
           bor_iset_t *unreachable_facts,
           bor_iset_t *unreachable_ops,
           bor_err_t *err);

/**
 * TODO
 */
int pddlH2FwBw(const pddl_strips_t *strips,
               const pddl_mgroups_t *mgroup,
               pddl_mutex_pairs_t *m,
               bor_iset_t *unreachable_facts,
               bor_iset_t *unreachable_ops,
               bor_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_CRITICAL_PATH_H__ */
