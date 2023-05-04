/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_STRUCT_H__
#define __PDDL_STRUCT_H__

#include <pddl/config.h>
#include <pddl/lisp.h>
#include <pddl/require.h>
#include <pddl/type.h>
#include <pddl/obj.h>
#include <pddl/pred.h>
#include <pddl/fact.h>
#include <pddl/action.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_config {
    int force_adl; /*!< Force ADL to requirements */
    // TODO: info output, err output
};
typedef struct pddl_config pddl_config_t;

#define PDDL_CONFIG_INIT_EMPTY { 0 }
#define PDDL_CONFIG_INIT \
    { 0, /* force_adl */ \
    }

struct pddl {
    pddl_config_t cfg;
    pddl_lisp_t *domain_lisp;
    pddl_lisp_t *problem_lisp;
    char *domain_name;
    char *problem_name;
    unsigned require;
    pddl_types_t type;
    pddl_objs_t obj;
    pddl_preds_t pred;
    pddl_preds_t func;
    pddl_cond_part_t *init;
    pddl_cond_t *goal;
    pddl_actions_t action;
    int metric;
    int normalized;
};

/**
 * Initialize pddl structure from the domain/problem PDDL files.
 */
int pddlInit(pddl_t *pddl, const char *domain_fn, const char *problem_fn,
             const pddl_config_t *cfg, bor_err_t *err);

/**
 * Creates a copy of the pddl structure.
 */
void pddlInitCopy(pddl_t *dst, const pddl_t *src);

/**
 * Frees allocated memory.
 */
void pddlFree(pddl_t *pddl);

pddl_t *pddlNew(const char *domain_fn, const char *problem_fn,
                const pddl_config_t *cfg, bor_err_t *err);
void pddlDel(pddl_t *pddl);

/**
 * Normalize pddl, i.e., make preconditions and effects CNF
 */
void pddlNormalize(pddl_t *pddl);

/**
 * Generate pddl without conditional effects.
 */
void pddlCompileAwayCondEff(pddl_t *pddl);

/**
 * Generate pddl without conditional effects unless the conditional effects
 * that have only static predicates in its preconditions.
 * This is enough for grounding.
 */
void pddlCompileAwayNonStaticCondEff(pddl_t *pddl);

/**
 * Returns maximal number of parameters of all predicates and functions.
 */
int pddlPredFuncMaxParamSize(const pddl_t *pddl);

/**
 * Checks pddl_*_size_t types agains the parsed pddl.
 * If any of these types is too small the program exists with error
 * message.
 */
void pddlCheckSizeTypes(const pddl_t *pddl);

/**
 * Adds one new type per object if necessary.
 */
void pddlAddObjectTypes(pddl_t *pddl);

/**
 * Prints PDDL domain file.
 */
void pddlPrintPDDLDomain(const pddl_t *pddl, FILE *fout);

/**
 * Prints PDDL problem file.
 */
void pddlPrintPDDLProblem(const pddl_t *pddl, FILE *fout);

void pddlPrintDebug(const pddl_t *pddl, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRUCT_H__ */
