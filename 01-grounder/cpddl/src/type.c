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

#include <boruvka/alloc.h>
#include "pddl/pddl.h"
#include "pddl/type.h"
#include "lisp_err.h"
#include "assert.h"

static const char *object_name = "object";

static void pddlTypeInit(pddl_type_t *t)
{
    bzero(t, sizeof(*t));
}

static void pddlTypeFree(pddl_type_t *t)
{
    if (t->name != NULL)
        BOR_FREE(t->name);
    borISetFree(&t->child);
    borISetFree(&t->either);
    if (t->obj.obj != NULL)
        BOR_FREE(t->obj.obj);
}

static void pddlTypeInitCopy(pddl_type_t *dst, const pddl_type_t *src)
{
    if (src->name != NULL)
        dst->name = BOR_STRDUP(src->name);
    dst->parent = src->parent;
    borISetUnion(&dst->child, &src->child);
    borISetUnion(&dst->either, &src->either);
    dst->obj.obj_size = src->obj.obj_size;
    dst->obj.obj_alloc = src->obj.obj_alloc;
    dst->obj.obj = BOR_ALLOC_ARR(pddl_obj_id_t, dst->obj.obj_alloc);
    memcpy(dst->obj.obj, src->obj.obj,
           sizeof(pddl_obj_id_t) * dst->obj.obj_size);
}

int pddlTypesGet(const pddl_types_t *t, const char *name)
{
    for (int i = 0; i < t->type_size; ++i){
        if (strcmp(t->type[i].name, name) == 0)
            return i;
    }

    return -1;
}


int pddlTypesAdd(pddl_types_t *t, const char *name, int parent)
{
    int id;

    if ((id = pddlTypesGet(t, name)) != -1)
        return id;

    if (t->type_size >= t->type_alloc){
        if (t->type_alloc == 0)
            t->type_alloc = 2;
        t->type_alloc *= 2;
        t->type = BOR_REALLOC_ARR(t->type, pddl_type_t, t->type_alloc);
    }

    id = t->type_size++;
    pddl_type_t *type = t->type + id;
    pddlTypeInit(type);
    if (name != NULL)
        type->name = BOR_STRDUP(name);
    type->parent = parent;
    if (parent >= 0)
        borISetAdd(&t->type[parent].child, id);
    return id;
}

static int setCB(const pddl_lisp_node_t *root,
                 int child_from, int child_to, int child_type, void *ud,
                 bor_err_t *err)
{
    pddl_types_t *t = ud;
    int pid;

    pid = 0;
    if (child_type >= 0){
        if (root->child[child_type].value == NULL){
            ERR_LISP_RET2(err, -1, root->child + child_type,
                          "Invalid typed list. Unexpected expression");
        }
        pid = pddlTypesAdd(t, root->child[child_type].value, 0);
    }

    for (int i = child_from; i < child_to; ++i){
        // This is checked in pddlLispParseTypedList()
        ASSERT(root->child[i].value != NULL);
        if (root->child[i].value == NULL)
            ERR_LISP_RET2(err, -1, root->child + i, "Unexpected expression");

        pddlTypesAdd(t, root->child[i].value, pid);
    }

    return 0;
}

int pddlTypesParse(pddl_t *pddl, bor_err_t *e)
{
    pddl_types_t *types;
    const pddl_lisp_node_t *n;

    // Create a default "object" type
    types = &pddl->type;
    pddlTypesAdd(types, object_name, -1);

    n = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_TYPES);
    if (n != NULL){
        if (pddlLispParseTypedList(n, 1, n->child_size, setCB, types, e) != 0){
            BOR_TRACE_PREPEND_RET(e, -1, "Invalid definition of :types in %s: ",
                                  pddl->domain_lisp->filename);
        }
    }

    // TODO: Check circular dependency on types
    return 0;
}

