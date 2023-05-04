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

#include <limits.h>
#include <boruvka/sort.h>
#include <boruvka/iarr.h>
#include <boruvka/hfunc.h>
#include "pddl/pddl.h"
#include "pddl/strips_op.h"
#include "helper.h"
#include "assert.h"

void pddlStripsOpInit(pddl_strips_op_t *op)
{
    bzero(op, sizeof(*op));
}

static void condEffFree(pddl_strips_op_cond_eff_t *ce)
{
    borISetFree(&ce->pre);
    borISetFree(&ce->add_eff);
    borISetFree(&ce->del_eff);
}

void pddlStripsOpFree(pddl_strips_op_t *op)
{
    if (op->name)
        BOR_FREE(op->name);
    borISetFree(&op->pre);
    borISetFree(&op->del_eff);
    borISetFree(&op->add_eff);
    for (int i = 0; i < op->cond_eff_size; ++i)
        condEffFree(&op->cond_eff[i]);
    if (op->cond_eff != NULL)
        BOR_FREE(op->cond_eff);
}

void pddlStripsOpFreeAllCondEffs(pddl_strips_op_t *op)
{
    for (int i = 0; i < op->cond_eff_size; ++i)
        condEffFree(&op->cond_eff[i]);
    op->cond_eff_size = 0;
}

pddl_strips_op_t *pddlStripsOpNew(void)
{
    pddl_strips_op_t *op;
    op = BOR_ALLOC(pddl_strips_op_t);
    pddlStripsOpInit(op);
    return op;
}

void pddlStripsOpDel(pddl_strips_op_t *op)
{
    pddlStripsOpFree(op);
    BOR_FREE(op);
}

static pddl_strips_op_cond_eff_t *addCondEff(pddl_strips_op_t *op)
{
    pddl_strips_op_cond_eff_t *ce;

    if (op->cond_eff_size >= op->cond_eff_alloc){
        if (op->cond_eff_alloc == 0)
            op->cond_eff_alloc = 1;
        op->cond_eff_alloc *= 2;
        op->cond_eff = BOR_REALLOC_ARR(op->cond_eff,
                                       pddl_strips_op_cond_eff_t,
                                       op->cond_eff_alloc);
    }

    ce = op->cond_eff + op->cond_eff_size++;
    bzero(ce, sizeof(*ce));
    return ce;
}

pddl_strips_op_cond_eff_t *pddlStripsOpAddCondEff(pddl_strips_op_t *op,
                                                  const pddl_strips_op_t *f)
{
    pddl_strips_op_cond_eff_t *ce = addCondEff(op);
    borISetUnion(&ce->pre, &f->pre);
    borISetUnion(&ce->add_eff, &f->add_eff);
    borISetUnion(&ce->del_eff, &f->del_eff);
    return ce;
}

void pddlStripsOpNormalize(pddl_strips_op_t *op)
{
    borISetMinus(&op->del_eff, &op->add_eff);
    borISetMinus(&op->add_eff, &op->pre);
}

int pddlStripsOpFinalize(pddl_strips_op_t *op, char *name)
{
    op->name = name;
    pddlStripsOpNormalize(op);
    if (op->add_eff.size == 0 && op->del_eff.size == 0)
        return -1;
    return 0;
}

void pddlStripsOpAddEffFromOp(pddl_strips_op_t *dst,
                              const pddl_strips_op_t *src)
{
    borISetUnion(&dst->add_eff, &src->add_eff);
    borISetUnion(&dst->del_eff, &src->del_eff);
    pddlStripsOpNormalize(dst);
}

void pddlStripsOpCopy(pddl_strips_op_t *dst, const pddl_strips_op_t *src)
{
    pddl_strips_op_cond_eff_t *ce;

    pddlStripsOpCopyWithoutCondEff(dst, src);
    for (int i = 0; i < src->cond_eff_size; ++i){
        const pddl_strips_op_cond_eff_t *f = src->cond_eff + i;
        ce = addCondEff(dst);
        borISetUnion(&ce->pre, &f->pre);
        borISetUnion(&ce->add_eff, &f->add_eff);
        borISetUnion(&ce->del_eff, &f->del_eff);
    }
}

