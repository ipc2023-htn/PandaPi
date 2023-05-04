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

#include <boruvka/alloc.h>
#include <boruvka/hfunc.h>
#include "pddl/lifted_mgroup_htable.h"
#include "assert.h"

struct el {
    int id;
    pddl_lifted_mgroup_t mgroup;
    bor_htable_key_t hash;
    bor_list_t htable;
};
typedef struct el el_t;

static bor_htable_key_t mgroupHash(const pddl_lifted_mgroup_t *m)
{
    int *buf;
    int bufsize;

    bufsize = m->param.param_size * 2;
    for (int i = 0; i < m->cond.size; ++i){
        const pddl_cond_atom_t *a = PDDL_COND_CAST(m->cond.cond[i], atom);
        bufsize += 1 + a->arg_size;
    }

    buf = BOR_ALLOC_ARR(int, bufsize);

    for (int i = 0; i < m->param.param_size; ++i){
        buf[2 * i] = m->param.param[i].type;
        buf[2 * i + 1] = m->param.param[i].is_counted_var;
    }

    int ins = 2 * m->param.param_size;
    for (int i = 0; i < m->cond.size; ++i){
        const pddl_cond_atom_t *a = PDDL_COND_CAST(m->cond.cond[i], atom);
        buf[ins++] = a->pred;
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].param >= 0){
                buf[ins++] = a->arg[ai].param;
            }else{
                buf[ins++] = a->arg[ai].obj * -1;
            }
        }
    }

    ASSERT(ins == bufsize);
    bor_htable_key_t hash = borCityHash_64(buf, bufsize * sizeof(int));

    BOR_FREE(buf);
    return hash;
}

static bor_htable_key_t htableHash(const bor_list_t *k, void *_)
{
    el_t *m = BOR_LIST_ENTRY(k, el_t, htable);
    return m->hash;
}

static int htableEq(const bor_list_t *k1, const bor_list_t *k2, void *_)
{
    el_t *m1 = BOR_LIST_ENTRY(k1, el_t, htable);
    el_t *m2 = BOR_LIST_ENTRY(k2, el_t, htable);
    return pddlLiftedMGroupEq(&m1->mgroup, &m2->mgroup);
}


void pddlLiftedMGroupHTableInit(pddl_lifted_mgroup_htable_t *h)
{
    el_t el;

    bzero(h, sizeof(*h));
    h->htable = borHTableNew(htableHash, htableEq, h);

    bzero(&el, sizeof(el));
    h->mgroup = borExtArrNew(sizeof(el), NULL, &el);
    h->mgroup_size = 0;
}

void pddlLiftedMGroupHTableFree(pddl_lifted_mgroup_htable_t *h)
{
    for (int i = 0; i < h->mgroup_size; ++i){
        el_t *m = borExtArrGet(h->mgroup, i);
        pddlLiftedMGroupFree(&m->mgroup);
    }

    borHTableDel(h->htable);
    borExtArrDel(h->mgroup);
}

int pddlLiftedMGroupHTableAdd(pddl_lifted_mgroup_htable_t *h,
                              const pddl_lifted_mgroup_t *mg)
{
    el_t *el = borExtArrGet(h->mgroup, h->mgroup_size);
    el->mgroup = *mg;
    el->hash = mgroupHash(mg);

    bor_list_t *ins = borHTableInsertUnique(h->htable, &el->htable);
    if (ins == NULL){
        pddlLiftedMGroupInitCopy(&el->mgroup, mg);
        el->id = h->mgroup_size;
        ++h->mgroup_size;
        return el->id;

    }else{
        el = BOR_LIST_ENTRY(ins, el_t, htable);
        return el->id;
    }
}

const pddl_lifted_mgroup_t *pddlLiftedMGroupHTableGet(
                                const pddl_lifted_mgroup_htable_t *h, int id)
{
    if (id < 0 || id >= h->mgroup_size)
        return NULL;
    const el_t *e = borExtArrGet(h->mgroup, id);
    return &e->mgroup;
}