void pddlTypesInitCopy(pddl_types_t *dst, const pddl_types_t *src)
{
    bzero(dst, sizeof(*dst));
    dst->type_size = src->type_size;
    dst->type_alloc = src->type_alloc;
    dst->type = BOR_CALLOC_ARR(pddl_type_t, dst->type_alloc);
    for (int i = 0; i < dst->type_size; ++i)
        pddlTypeInitCopy(dst->type + i, src->type + i);

    if (src->obj_type_map != NULL){
        dst->obj_type_map_memsize = src->obj_type_map_memsize;
        dst->obj_type_map = BOR_ALLOC_ARR(char, dst->obj_type_map_memsize);
        memcpy(dst->obj_type_map, src->obj_type_map, dst->obj_type_map_memsize);
    }
}

void pddlTypesFree(pddl_types_t *types)
{
    for (int i = 0; i < types->type_size; ++i)
        pddlTypeFree(types->type + i);
    if (types->type != NULL)
        BOR_FREE(types->type);

    if (types->obj_type_map != NULL)
        BOR_FREE(types->obj_type_map);
}

void pddlTypesPrint(const pddl_types_t *t, FILE *fout)
{
    fprintf(fout, "Type[%d]:\n", t->type_size);
    for (int i = 0; i < t->type_size; ++i){
        fprintf(fout, "    [%d]: %s, parent: %d", i,
                t->type[i].name, t->type[i].parent);
        fprintf(fout, "\n");
    }

    fprintf(fout, "Obj-by-Type:\n");
    for (int i = 0; i < t->type_size; ++i){
        fprintf(fout, "    [%d]:", i);
        for (int j = 0; j < t->type[i].obj.obj_size; ++j)
            fprintf(fout, " %d", (int)t->type[i].obj.obj[j]);
        fprintf(fout, "\n");
    }
}

int pddlTypesIsEither(const pddl_types_t *ts, int tid)
{
    return borISetSize(&ts->type[tid].either) > 0;
}

void pddlTypesAddObj(pddl_types_t *ts, pddl_obj_id_t obj_id, int type_id)
{
    pddl_type_t *t = ts->type + type_id;
    pddl_objset_t *obj = &t->obj;

    for (int i = 0; i < obj->obj_size; ++i){
        if (obj->obj[i] == obj_id)
            return;
    }

    if (obj->obj_size >= obj->obj_alloc){
        if (obj->obj_alloc == 0)
            obj->obj_alloc = 2;
        obj->obj_alloc *= 2;
        obj->obj = BOR_REALLOC_ARR(obj->obj, pddl_obj_id_t, obj->obj_alloc);
    }

    obj->obj[obj->obj_size++] = obj_id;

    if (t->parent != -1)
        pddlTypesAddObj(ts, obj_id, t->parent);
}

void pddlTypesBuildObjTypeMap(pddl_types_t *ts, int obj_size)
{
    if (ts->obj_type_map != NULL)
        BOR_FREE(ts->obj_type_map);
    ts->obj_type_map = BOR_CALLOC_ARR(char, obj_size * ts->type_size);
    ts->obj_type_map_memsize = obj_size * ts->type_size;
    for (int type_id = 0; type_id < ts->type_size; ++type_id){
        const pddl_objset_t *tobj = &ts->type[type_id].obj;
        for (int i = 0; i < tobj->obj_size; ++i){
            int obj = tobj->obj[i];
            ts->obj_type_map[obj * ts->type_size + type_id] = 1;
        }
    }
}

const pddl_obj_id_t *pddlTypesObjsByType(const pddl_types_t *ts, int type_id,
                                         int *size)
{
    const pddl_type_t *t = ts->type + type_id;
    if (size != NULL)
        *size = t->obj.obj_size;
    return t->obj.obj;
}

int pddlTypeNumObjs(const pddl_types_t *ts, int type_id)
{
    return ts->type[type_id].obj.obj_size;
}

int pddlTypeGetObj(const pddl_types_t *ts, int type_id, int idx)
{
    return ts->type[type_id].obj.obj[idx];
}

int pddlTypesObjHasType(const pddl_types_t *ts, int type, pddl_obj_id_t obj)
{
    if (ts->obj_type_map != NULL){
        return ts->obj_type_map[obj * ts->type_size + type];

    }else{
        const pddl_obj_id_t *objs;
        int size;

        objs = pddlTypesObjsByType(ts, type, &size);
        for (int i = 0; i < size; ++i){
            if (objs[i] == obj)
                return 1;
        }
        return 0;
    }
}