void pddlStripsOpCopyWithoutCondEff(pddl_strips_op_t *dst,
                                    const pddl_strips_op_t *src)
{
    dst->name = BOR_STRDUP(src->name);
    dst->cost = src->cost;
    borISetUnion(&dst->pre, &src->pre);
    borISetUnion(&dst->add_eff, &src->add_eff);
    borISetUnion(&dst->del_eff, &src->del_eff);
}

void pddlStripsOpCopyDual(pddl_strips_op_t *dst, const pddl_strips_op_t *src)
{
    pddl_strips_op_cond_eff_t *ce;

    dst->name = BOR_STRDUP(src->name);
    dst->cost = src->cost;
    borISetUnion(&dst->pre, &src->del_eff);
    borISetUnion(&dst->add_eff, &src->add_eff);
    borISetUnion(&dst->del_eff, &src->pre);
    for (int i = 0; i < src->cond_eff_size; ++i){
        const pddl_strips_op_cond_eff_t *f = src->cond_eff + i;
        ce = addCondEff(dst);
        borISetUnion(&ce->pre, &f->del_eff);
        borISetUnion(&ce->add_eff, &f->add_eff);
        borISetUnion(&ce->del_eff, &f->pre);
    }
}

void pddlStripsOpRemapFacts(pddl_strips_op_t *op, const int *remap)
{
    pddl_strips_op_cond_eff_t *ce;

    pddlISetRemap(&op->pre, remap);
    pddlISetRemap(&op->add_eff, remap);
    pddlISetRemap(&op->del_eff, remap);
    for (int i = 0; i < op->cond_eff_size; ++i){
        ce = op->cond_eff + i;
        pddlISetRemap(&ce->pre, remap);
        pddlISetRemap(&ce->add_eff, remap);
        pddlISetRemap(&ce->del_eff, remap);
    }
}

static void iarrAppendISet(bor_iarr_t *arr, const bor_iset_t *set)
{
    int fact;
    BOR_ISET_FOR_EACH(set, fact)
        borIArrAdd(arr, fact);
}

static uint64_t opHash(const pddl_strips_op_t *op)
{
    const int delim = INT_MAX;
    BOR_IARR(buf);
    uint64_t hash;

    iarrAppendISet(&buf, &op->pre);
    borIArrAdd(&buf, delim);
    iarrAppendISet(&buf, &op->add_eff);
    borIArrAdd(&buf, delim);
    iarrAppendISet(&buf, &op->del_eff);
    borIArrAdd(&buf, delim);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        iarrAppendISet(&buf, &ce->pre);
        borIArrAdd(&buf, delim);
        iarrAppendISet(&buf, &ce->add_eff);
        borIArrAdd(&buf, delim);
        iarrAppendISet(&buf, &ce->del_eff);
        borIArrAdd(&buf, delim);
    }

    hash = borCityHash_64(buf.arr, buf.size);
    borIArrFree(&buf);

    return hash;
}

static uint64_t opEq(const pddl_strips_op_t *op1,
                     const pddl_strips_op_t *op2)
{
    if (op1->cond_eff_size != op2->cond_eff_size)
        return 0;

    if (!borISetEq(&op1->pre, &op2->pre)
            || !borISetEq(&op1->add_eff, &op2->add_eff)
            || !borISetEq(&op2->del_eff, &op2->del_eff))
        return 0;

    for (int cei = 0; cei < op1->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce1 = op1->cond_eff + cei;
        const pddl_strips_op_cond_eff_t *ce2 = op2->cond_eff + cei;
        if (!borISetEq(&ce1->pre, &ce2->pre)
                || !borISetEq(&ce1->add_eff, &ce2->add_eff)
                || !borISetEq(&ce1->del_eff, &ce2->del_eff))
            return 0;
    }
    return 1;
}

struct deduplicate {
    int id;
    int cost;
    uint64_t hash;
};
typedef struct deduplicate deduplicate_t;

static int opDeduplicateCmp(const void *_a, const void *_b, void *_)
{
    const deduplicate_t *a = _a;
    const deduplicate_t *b = _b;
    if (a->hash < b->hash)
        return -1;
    if (a->hash > b->hash)
        return 1;
    if (a->cost == b->cost)
        return a->id - b->id;
    return a->cost - b->cost;
}

