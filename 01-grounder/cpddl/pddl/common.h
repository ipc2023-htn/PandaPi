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

#ifndef __PDDL_COMMON_H__
#define __PDDL_COMMON_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl pddl_t;
typedef struct pddl_strips pddl_strips_t;

/** Type for holding number of objects */
typedef uint16_t pddl_obj_size_t;
/** Type for holding number of action parameters */
typedef uint16_t pddl_action_param_size_t;

typedef int pddl_obj_id_t;

/** Constant for undefined object ID.
 *  It should be always defined as something negative so we can test object
 *  ID with >= 0 and < 0. */
#define PDDL_OBJ_ID_UNDEF ((pddl_obj_id_t)-1)

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_COMMON_H__ */
