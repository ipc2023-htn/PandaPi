/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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

#include <boruvka/alloc.h>
#include <boruvka/timer.h>
#include "pddl/strips.h"
#include "pddl/critical_path.h"
#include "pddl/disambiguation.h"
#include "assert.h"

#define REACHED 1
#define MUTEX -1
#define PRUNED -1
#define _FACT(h2, x, y) ((h2)->fact[(x) * (h2)->fact_size + (y)])

struct h2 {
    char *fact; /*!< 0/REACHED/MUTEX for each pair of facts */
    int fact_size;
    int op_size;
    char *op; /*!< 0/REACHED/PRUNED for each operator */
    char *op_fact;
    pddl_disambiguate_t *disambiguate;
};
typedef struct h2 h2_t;

_bor_inline int setReached(h2_t *h2, int f1, int f2)
{
    if (_FACT(h2, f1, f2) == 0){
        _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = REACHED;
        return 1;
    }
    return 0;
}

_bor_inline void setMutex(h2_t *h2, int f1, int f2)
{
    _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = MUTEX;
}

_bor_inline void reset(h2_t *h2, int f1, int f2)
{
    _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = 0;
}

_bor_inline int isNotReached(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == 0;
}

_bor_inline int isReached(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == REACHED;
}

_bor_inline int isMutex(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == MUTEX;
}


_bor_inline void setOpReached(h2_t *h2, int op_id)
{
    ASSERT(h2->op[op_id] == 0);
    h2->op[op_id] = REACHED;
}

_bor_inline void setOpPruned(h2_t *h2, int op_id)
{
    h2->op[op_id] = PRUNED;
}

_bor_inline void resetOp(h2_t *h2, int op_id)
{
    h2->op[op_id] = 0;
}

_bor_inline int isOpNotReached(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == 0;
}

_bor_inline int isOpReached(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == REACHED;
}

_bor_inline int isOpPruned(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == PRUNED;
}

static void setFwInit(h2_t *h2, const bor_iset_t *init)
{
    int f1, f2;
    BOR_ISET_FOR_EACH(init, f1){
        BOR_ISET_FOR_EACH(init, f2){
            setReached(h2, f1, f2);
        }
    }
}

static int isMutexWith(const h2_t *h2, int fact_id, const bor_iset_t *set)
{
    int fact_id2;
    BOR_ISET_FOR_EACH(set, fact_id2){
        if (isMutex(h2, fact_id, fact_id2))
            return 1;
    }
    return 0;
}

static void h2Init(h2_t *h2,
                   const pddl_strips_t *strips,
                   const pddl_mutex_pairs_t *mutexes,
                   const bor_iset_t *unreachable_facts,
                   const bor_iset_t *unreachable_ops,
                   bor_err_t *err)
{
    bzero(h2, sizeof(*h2));
    h2->fact_size = strips->fact.fact_size;
    h2->op_size = strips->op.op_size;
    h2->fact = BOR_CALLOC_ARR(char, (size_t)h2->fact_size * h2->fact_size);
    h2->op = BOR_CALLOC_ARR(char, h2->op_size);

    // Copy mutexes into the table
    PDDL_MUTEX_PAIRS_FOR_EACH(mutexes, f1, f2)
        setMutex(h2, f1, f2);

    if (unreachable_facts != NULL){
        int fact_id;
        BOR_ISET_FOR_EACH(unreachable_facts, fact_id){
            if (!isMutex(h2, fact_id, fact_id))
                setMutex(h2, fact_id, fact_id);
        }
    }

    if (unreachable_ops != NULL){
        int op_id;
        BOR_ISET_FOR_EACH(unreachable_ops, op_id){
            if (!isOpPruned(h2, op_id))
                setOpPruned(h2, op_id);
        }
    }
}

static void h2AllocOpFact(h2_t *h2, bor_err_t *err)
{
    size_t op_fact_size = (size_t)h2->fact_size * h2->op_size;
    h2->op_fact = calloc(op_fact_size, 1);
    if (h2->op_fact != NULL){
        BOR_INFO(err, "  h^2 uses additional memory of %.2f MB",
                op_fact_size / (1024. * 1024.));
    }
}

static void h2InitOpFact(h2_t *h2, const pddl_strips_ops_t *ops, bor_err_t *err)
{
    h2AllocOpFact(h2, err);
    if (h2->op_fact != NULL){
        for (int op_id = 0; op_id < h2->op_size; ++op_id){
            const pddl_strips_op_t *op = ops->op[op_id];
            char *fact = h2->op_fact + (size_t)op_id * h2->fact_size;
            int fact_id;
            BOR_ISET_FOR_EACH(&op->add_eff, fact_id)
                fact[fact_id] = -1;
            BOR_ISET_FOR_EACH(&op->del_eff, fact_id)
                fact[fact_id] = -1;
        }
    }
}