static int deduplicateRange(const pddl_strips_ops_t *ops,
                            deduplicate_t *dedup,
                            int start,
                            int end,
                            int *remove)
{
    int change = 0;

    for (int di = start; di < end; ++di){
        const deduplicate_t *d1 = dedup + di;
        if (d1->id < 0)
            continue;

        for (int di2 = di + 1; di2 < end; ++di2){
            deduplicate_t *d2 = dedup + di2;
            if (d2->id < 0)
                continue;

            if (opEq(ops->op[d1->id], ops->op[d2->id])){
                // dedup is sorted by the cost so it always holds that
                // d1->cost <= d2->cost
                remove[d2->id] = 1;
                change = 1;
                d2->id = -1;
            }
        }
    }

    return change;
}

void pddlStripsOpsDeduplicate(pddl_strips_ops_t *ops)
{
    deduplicate_t *dedup;
    int *remove;
    int change = 0;

    remove = BOR_CALLOC_ARR(int, ops->op_size);
    dedup = BOR_CALLOC_ARR(deduplicate_t, ops->op_size);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        dedup[op_id].id = op_id;
        dedup[op_id].cost = ops->op[op_id]->cost;
        dedup[op_id].hash = opHash(ops->op[op_id]);
    }

    borSort(dedup, ops->op_size, sizeof(deduplicate_t),
            opDeduplicateCmp, NULL);

    int start, cur;
    for (start = 0, cur = 1; cur < ops->op_size; ++cur){
        if (dedup[cur].hash != dedup[start].hash){
            if (start < cur - 1)
                change |= deduplicateRange(ops, dedup, start, cur, remove);
            start = cur;
        }
    }
    if (start < cur - 1)
        change |= deduplicateRange(ops, dedup, start, cur, remove);

    if (change)
        pddlStripsOpsDelOps(ops, remove);
    BOR_FREE(dedup);
    BOR_FREE(remove);
}

static int opCmp(const void *a, const void *b, void *_)
{
    pddl_strips_op_t *o1 = *(pddl_strips_op_t **)a;
    pddl_strips_op_t *o2 = *(pddl_strips_op_t **)b;
    int cmp = strcmp(o1->name, o2->name);
    if (cmp == 0)
        cmp = borISetCmp(&o1->pre, &o2->pre);
    if (cmp == 0)
        cmp = borISetCmp(&o1->add_eff, &o2->add_eff);
    if (cmp == 0)
        cmp = borISetCmp(&o1->del_eff, &o2->del_eff);
    return cmp;
}

void pddlStripsOpsSort(pddl_strips_ops_t *ops)
{
    borSort(ops->op, ops->op_size, sizeof(pddl_strips_op_t *), opCmp, NULL);
    for (int i = 0; i < ops->op_size; ++i)
        ops->op[i]->id = i;
}

static void reorderCondEffs(pddl_strips_op_t *op)
{
    int size = 0;
    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        if (borISetSize(&ce->pre) == 0){
            condEffFree(&op->cond_eff[cei]);
        }else{
            op->cond_eff[size++] = op->cond_eff[cei];
        }
    }
    op->cond_eff_size = size;
    pddlStripsOpNormalize(op);
}

void pddlStripsOpRemoveFact(pddl_strips_op_t *op, int fact_id)
{
    int reorder = 0;

    borISetRm(&op->pre, fact_id);
    borISetRm(&op->add_eff, fact_id);
    borISetRm(&op->del_eff, fact_id);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        borISetRm(&ce->pre, fact_id);
        borISetRm(&ce->add_eff, fact_id);
        borISetRm(&ce->del_eff, fact_id);
        if (borISetSize(&ce->pre) == 0){
            borISetUnion(&op->add_eff, &ce->add_eff);
            borISetUnion(&op->del_eff, &ce->del_eff);
            reorder = 1;
        }
    }

    if (reorder)
        reorderCondEffs(op);
}

void pddlStripsOpRemoveFacts(pddl_strips_op_t *op, const bor_iset_t *facts)
{
    int reorder = 0;

    borISetMinus(&op->pre, facts);
    borISetMinus(&op->add_eff, facts);
    borISetMinus(&op->del_eff, facts);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        borISetMinus(&ce->pre, facts);
        borISetMinus(&ce->add_eff, facts);
        borISetMinus(&ce->del_eff, facts);
        if (borISetSize(&ce->pre) == 0){
            borISetUnion(&op->add_eff, &ce->add_eff);
            borISetUnion(&op->del_eff, &ce->del_eff);
            reorder = 1;
        }
    }

    if (reorder)
        reorderCondEffs(op);
}


