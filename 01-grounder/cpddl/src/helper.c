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

#include "helper.h"

void pddlISetRemap(bor_iset_t *dst, const int *remap)
{
    BOR_ISET(tmp);
    borISetUnion(&tmp, dst);

    borISetEmpty(dst);
    int v;
    BOR_ISET_FOR_EACH(&tmp, v)
        borISetAdd(dst, remap[v]);
    borISetFree(&tmp);
}