static void h2ResetOpFact(h2_t *h2, const pddl_strips_ops_t *ops)
{
    if (h2->op_fact == NULL)
        return;
    bzero(h2->op_fact, sizeof(char) * h2->fact_size * h2->op_size);
    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        const pddl_strips_op_t *op = ops->op[op_id];
        char *fact = h2->op_fact + (size_t)op_id * h2->fact_size;
        int fact_id;
        BOR_ISET_FOR_EACH(&op->add_eff, fact_id)
            fact[fact_id] = -1;
        BOR_ISET_FOR_EACH(&op->del_eff, fact_id)
            fact[fact_id] = -1;
    }
}

static void h2Free(h2_t *h2)
{
    if (h2->fact != NULL)
        BOR_FREE(h2->fact);
    if (h2->op != NULL)
        BOR_FREE(h2->op);
    if (h2->op_fact != NULL)
        free(h2->op_fact);
}

/** Returns true if operator is applicable with the currently reachable facts */
static int isApplicable(const pddl_strips_op_t *op, h2_t *h2)
{
    int f1, f2;

    if (isOpPruned(h2, op->id))
        return 0;

    if (isOpReached(h2, op->id))
        return 1;

    BOR_ISET_FOR_EACH(&op->pre, f1){
        BOR_ISET_FOR_EACH(&op->pre, f2){
            if (!isReached(h2, f1, f2))
                return 0;
        }
    }

    return 1;
}

/** Returns true if operator is applicable with the additional fact_id */
static int isApplicable2(const pddl_strips_op_t *op, int fact_id, h2_t *h2)
{
    int f1;

    if (!isOpReached(h2, op->id))
        return 0;
    if (!isReached(h2, fact_id, fact_id))
        return 0;
    if (h2->op_fact == NULL
            && (borISetHas(&op->add_eff, fact_id)
                    || borISetHas(&op->del_eff, fact_id))){
        return 0;
    }

    BOR_ISET_FOR_EACH(&op->pre, f1){
        if (!isReached(h2, f1, fact_id))
            return 0;
    }

    return 1;
}

/** Apply operator if currently applicable */
static int applyOp(const pddl_strips_op_t *op, h2_t *h2)
{
    int f1, f2;
    int updated = 0;
    char *op_fact = NULL;

    if (!isApplicable(op, h2))
        return 0;

    if (!isOpReached(h2, op->id)){
        // This needs to be run only the first time the operator is
        // applied.
        BOR_ISET_FOR_EACH(&op->add_eff, f1){
            BOR_ISET_FOR_EACH(&op->add_eff, f2){
                updated |= setReached(h2, f1, f2);
            }
        }
        // This needs to be set here because isApplicable2 depends on it
        setOpReached(h2, op->id);
    }

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (h2->op_fact != NULL)
            op_fact = h2->op_fact + (size_t)op->id * h2->fact_size;
        if (op_fact != NULL && op_fact[fact_id])
            continue;
        if (isApplicable2(op, fact_id, h2)){
            if (op_fact != NULL)
                op_fact[fact_id] = 1;
            BOR_ISET_FOR_EACH(&op->add_eff, f1)
                updated |= setReached(h2, f1, fact_id);
        }
    }

    return updated;
}

static int h2Run(h2_t *h2, const pddl_strips_ops_t *ops, bor_err_t *err)
{
    int updated;
    int ret = 0;

    do {
        updated = 0;
        for (int op_id = 0; op_id < ops->op_size; ++op_id){
            const pddl_strips_op_t *op = ops->op[op_id];
            updated |= applyOp(op, h2);
        }
    } while (updated);

    for (int f1 = 0; f1 < h2->fact_size; ++f1){
        for (int f2 = f1; f2 < h2->fact_size; ++f2){
            if (isNotReached(h2, f1, f2)){
                if (!isMutex(h2, f1, f2)){
                    setMutex(h2, f1, f2);
                    if (h2->disambiguate != NULL)
                        pddlDisambiguateAddMutex(h2->disambiguate, f1, f2);
                    ret = 1;
                }
            }else if (isReached(h2, f1, f2)){
                reset(h2, f1, f2);
            }
        }
    }

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpNotReached(h2, op_id)){
            if (!isOpPruned(h2, op_id)){
                setOpPruned(h2, op_id);
                ret = 1;
            }
        }else if (isOpReached(h2, op_id)){
            resetOp(h2, op_id);
        }
    }

    return ret;
}

