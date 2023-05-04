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

#ifndef __PDDL_FILE_H__
#define __PDDL_FILE_H__

#include <boruvka/err.h>
#include <pddl/config.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_FILE_MAX_PATH_LEN 512

struct pddl_files {
    char domain_pddl[PDDL_FILE_MAX_PATH_LEN];
    char problem_pddl[PDDL_FILE_MAX_PATH_LEN];
};
typedef struct pddl_files pddl_files_t;

int pddlFiles1(pddl_files_t *files, const char *s, bor_err_t *err);
int pddlFiles(pddl_files_t *files, const char *s1, const char *s2,
              bor_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FILE_H__ */
