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

#ifndef OPTARG_NAME
# error OPTARG_NAME must be defined
#endif

#ifndef OPTARG_TYPE
# error OPTARG_TYPE must be defined
#endif

#ifndef OPTARG_BASETYPE
# error OPTARG_BASETYPE must be defined
#endif

#ifndef OPTARG_PARSEFUNC
# error OPTARG_PARSEFUNC must be defined
#endif


static int OPTARG_NAME(opt_t *opt, const char *arg, int len)
{
    void (*cb)(const char *, char, OPTARG_TYPE);
    void (*cbarr)(const char *, char, OPTARG_TYPE *);
    OPTARG_BASETYPE val;
    int i, ilen;
    char *next;

    ilen = (len == 0 ? 1 : len);

    next = (char *)arg;
    for (i = 0; i < ilen && *next != 0x0; i++){
        if (OPTARG_PARSEFUNC(arg, strelend(arg), &val, &next) == 0){
            if (opt->set){
                ((OPTARG_TYPE *)opt->set)[i] = val;
            }

            if (*next != 0x0)
                arg = ++next;
        }else{
            invalidOptErr(opt);
            return -1;
        }
    }

    if (*next != 0x0 || i != ilen){
        invalidOptErr(opt);
        return -1;
    }

    if (opt->callback){
        if (len > 0){
            cbarr = (void (*)(const char *, char, OPTARG_TYPE *))opt->callback;
            cbarr(opt->long_name, opt->short_name, (OPTARG_TYPE *)opt->set);
        }else{
            cb = (void (*)(const char *, char, OPTARG_TYPE))opt->callback;
            cb(opt->long_name, opt->short_name, *(OPTARG_TYPE *)opt->set);
        }
    }

    return 0;
}

#undef OPTARG_NAME
#undef OPTARG_TYPE
#undef OPTARG_BASETYPE
#undef OPTARG_PARSEFUNC