static void outUnreachableOps(const h2_t *h2, bor_iset_t *unreachable_ops)
{
    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            borISetAdd(unreachable_ops, op_id);
    }
}

static void outMutexes(const h2_t *h2,
                       pddl_mutex_pairs_t *mutexes,
                       bor_iset_t *unreachable_facts)
{
    for (int f1 = 0; f1 < h2->fact_size; ++f1){
        for (int f2 = f1; f2 < h2->fact_size; ++f2){
            if (isMutex(h2, f1, f2)){
                pddlMutexPairsAdd(mutexes, f1, f2);
                if (f1 == f2 && unreachable_facts != NULL)
                    borISetAdd(unreachable_facts, f1);
            }
        }
    }
}

static void setOutput(const h2_t *h2,
                      pddl_mutex_pairs_t *mutexes,
                      bor_iset_t *unreachable_facts,
                      bor_iset_t *unreachable_ops)
{
    outMutexes(h2, mutexes, unreachable_facts);
    if (unreachable_ops != NULL)
        outUnreachableOps(h2, unreachable_ops);
}


int pddlH2(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *m,
           bor_iset_t *unreachable_facts,
           bor_iset_t *unreachable_ops,
           bor_err_t *err)
{
    if (strips->has_cond_eff)
        BOR_ERR_RET2(err, -1, "h^2: Conditional effects not supported!");

    BOR_INFO(err, "h^2. facts: %d, ops: %d, mutex pairs: %lu",
             strips->fact.fact_size,
             strips->op.op_size,
             (unsigned long)m->num_mutex_pairs);

    h2_t h2;

    h2Init(&h2, strips, m, unreachable_facts, unreachable_ops, err);
    h2InitOpFact(&h2, &strips->op, err);

    setFwInit(&h2, &strips->init);
    h2Run(&h2, &strips->op, err);

    setOutput(&h2, m, unreachable_facts, unreachable_ops);

    BOR_INFO(err, "h^2 DONE. mutex pairs: %lu, unreachable facts: %d,"
                  " unreachable ops: %d",
             (unsigned long)m->num_mutex_pairs,
             (unreachable_facts != NULL ? borISetSize(unreachable_facts) : -1),
             (unreachable_ops != NULL ? borISetSize(unreachable_ops) : -1));

    h2Free(&h2);
    return 0;
}

static void setBwInit(h2_t *h2, const bor_iset_t *goal_in)
{
    BOR_ISET(goal);
    borISetUnion(&goal, goal_in);

    if (h2->disambiguate != NULL)
        pddlDisambiguateSet(h2->disambiguate, &goal);

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (isMutex(h2, fact_id, fact_id) || isMutexWith(h2, fact_id, &goal))
            continue;

        setReached(h2, fact_id, fact_id);
    }

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (!isReached(h2, fact_id, fact_id))
            continue;

        for (int fact_id2 = fact_id + 1; fact_id2 < h2->fact_size; ++fact_id2){
            if (!isReached(h2, fact_id2, fact_id2)
                    || isMutex(h2, fact_id, fact_id2)){
                continue;
            }

            setReached(h2, fact_id, fact_id2);
        }
    }

    borISetFree(&goal);
}

static void opSetEDeletes(pddl_strips_op_t *bw_op,
                          const pddl_strips_op_t *fw_op,
                          const h2_t *h2)
{
    // Set e-deletes -- fw_op->pre contains prevails and delete effects.
    // We can't iterate over fw_op->del_eff \setminus sop->pre!
    int pre_fact;
    BOR_ISET_FOR_EACH(&fw_op->pre, pre_fact){
        for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
            if (isMutex(h2, pre_fact, fact_id))
                borISetAdd(&bw_op->del_eff, fact_id);
        }
    }
    pddlStripsOpNormalize(bw_op);
}

static void opInitBw(pddl_strips_op_t *bw_op,
                     const pddl_strips_op_t *fw_op,
                     const h2_t *h2)
{
    // Erase bw operator
    borISetEmpty(&bw_op->pre);
    borISetEmpty(&bw_op->add_eff);
    borISetEmpty(&bw_op->del_eff);

    // Set precondition as prevail + add effect from sop
    borISetMinus2(&bw_op->pre, &fw_op->pre, &fw_op->del_eff);
    borISetUnion(&bw_op->pre, &fw_op->add_eff);

    // Set add effects as fw_op's delete effects
    borISetSet(&bw_op->add_eff, &fw_op->del_eff);

    opSetEDeletes(bw_op, fw_op, h2);
}

