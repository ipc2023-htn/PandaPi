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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "pddl/pddl_file.h"

#define MAX_LEN 512

static int isDir(const char *d)
{
    struct stat st;
    if (stat(d, &st) == -1)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 1;
    return 0;
}

static int isFile(const char *d)
{
    struct stat st;
    if (stat(d, &st) == -1)
        return 0;

    if (S_ISREG(st.st_mode))
        return 1;
    return 0;
}

static void extractDir(const char *path, char *dir)
{
    int len = strlen(path);
    strcpy(dir, path);
    int idx;
    for (idx = len - 1; idx >= 0 && dir[idx] != '/'; --idx);
    if (idx >= 0){
        dir[idx + 1] = 0x0;
    }else{
        strcpy(dir, "./");
    }
}

static void extractProblemName(const char *prob, char *name)
{
    int idx;
    for (idx = strlen(prob) - 1; idx >= 0 && prob[idx] != '/'; --idx);
    strcpy(name, prob + idx + 1);
    int len = strlen(name);
    if (strcmp(name + len - 5, ".pddl") == 0)
        name[len - 5] = 0x0;
}

static int findDomainReplace(const char *prob_name,
                             const char *find,
                             const char *replace,
                             char *domain_pddl,
                             int dirlen)
{
    char *s;

    domain_pddl[dirlen] = 0x0;
    if ((s = strstr(prob_name, find)) != NULL){
        int len = dirlen;
        if (s != prob_name){
            strncpy(domain_pddl + dirlen, prob_name, s - prob_name);
            len += s - prob_name;
        }
        strcpy(domain_pddl + len, replace);

        s += strlen(find);
        if (*s != 0x0)
            sprintf(domain_pddl + strlen(domain_pddl), "%s.pddl", s);

        if (isFile(domain_pddl))
            return 0;
    }

    return -1;
}

static int findDomainToProblem(const char *prob, char *domain_pddl)
{
    extractDir(prob, domain_pddl);
    int dirlen = strlen(domain_pddl);
    char *s;

    char prob_name[PDDL_FILE_MAX_PATH_LEN];
    extractProblemName(prob, prob_name);

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "domain_%s.pddl", prob_name);
    if (isFile(domain_pddl))
        return 0;

    if (findDomainReplace(prob_name, "problem", "domain",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "satprob", "satdom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "satprob", "dom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "prob", "dom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "domain-%s.pddl", prob_name);
    if (isFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "%s-domain.pddl", prob_name);
    if (isFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    strcpy(domain_pddl + dirlen, "domain.pddl");
    if (isFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    s = prob_name;
    while (strlen(s) > 1 && (s = strstr(s + 1, "-")) != NULL){
        strncpy(domain_pddl + dirlen, prob_name, s - prob_name);
        strcpy(domain_pddl + dirlen + (s - prob_name), "-domain.pddl");
        if (isFile(domain_pddl))
            return 0;
    }

    return -1;
}

int pddlFiles1(pddl_files_t *files, const char *s, bor_err_t *err)
{
    if (isFile(s)){
        if (strlen(s) >= PDDL_FILE_MAX_PATH_LEN - 1){
            BOR_ERR_RET2(err, -1, "Path(s) too long.");
        }

        if (findDomainToProblem(s, files->domain_pddl) == 0){
            strcpy(files->problem_pddl, s);
            return 0;
        }else{
            BOR_ERR_RET2(err, -1, "Cannot find domain pddl file.");
        }

    }else{
        if (strlen(s) + 5 >= PDDL_FILE_MAX_PATH_LEN - 1){
            BOR_ERR_RET2(err, -1, "Path(s) too long.");
        }

        char prob_pddl[MAX_LEN];
        strcpy(prob_pddl, s);
        strcpy(prob_pddl + strlen(prob_pddl), ".pddl");
        if (isFile(prob_pddl)){
            return pddlFiles1(files, prob_pddl, err);

        }else{
            BOR_ERR_RET(err, -1, "Cannot find problem pddl file"
                                 " (tried %s or %s).",
                                 s, prob_pddl);
        }
    }
}

int pddlFiles(pddl_files_t *files, const char *s1, const char *s2,
              bor_err_t *err)
{
    bzero(files, sizeof(*files));

    if (s1 == NULL && s2 == NULL){
        BOR_ERR_RET2(err, -1, "Unspecified specifiers.");

    }else if (s1 == NULL && s2 != NULL){
        return pddlFiles1(files, s2, err);

    }else if (s1 != NULL && s2 == NULL){
        return pddlFiles1(files, s1, err);

    }else{
        if (isFile(s1) && isFile(s2)){
            if (strlen(s1) >= PDDL_FILE_MAX_PATH_LEN - 1
                    || strlen(s2) >= PDDL_FILE_MAX_PATH_LEN - 1){
                BOR_ERR_RET2(err, -1, "Path(s) too long.");
            }
            strcpy(files->domain_pddl, s1);
            strcpy(files->problem_pddl, s2);
            return 0;

        }else if (isDir(s1)){
            if (strlen(s1) + strlen(s2) >= PDDL_FILE_MAX_PATH_LEN - 1){
                BOR_ERR_RET2(err, -1, "Path(s) too long.");
            }

            char prob[PDDL_FILE_MAX_PATH_LEN];
            strcpy(prob, s1);
            if (s1[strlen(s1) - 1] != '/')
                strcpy(prob + strlen(prob), "/");
            strcpy(prob + strlen(prob), s2);
            return pddlFiles1(files, prob, err);

        }else{
            BOR_ERR_RET2(err, -1, "Cannot find pddl files.");
        }
    }
}