static int pddlTypesEither(pddl_types_t *ts, const bor_iset_t *either)
{
    int tid;

    // Try to find already created (either ...) type
    for (int i = 0; i < ts->type_size; ++i){
        if (pddlTypesIsEither(ts, i)
                && borISetEq(&ts->type[i].either, either)){
            return i;
        }
    }

    // Construct a name of the (either ...) type
    char *name, *cur;
    int eid;
    int slen = 0;
    BOR_ISET_FOR_EACH(either, eid)
        slen += 1 + strlen(ts->type[eid].name);
    slen += 2 + 6 + 1;
    name = cur = BOR_ALLOC_ARR(char, slen);
    cur += sprintf(cur, "(either");
    BOR_ISET_FOR_EACH(either, eid)
        cur += sprintf(cur, " %s", ts->type[eid].name);
    sprintf(cur, ")");

    tid = pddlTypesAdd(ts, name, -1);
    if (name != NULL)
        BOR_FREE(name);
    pddl_type_t *type = ts->type + tid;
    borISetUnion(&type->child, either);
    borISetUnion(&type->either, either);

    // Merge obj IDs from all simple types from which this (either ...)
    // type consists of.
    BOR_ISET_FOR_EACH(either, eid){
        const pddl_type_t *et = ts->type + eid;
        for (int j = 0; j < et->obj.obj_size; ++j){
            pddlTypesAddObj(ts, et->obj.obj[j], tid);
        }
    }

    return tid;
}


int pddlTypeFromLispNode(pddl_types_t *ts, const pddl_lisp_node_t *node,
                         bor_err_t *err)
{
    int tid;

    if (node->value != NULL){
        tid = pddlTypesGet(ts, node->value);
        if (tid < 0)
            ERR_LISP_RET(err, -1, node, "Unkown type `%s'", node->value);
        return tid;
    }

    if (node->child_size < 2 || node->child[0].kw != PDDL_KW_EITHER)
        ERR_LISP_RET2(err, -1, node, "Unknown expression");

    if (node->child_size == 2 && node->child[1].value != NULL)
        return pddlTypeFromLispNode(ts, node->child + 1, err);

    BOR_ISET(either);
    for (int i = 1; i < node->child_size; ++i){
        if (node->child[i].value == NULL){
            ERR_LISP_RET2(err, -1, node->child + i,
                          "Invalid (either ...) expression");
        }
        tid = pddlTypesGet(ts, node->child[i].value);
        if (tid < 0){
            ERR_LISP_RET(err, -1, node->child + i, "Unkown type `%s'",
                         node->child[i].value);
        }

        borISetAdd(&either, tid);
    }

    tid = pddlTypesEither(ts, &either);
    borISetFree(&either);
    return tid;
}

int pddlTypesIsParent(const pddl_types_t *ts, int child, int parent)
{
    const pddl_type_t *tparent = ts->type + parent;
    int eid;

    for (int cur_type = child; cur_type >= 0;){
        if (cur_type == parent)
            return 1;
        BOR_ISET_FOR_EACH(&tparent->either, eid){
            if (cur_type == eid)
                return 1;
        }
        cur_type = ts->type[cur_type].parent;
    }

    return 0;
}

int pddlTypesAreDisjunct(const pddl_types_t *ts, int t1, int t2)
{
    return !pddlTypesIsParent(ts, t1, t2) && !pddlTypesIsParent(ts, t2, t1);
}

void pddlTypesPrintPDDL(const pddl_types_t *ts, FILE *fout)
{
    int q[ts->type_size];
    int qi = 0, qsize = 0;

    fprintf(fout, "(:types\n");
    for (int i = 0; i < ts->type_size; ++i){
        if (ts->type[i].parent == 0)
            q[qsize++] = i;
    }

    for (qi = 0; qi < qsize; ++qi){
        fprintf(fout, "    %s - %s\n",
                ts->type[q[qi]].name,
                ts->type[ts->type[q[qi]].parent].name);
        for (int i = 0; i < ts->type_size; ++i){
            if (ts->type[i].parent == q[qi] && !pddlTypesIsEither(ts, i))
                q[qsize++] = i;
        }
    }

    fprintf(fout, ")\n");
}