static void opsInitBw(pddl_strips_ops_t *bw_ops,
                      const pddl_strips_ops_t *fw_ops,
                      const h2_t *h2)
{
    pddlStripsOpsInit(bw_ops);
    pddlStripsOpsCopy(bw_ops, fw_ops);
    for (int op_id = 0; op_id < h2->op_size; ++op_id)
        opInitBw(bw_ops->op[op_id], fw_ops->op[op_id], h2);
}

static int opsUpdateBw(pddl_strips_ops_t *bw_ops,
                        const pddl_strips_ops_t *fw_ops,
                        h2_t *h2)
{
    int ret = 0;

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            continue;

        pddl_strips_op_t *bw_op = bw_ops->op[op_id];
        const pddl_strips_op_t *fw_op = fw_ops->op[op_id];
        if (h2->disambiguate != NULL){
            if (pddlDisambiguateSet(h2->disambiguate, &bw_op->pre) < 0){
                setOpPruned(h2, op_id);
                ret = 1;
                continue;
            }
        }
        opSetEDeletes(bw_op, fw_op, h2);
    }

    return ret;
}

static int opsUpdateFw(pddl_strips_ops_t *fw_ops, h2_t *h2)
{
    int ret = 0;

    if (h2->disambiguate == NULL)
        return 0;

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            continue;

        pddl_strips_op_t *op = fw_ops->op[op_id];
        if (pddlDisambiguateSet(h2->disambiguate, &op->pre) < 0){
            setOpPruned(h2, op_id);
            ret = 1;
            continue;
        }
    }

    return ret;
}

int pddlH2FwBw(const pddl_strips_t *strips,
               const pddl_mgroups_t *mgroup,
               pddl_mutex_pairs_t *mutex,
               bor_iset_t *unreachable_facts,
               bor_iset_t *unreachable_ops,
               bor_err_t *err)
{
    if (strips->has_cond_eff)
        BOR_ERR_RET2(err, -1, "h^2 fw/bw: Conditional effects not supported!");

    BOR_INFO(err, "h^2 fw/bw. facts: %d, ops: %d, mutex pairs: %lu",
             strips->fact.fact_size,
             strips->op.op_size,
             (unsigned long)mutex->num_mutex_pairs);

    h2_t h2;
    int update_fw = 1;
    int update_bw = 1;
    pddl_strips_ops_t ops_fw, ops_bw;
    pddl_disambiguate_t disamb;

    h2Init(&h2, strips, mutex, unreachable_facts, unreachable_ops, err);

    pddlStripsOpsInit(&ops_fw);
    pddlStripsOpsCopy(&ops_fw, &strips->op);
    opsInitBw(&ops_bw, &ops_fw, &h2);

    if (pddlDisambiguateInit(&disamb, h2.fact_size, mutex, mgroup) == 0)
        h2.disambiguate = &disamb;

    h2AllocOpFact(&h2, err);

    while (update_fw || update_bw){
        if (update_fw){
            update_fw = 0;
            setFwInit(&h2, &strips->init);
            opsUpdateFw(&ops_fw, &h2);
            h2ResetOpFact(&h2, &ops_fw);
            update_bw |= h2Run(&h2, &ops_fw, err);
        }

        if (update_bw){
            update_bw = 0;
            setBwInit(&h2, &strips->goal);
            update_fw |= opsUpdateFw(&ops_fw, &h2);
            opsUpdateBw(&ops_bw, &ops_fw, &h2);
            h2ResetOpFact(&h2, &ops_bw);
            update_fw |= h2Run(&h2, &ops_bw, err);
        }
    }

    pddlStripsOpsFree(&ops_fw);
    pddlStripsOpsFree(&ops_bw);
    if (h2.disambiguate != NULL)
        pddlDisambiguateFree(&disamb);

    setOutput(&h2, mutex, unreachable_facts, unreachable_ops);

    BOR_INFO(err, "h^2 fw/bw DONE. mutex pairs: %lu, unreachable facts: %d,"
                  " unreachable ops: %d",
             (unsigned long)mutex->num_mutex_pairs,
             (unreachable_facts != NULL ? borISetSize(unreachable_facts) : -1),
             (unreachable_ops != NULL ? borISetSize(unreachable_ops) : -1));

    h2Free(&h2);
    return 0;
}
