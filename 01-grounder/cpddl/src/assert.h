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

#ifndef __PDDL_ASSERT_H__
#define __PDDL_ASSERT_H__

#include "pddl/config.h"

#ifdef PDDL_DEBUG
#include <assert.h>
# define ASSERT(x) assert(x)
# define ASSERT_RUNTIME(x) assert(x)
#else /* PDDL_DEBUG */
# define NDEBUG
# define ASSERT(x)
# define ASSERT_RUNTIME(x) \
    do { \
    if (!(x)){ \
        fprintf(stderr, "%s:%d Assertion `" #x "' failed!\n", \
                __FILE__, __LINE__); \
        exit(-1); \
    } \
    } while (0)
#endif /* PDDL_DEBUG */

# define ASSERT_RUNTIME_M(X, M) \
    do { \
    if (!(X)){ \
        fprintf(stderr, "%s:%d Assertion `" #X "' failed: %s\n", \
                __FILE__, __LINE__, (M)); \
        exit(-1); \
    } \
    } while (0)


#endif /* __PDDL_ASSERT_H__ */
