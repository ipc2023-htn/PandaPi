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

#ifndef __PDDL_COND_H__
#define __PDDL_COND_H__

#include <boruvka/list.h>

#include <pddl/common.h>
#include <pddl/lisp.h>
#include <pddl/require.h>
#include <pddl/type.h>
#include <pddl/param.h>
#include <pddl/obj.h>
#include <pddl/pred.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Types of conditions
 */
#define PDDL_COND_AND    0u /*!< Conjuction */
#define PDDL_COND_OR     1u /*!< Disjunction */
#define PDDL_COND_FORALL 2u /*!< Universal quantifier */
#define PDDL_COND_EXIST  3u /*!< Existential quantifier */
#define PDDL_COND_WHEN   4u /*!< Conditional effect */
#define PDDL_COND_ATOM   5u /*!< Atom in a positive or negative form */
#define PDDL_COND_ASSIGN 6u /*!< Assignement to a function (= ...) */
#define PDDL_COND_INCREASE 7u /*!< (increase ...) */
#define PDDL_COND_BOOL   8u /*!< True/False */
#define PDDL_COND_IMPLY  9u /*!< (imply X Y) */
#define PDDL_COND_NUM_TYPES 10

const char *pddlCondTypeName(int type);

#define PDDL_COND_CAST(C, T) \
    (bor_container_of((C), pddl_cond_##T##_t, cls))

/**
 * General condition
 */
struct pddl_cond {
    unsigned type;   /*!< Type of the condition */
    bor_list_t conn; /*!< Connection to the parent cond */
};
typedef struct pddl_cond pddl_cond_t;

/**
 * Conuction / Disjunction
 */
struct pddl_cond_part {
    pddl_cond_t cls;
    bor_list_t part; /*!< List of parts */
};
typedef struct pddl_cond_part pddl_cond_part_t;

/**
 * Quantifiers
 */
struct pddl_cond_quant {
    pddl_cond_t cls;
    pddl_params_t param; /*!< List of parameters */
    pddl_cond_t *cond;   /*!< Quantified condition */
};
typedef struct pddl_cond_quant pddl_cond_quant_t;

/**
 * Conditional effect
 */
struct pddl_cond_when {
    pddl_cond_t cls;
    pddl_cond_t *pre;
    pddl_cond_t *eff;
};
typedef struct pddl_cond_when pddl_cond_when_t;


/**
 * Argument of an atom
 */
struct pddl_cond_atom_arg {
    int param; /*!< -1 or index of parameter */
    pddl_obj_id_t obj; /*!< object ID (constant) or PDDL_OBJ_ID_UNDEF */
};
typedef struct pddl_cond_atom_arg pddl_cond_atom_arg_t;

/**
 * Atom
 */
struct pddl_cond_atom {
    pddl_cond_t cls;
    int pred;                  /*!< Predicate ID */
    pddl_cond_atom_arg_t *arg; /*!< List of arguments */
    int arg_size;              /*!< Number of arguments */
    int neg;                   /*!< True if negated */
};
typedef struct pddl_cond_atom pddl_cond_atom_t;


/**
 * Assign
 * TODO: For now only (increase (total-cost) (...)) is supported
 */
struct pddl_cond_func_op {
    pddl_cond_t cls;
    pddl_cond_atom_t *lvalue; /*!< lvalue for assignement */
    int value;                /*!< Assigned immediate value */
    pddl_cond_atom_t *fvalue; /*!< Assigned value through function symbol */
};
typedef struct pddl_cond_func_op pddl_cond_func_op_t;

/**
 * Boolean value
 */
struct pddl_cond_bool {
    pddl_cond_t cls;
    int val;
};
typedef struct pddl_cond_bool pddl_cond_bool_t;

/**
 * Imply: (imply (...) (...))
 */
struct pddl_cond_imply {
    pddl_cond_t cls;
    pddl_cond_t *left;
    pddl_cond_t *right;
};
typedef struct pddl_cond_imply pddl_cond_imply_t;


/**
 * Free memory.
 */
void pddlCondDel(pddl_cond_t *cond);

/**
 * Creates an exact copy of the condition.
 */
pddl_cond_t *pddlCondClone(const pddl_cond_t *cond);

/**
 * Returns a negated copy of the condition.
 */
pddl_cond_t *pddlCondNegate(const pddl_cond_t *cond,
                            const pddl_t *pddl);

/**
 * Returns true if the conds match exactly.
 */
int pddlCondEq(const pddl_cond_t *c1, const pddl_cond_t *c2);

/**
 * Traverse all conditionals in a tree and call in pre/post order callbacks
 * if non-NULL.
 * If pre returns -1 the element is skipped (it is not traversed deeper).
 * If pre returns -2 the whole traversing is terminated.
 * If post returns non-zero value the whole traversing is terminated.
 */
void pddlCondTraverse(pddl_cond_t *c,
                     int (*pre)(pddl_cond_t *, void *),
                     int (*post)(pddl_cond_t *, void *),
                     void *u);

/**
 * Same as pddlCondTraverse() but pddl_cond_t structures are passed so that
 * they can be safely changed within callbacks.
 * The return values of pre and post and treated the same way as in
 * pddlCondTraverse().
 */
void pddlCondRebuild(pddl_cond_t **c,
                     int (*pre)(pddl_cond_t **, void *),
                     int (*post)(pddl_cond_t **, void *),
                     void *userdata);

/**
 * When first (when ...) node, that has non-static preconditions, is found,
 * it is removed and returned.
 * If no (when ...) is found, NULL is returned.
 * The function requires that c is the (and ...) node.
 */
pddl_cond_when_t *pddlCondRemoveFirstNonStaticWhen(pddl_cond_t *c,
                                                   const pddl_t *pddl);
pddl_cond_when_t *pddlCondRemoveFirstWhen(pddl_cond_t *c, const pddl_t *pddl);

/**
 * Creates a new (and a b) node.
 * The objects a and b should not be used after this call.
 */
pddl_cond_t *pddlCondNewAnd2(pddl_cond_t *a, pddl_cond_t *b);

/**
 * Creates a new empty (and ) node.
 */
pddl_cond_t *pddlCondNewEmptyAnd(void);

/**
 * Creates a new empty atom with the specified number of arguments all set
 * as "undefined".
 */
pddl_cond_atom_t *pddlCondNewEmptyAtom(int num_args);

/**
 * Returns true if the conditional contains any atom.
 */
int pddlCondHasAtom(const pddl_cond_t *c);

/**
 * Parse condition from PDDL lisp.
 */
pddl_cond_t *pddlCondParse(const pddl_lisp_node_t *root,
                           pddl_t *pddl,
                           const pddl_params_t *params,
                           const char *err_prefix,
                           bor_err_t *err);

/**
 * Parse (:init ...) into a conjuction of atoms.
 */
pddl_cond_part_t *pddlCondParseInit(const pddl_lisp_node_t *root, pddl_t *pddl,
                                    bor_err_t *err);

/**
 * Transforms atom into (and atom).
 */
pddl_cond_t *pddlCondAtomToAnd(pddl_cond_t *atom);

/**
 * Creates a new atom that corresponds to a grounded fact.
 */
pddl_cond_atom_t *pddlCondCreateFactAtom(int pred, int arg_size, 
                                         const pddl_obj_id_t *arg);

/**
 * Adds {c} to and/or condition.
 */
void pddlCondPartAdd(pddl_cond_part_t *part, pddl_cond_t *c);

/**
 * Removes {c} from the and/or condition
 */
void pddlCondPartRm(pddl_cond_part_t *part, pddl_cond_t *c);

/**
 * Returns 0 if cond is a correct precondition, -1 otherwise.
 */
int pddlCondCheckPre(const pddl_cond_t *cond, int require, bor_err_t *err);

/**
 * Same as pddlCondCheckPre() buf effect is checked.
 */
int pddlCondCheckEff(const pddl_cond_t *cond, int require, bor_err_t *err);


/**
 * Set .read to true for all found atoms.
 */
void pddlCondSetPredRead(const pddl_cond_t *cond, pddl_preds_t *preds);

/**
 * Set .write to true for all found atoms, and set .read to true for all
 * atoms found as precondtions in (when ) statement.
 */
void pddlCondSetPredReadWriteEff(const pddl_cond_t *cond, pddl_preds_t *preds);

/**
 * Normalize conditionals by instantiation qunatifiers and transformation to
 * DNF so that the actions can be split.
 */
pddl_cond_t *pddlCondNormalize(pddl_cond_t *cond, const pddl_t *pddl,
                               const pddl_params_t *params);

/**
 * Remove atom node duplicates.
 */
pddl_cond_t *pddlCondDeduplicate(pddl_cond_t *cond, const pddl_t *pddl);

/**
 * If conflicting literals are found
 *   1) in the and node, then the and node is replaced by false
 *   2) in the or node, the literals are removed (as if they were replaced
 *      by true are simplified).
 */
pddl_cond_t *pddlCondDeconflictPre(pddl_cond_t *cond, const pddl_t *pddl,
                                   const pddl_params_t *params);

/**
 * If conflicting literals are found
 *   1) in the and node, then the positive literal is kept (following the
 *      rule "first delete then add".
 *   2) in the or node, the error is reported.
 */
pddl_cond_t *pddlCondDeconflictEff(pddl_cond_t *cond, const pddl_t *pddl,
                                   const pddl_params_t *params);

/**
 * Returns true if the atom is a grounded fact.
 */
int pddlCondAtomIsGrounded(const pddl_cond_atom_t *atom);

/**
 * Compares two atoms.
 */
int pddlCondAtomCmp(const pddl_cond_atom_t *a1,
                    const pddl_cond_atom_t *a2);

/**
 * Compares two atoms without considering negation (.neg flag).
 */
int pddlCondAtomCmpNoNeg(const pddl_cond_atom_t *a1,
                         const pddl_cond_atom_t *a2);

/**
 * Returns true if a1 and a2 are negations of each other.
 */
int pddlCondAtomInConflict(const pddl_cond_atom_t *a1,
                           const pddl_cond_atom_t *a2,
                           const pddl_t *pddl);

void pddlCondPrint(const pddl_t *pddl,
                   const pddl_cond_t *cond,
                   const pddl_params_t *params,
                   FILE *fout);

void pddlCondPrintPDDL(const pddl_cond_t *cond,
                       const pddl_t *pddl,
                       const pddl_params_t *params,
                       FILE *fout);



struct pddl_cond_const_it_atom {
    const bor_list_t *list;
    const bor_list_t *cur;
};
typedef struct pddl_cond_const_it_atom pddl_cond_const_it_atom_t;


const pddl_cond_atom_t *pddlCondConstItAtomInit(pddl_cond_const_it_atom_t *it,
                                                const pddl_cond_t *cond);
const pddl_cond_atom_t *pddlCondConstItAtomNext(pddl_cond_const_it_atom_t *it);

#define PDDL_COND_FOR_EACH_ATOM(COND, IT, ATOM) \
    for ((ATOM) = pddlCondConstItAtomInit((IT), (COND)); \
            (ATOM) != NULL; \
            (ATOM) = pddlCondConstItAtomNext((IT)))

#define PDDL_COND_FOR_EACH_CONT(IT, ATOM) \
    for ((ATOM) = pddlCondConstItAtomNext((IT)); \
            (ATOM) != NULL; \
            (ATOM) = pddlCondConstItAtomNext((IT)))

struct pddl_cond_const_it_eff {
    const bor_list_t *list;
    const bor_list_t *cur;
    const pddl_cond_t *when_pre;
    const bor_list_t *when_list;
    const bor_list_t *when_cur;
};
typedef struct pddl_cond_const_it_eff pddl_cond_const_it_eff_t;

const pddl_cond_atom_t *pddlCondConstItEffInit(pddl_cond_const_it_eff_t *it,
                                               const pddl_cond_t *cond,
                                               const pddl_cond_t **pre);
const pddl_cond_atom_t *pddlCondConstItEffNext(pddl_cond_const_it_eff_t *it,
                                               const pddl_cond_t **pre);

#define PDDL_COND_FOR_EACH_EFF(COND, IT, ATOM, PRE) \
    for ((ATOM) = pddlCondConstItEffInit((IT), (COND), &(PRE)); \
            (ATOM) != NULL; \
            (ATOM) = pddlCondConstItEffNext((IT), &(PRE)))
#define PDDL_COND_FOR_EACH_EFF_CONT(IT, ATOM, PRE) \
    for ((ATOM) = pddlCondConstItEffNext((IT), &(PRE)); \
            (ATOM) != NULL; \
            (ATOM) = pddlCondConstItEffNext((IT), &(PRE)))

#define PDDL_COND_FOR_EACH_ADD_EFF(COND, IT, ATOM, PRE) \
    PDDL_COND_FOR_EACH_EFF((COND), (IT), (ATOM), (PRE)) \
        if (!(ATOM)->neg)
#define PDDL_COND_FOR_EACH_ADD_EFF_CONT(IT, ATOM, PRE) \
    PDDL_COND_FOR_EACH_EFF_CONT((IT), (ATOM), (PRE)) \
        if (!(ATOM)->neg)

#define PDDL_COND_FOR_EACH_DEL_EFF(COND, IT, ATOM, PRE) \
    PDDL_COND_FOR_EACH_EFF((COND), (IT), (ATOM), (PRE)) \
        if ((ATOM)->neg)
#define PDDL_COND_FOR_EACH_DEL_EFF_CONT(IT, ATOM, PRE) \
    PDDL_COND_FOR_EACH_EFF_CONT((IT), (ATOM), (PRE)) \
        if ((ATOM)->neg)

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_COND_H__ */
