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

#include <boruvka/compiler.h>
#include <boruvka/alloc.h>
#include <boruvka/hfunc.h>
#include <boruvka/sort.h>
#include "pddl/ground_atom.h"
#include "err.h"
#include "assert.h"


#define PDDL_GROUND_ATOM_STACK(NAME, ARG_SIZE) \
    pddl_ground_atom_t NAME; \
    pddl_obj_id_t __##NAME##__arg[ARG_SIZE]; \
    NAME.arg = __##NAME##__arg


/** Compares two ground atoms. */
static int pddlGroundAtomCmp(const pddl_ground_atom_t *a1,
                             const pddl_ground_atom_t *a2);


static bor_htable_key_t htableKey(const bor_list_t *key, void *_)
{
    pddl_ground_atom_t *a = BOR_LIST_ENTRY(key, pddl_ground_atom_t, htable);
    return a->hash;
}

static int htableEq(const bor_list_t *k1,
                    const bor_list_t *k2, void *_)
{
    pddl_ground_atom_t *a1 = BOR_LIST_ENTRY(k1, pddl_ground_atom_t, htable);
    pddl_ground_atom_t *a2 = BOR_LIST_ENTRY(k2, pddl_ground_atom_t, htable);
    return pddlGroundAtomCmp(a1, a2) == 0;
}

static uint64_t pddlGroundAtomHash(const pddl_ground_atom_t *a)
{
    uint64_t hash;

    hash = borCityHash_32(&a->pred, sizeof(int));
    hash <<= 32u;
    hash |= 0xffffffffu \
                & borCityHash_32(a->arg, sizeof(pddl_obj_id_t) * a->arg_size);
    return hash;
}

void pddlGroundAtomDel(pddl_ground_atom_t *a)
{
    if (a->arg != NULL)
        BOR_FREE(a->arg);
    BOR_FREE(a);
}


pddl_ground_atom_t *pddlGroundAtomClone(const pddl_ground_atom_t *a)
{
    pddl_ground_atom_t *c = BOR_ALLOC(pddl_ground_atom_t);
    *c = *a;
    if (a->arg != NULL){
        c->arg = BOR_ALLOC_ARR(pddl_obj_id_t, c->arg_size);
        memcpy(c->arg, a->arg, sizeof(pddl_obj_id_t) * c->arg_size);
    }
    return c;
}

static int pddlGroundAtomCmp(const pddl_ground_atom_t *a1,
                             const pddl_ground_atom_t *a2)
{
    if (a1->pred != a2->pred)
        return a1->pred - a2->pred;
    ASSERT(a1->arg_size == a2->arg_size);
    return memcmp(a1->arg, a2->arg, sizeof(pddl_obj_id_t) * a1->arg_size);
}

static pddl_ground_atom_t *nextNewGroundAtom(pddl_ground_atoms_t *ga,
                                             const pddl_ground_atom_t *a)
{
    pddl_ground_atom_t *g;

    if (ga->atom_size >= ga->atom_alloc){
        if (ga->atom_alloc == 0){
            ga->atom_alloc = 2;
        }else{
            ga->atom_alloc *= 2;
        }
        ga->atom = BOR_REALLOC_ARR(ga->atom,
                                   pddl_ground_atom_t *, ga->atom_alloc);
    }

    g = pddlGroundAtomClone(a);
    g->id = ga->atom_size;
    ga->atom[ga->atom_size++] = g;
    return g;
}

void pddlGroundAtomsInit(pddl_ground_atoms_t *ga)
{
    bzero(ga, sizeof(*ga));
    ga->htable = borHTableNew(htableKey, htableEq, ga);
}

void pddlGroundAtomsFree(pddl_ground_atoms_t *ga)
{
    if (ga->htable != NULL)
        borHTableDel(ga->htable);
    for (int i = 0; i < ga->atom_size; ++i){
        if (ga->atom[i] != NULL)
            pddlGroundAtomDel(ga->atom[i]);
    }
    if (ga->atom != NULL)
        BOR_FREE(ga->atom);
}

static void groundAtom(pddl_ground_atom_t *a,
                       const pddl_cond_atom_t *c, const pddl_obj_id_t *arg)
{
    a->func_val = 0;
    a->pred = c->pred;
    a->arg_size = c->arg_size;
    for (int i = 0; i < c->arg_size; ++i){
        if (c->arg[i].obj >= 0){
            a->arg[i] = c->arg[i].obj;
        }else{
            ASSERT(arg != NULL);
            a->arg[i] = arg[c->arg[i].param];
        }
    }
}

pddl_ground_atom_t *pddlGroundAtomsAddAtom(pddl_ground_atoms_t *ga,
                                           const pddl_cond_atom_t *c,
                                           const pddl_obj_id_t *arg)
{
    bor_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, c->arg_size);

    groundAtom(&loc, c, arg);
    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = borHTableFind(ga->htable, &loc.htable)) != NULL){
        out = BOR_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }

    out = nextNewGroundAtom(ga, &loc);
    borListInit(&out->htable);
    borHTableInsert(ga->htable, &out->htable);
    return out;
}

pddl_ground_atom_t *pddlGroundAtomsAddPred(pddl_ground_atoms_t *ga,
                                           int pred,
                                           const pddl_obj_id_t *arg,
                                           int arg_size)
{
    bor_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, arg_size);

    loc.func_val = 0;
    loc.pred = pred;
    memcpy(loc.arg, arg, sizeof(pddl_obj_id_t) * arg_size);
    loc.arg_size = arg_size;

    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = borHTableFind(ga->htable, &loc.htable)) != NULL){
        out = BOR_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }

    out = nextNewGroundAtom(ga, &loc);
    borListInit(&out->htable);
    borHTableInsert(ga->htable, &out->htable);
    return out;
}


pddl_ground_atom_t *pddlGroundAtomsFindAtom(const pddl_ground_atoms_t *ga,
                                            const pddl_cond_atom_t *c,
                                            const pddl_obj_id_t *arg)
{
    bor_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, c->arg_size);

    groundAtom(&loc, c, arg);
    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = borHTableFind(ga->htable, &loc.htable)) != NULL){
        out = BOR_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }
    return NULL;
}