void pddlStripsOpsInit(pddl_strips_ops_t *ops)
{
    bzero(ops, sizeof(*ops));
    ops->op_alloc = 4;
    ops->op = BOR_ALLOC_ARR(pddl_strips_op_t *, ops->op_alloc);
}

void pddlStripsOpsFree(pddl_strips_ops_t *ops)
{
    for (int i = 0; i < ops->op_size; ++i){
        if (ops->op[i])
            pddlStripsOpDel(ops->op[i]);
    }
    if (ops->op != NULL)
        BOR_FREE(ops->op);
}

void pddlStripsOpsCopy(pddl_strips_ops_t *dst, const pddl_strips_ops_t *src)
{
    for (int i = 0; i < src->op_size; ++i)
        pddlStripsOpsAdd(dst, src->op[i]);
}

static pddl_strips_op_t *nextNewOp(pddl_strips_ops_t *ops)
{
    pddl_strips_op_t *op;

    if (ops->op_size >= ops->op_alloc){
        ops->op_alloc *= 2;
        ops->op = BOR_REALLOC_ARR(ops->op, pddl_strips_op_t *, ops->op_alloc);
    }

    op = pddlStripsOpNew();
    op->id = ops->op_size;
    ops->op[ops->op_size] = op;
    ++ops->op_size;
    return op;
}

int pddlStripsOpsAdd(pddl_strips_ops_t *ops, const pddl_strips_op_t *add)
{
    pddl_strips_op_t *op;
    op = nextNewOp(ops);
    pddlStripsOpCopy(op, add);
    return op->id;
}

void pddlStripsOpsDelOps(pddl_strips_ops_t *ops, const int *m)
{
    int ins = 0;
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        if (m[op_id]){
            pddlStripsOpDel(ops->op[op_id]);
        }else{
            ops->op[op_id]->id = ins;
            ops->op[ins++] = ops->op[op_id];
        }
    }

    ops->op_size = ins;
}

void pddlStripsOpsDelOpsSet(pddl_strips_ops_t *ops, const bor_iset_t *del_ops)
{
    int op_id;
    BOR_ISET_FOR_EACH(del_ops, op_id){
        pddlStripsOpDel(ops->op[op_id]);
        ops->op[op_id] = NULL;
    }

    int ins = 0;
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        if (ops->op[op_id] != NULL){
            ops->op[op_id]->id = ins;
            ops->op[ins++] = ops->op[op_id];
        }
    }

    ops->op_size = ins;
}

void pddlStripsOpsRemapFacts(pddl_strips_ops_t *ops, const int *remap)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpRemapFacts(ops->op[i], remap);
}

void pddlStripsOpsRemoveFacts(pddl_strips_ops_t *ops, const bor_iset_t *facts)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpRemoveFacts(ops->op[i], facts);
}

void pddlStripsOpPrintDebug(const pddl_strips_op_t *op,
                            const pddl_facts_t *fs,
                            FILE *fout)
{
    fprintf(fout, "  (%s), cost: %d\n", op->name, op->cost);

    fprintf(fout, "    pre:");
    pddlFactsPrintSet(&op->pre, fs, " ", "", fout);
    fprintf(fout, "\n");
    fprintf(fout, "    add:");
    pddlFactsPrintSet(&op->add_eff, fs, " ", "", fout);
    fprintf(fout, "\n");
    fprintf(fout, "    del:");
    pddlFactsPrintSet(&op->del_eff, fs, " ", "", fout);
    fprintf(fout, "\n");

    if (op->cond_eff_size > 0)
        fprintf(fout, "    cond-eff[%d]:\n", op->cond_eff_size);

    for (int j = 0; j < op->cond_eff_size; ++j){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + j;

        fprintf(fout, "      pre:");
        pddlFactsPrintSet(&ce->pre, fs, " ", "", fout);
        fprintf(fout, "\n");
        fprintf(fout, "      add:");
        pddlFactsPrintSet(&ce->add_eff, fs, " ", "", fout);
        fprintf(fout, "\n");
        fprintf(fout, "      del:");
        pddlFactsPrintSet(&ce->del_eff, fs, " ", "", fout);
        fprintf(fout, "\n");
    }
}

void pddlStripsOpsPrintDebug(const pddl_strips_ops_t *ops,
                             const pddl_facts_t *fs,
                             FILE *fout)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpPrintDebug(ops->op[i], fs, fout);
}
