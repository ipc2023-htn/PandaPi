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

#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Alloc - Memory Allocation
 * ==========================
 *
 * Functions and macros for memory allocation.
 */

/* Memory allocation: - internal macro */
#define _ALLOC_MEMORY(type, ptr_old, size) \
    (type *)realloc((void *)ptr_old, (size))

/**
 * Allocate memory for one element of given type.
 */
#define ALLOC(type) \
    _ALLOC_MEMORY(type, NULL, sizeof(type))

/**
 * Allocate array of elements of given type.
 */
#define ALLOC_ARR(type, num_elements) \
    _ALLOC_MEMORY(type, NULL, sizeof(type) * (num_elements))

/**
 * Reallocates array.
 */
#define REALLOC_ARR(ptr, type, num_elements) \
    _ALLOC_MEMORY(type, ptr, sizeof(type) * (num_elements))

#define FREE(ptr) free(ptr) /*!< Deallocates memory */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __ALLOC_H__ */
