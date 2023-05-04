#include <stdio.h>
#include <pddl/pddl.h>
#include <opts.h>

struct options {
    int help;
    int not_force_adl;
    int compile_away_cond_eff;
    int compile_away_cond_eff_pddl;

    int lifted_mgroup_max_candidates;
    int lifted_mgroup_max_mgroups;
    int lifted_mgroup_fd;

    int no_ground_prune;
    int no_ground_prune_pre;
    int no_ground_prune_dead_end;

    int h2fw;
    int no_dead_end_op;
    int no_h2;
    int no_irr;

    int fdr_var_largest;
    int fdr_var_largest_multi;

    const char *fdr_out;
    const char *lifted_mgroup_out;
    const char *mgroup_out;
    const char *mgroup_pre_out;
} opt;

bor_err_t err = BOR_ERR_INIT;
pddl_files_t files;
pddl_config_t pddl_cfg = PDDL_CONFIG_INIT;
pddl_t pddl;
pddl_lifted_mgroups_infer_limits_t lifted_mgroups_limits
            = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
pddl_lifted_mgroups_t lifted_mgroups;
pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
pddl_strips_t strips;
pddl_mgroups_t mgroups;
pddl_mutex_pairs_t mutex;
unsigned fdr_var_flag = PDDL_FDR_VARS_ESSENTIAL_FIRST;

static FILE *openFile(const char *fn)
{
    if (fn == NULL
            || strcmp(fn, "-") == 0
            || strcmp(fn, "stdout") == 0)
        return stdout;
    if (strcmp(fn, "stderr") == 0)
        return stderr;
    FILE *fout = fopen(fn, "w");
    return fout;
}

static void closeFile(FILE *f)
{
    if (f != NULL && f != stdout && f != stderr)
        fclose(f);
}

static int readOpts(int *argc, char *argv[])
{
    bzero(&opt, sizeof(opt));
    opt.lifted_mgroup_max_candidates = 10000;
    opt.lifted_mgroup_max_mgroups = 10000;
    opt.fdr_out = "-";

    pddl_cfg.force_adl = 1;

    optsAddDesc("help", 'h', OPTS_NONE, &opt.help, NULL,
                "Print this help.");
    optsAddDesc("output", 'o', OPTS_STR, &opt.fdr_out, NULL,
                "Output filename (default: stdout)");

    optsAddDesc("no-adl", 0x0, OPTS_NONE, &opt.not_force_adl, NULL,
                "Do NOT force :adl requirement if it is not specified in the"
                " domain file.");
    optsAddDesc("ce", 0x0, OPTS_NONE, &opt.compile_away_cond_eff, NULL,
                "Compile away conditional effects on the STRIPS level"
                " (recommended instead of --ce-pddl).");
    optsAddDesc("ce-pddl", 0x0, OPTS_NONE, &opt.compile_away_cond_eff_pddl,
                NULL,
                "Compile away conditional effects on the PDDL level.");

    optsAddDesc("lmg-max-candidates", 0x0, OPTS_INT,
                &opt.lifted_mgroup_max_candidates, NULL,
                "Maximum number of lifted mutex group candidates."
                " (default: 10000)");
    optsAddDesc("lmg-max-mgroups", 0x0, OPTS_INT,
                &opt.lifted_mgroup_max_mgroups, NULL,
                "Maximum number of lifted mutex group. (default: 10000)");
    optsAddDesc("lmg-fd", 0x0, OPTS_NONE, &opt.lifted_mgroup_fd, NULL,
                "Find Fast-Downward type of lifted mutex groups.");
    optsAddDesc("lmg-out", 0x0, OPTS_STR, &opt.lifted_mgroup_out, NULL,
                "Output filename for infered lifted mutex groups."
                " (default: none)");

    optsAddDesc("no-ground-prune", 0x0, OPTS_NONE, &opt.no_ground_prune, NULL,
                "Do NOT use lifted mutex groups for pruning during grounding."
                " (default: off)");
    optsAddDesc("no-ground-prune-pre", 0x0, OPTS_NONE,
                &opt.no_ground_prune_pre, NULL,
                "Do NOT use lifted mutex groups for pruning during grounding by"
                " checking preconditions.");
    optsAddDesc("no-ground-prune-dead-end", 0x0, OPTS_NONE,
                &opt.no_ground_prune_dead_end, NULL,
                "Do NOT use lifted mutex groups for pruning of dead-end"
                " operators during grounding.");

    optsAddDesc("h2fw", 0x0, OPTS_NONE, &opt.h2fw, NULL,
                "Use only forward h^2 for pruning (instead of"
                " forward/backward).");
    optsAddDesc("no-dead-end-op", 0x0, OPTS_NONE, &opt.no_dead_end_op, NULL,
                "Do NOT use fam-groups for detecting dead-end operators.");
    optsAddDesc("no-h2", 0x0, OPTS_NONE, &opt.no_h2, NULL,
                "Do NOT use h^2 for pruning.");
    optsAddDesc("no-irrelevance", 0x0, OPTS_NONE, &opt.no_irr, NULL,
                "Do NOT use irrelevance analysis.");

    optsAddDesc("mg-out", 0x0, OPTS_STR, &opt.mgroup_out, NULL,
                "Output filename for found mutex groups (after pruning)."
                " (default: none)");
    optsAddDesc("mg-pre-out", 0x0, OPTS_STR, &opt.mgroup_pre_out, NULL,
                "Output filename for the mutex groups found before pruning."
                " (default: none)");

    optsAddDesc("fdr-var-largest", 0x0, OPTS_NONE, &opt.fdr_var_largest, NULL,
                "Allocate FDR variables with largest-first algorithm"
                " (instead of essential-first).");
    optsAddDesc("fdr-var-largest-multi", 0x0, OPTS_NONE,
                &opt.fdr_var_largest_multi, NULL,
                "Allocate FDR variables with largest-first algorithm and"
                " encode one strips fact as multiple fdr values if in more"
                " mutex groups.");

    if (opts(argc, argv) != 0 || opt.help || (*argc != 3 && *argc != 2)){
        if (*argc <= 1)
            fprintf(stderr, "Error: Missing input file(s)\n\n");

        if (*argc > 3){
            for (int i = 0; i < *argc; ++i){
                if (argv[i][0] == '-'){
                    fprintf(stderr, "Error: Unrecognized option '%s'\n",
                            argv[i]);
                }
            }
        }

        fprintf(stderr, "pddl-fdr is a program for translating PDDL into"
                        " FDR.\n");
        fprintf(stderr, "Usage: %s [OPTIONS] domain.pddl problem.pddl\n",
                argv[0]);
        fprintf(stderr, "  OPTIONS:\n");
        optsPrint(stderr, "    ");
        fprintf(stderr, "\n");
        return -1;
    }


    if (opt.no_ground_prune_pre && opt.no_ground_prune_dead_end)
        opt.no_ground_prune = 1;

    if (*argc == 2){
        BOR_INFO(&err, "Input file: '%s'", argv[1]);
        if (pddlFiles1(&files, argv[1], &err) != 0)
            BOR_TRACE_RET(&err, -1);
    }else{ // *argc == 3
        BOR_INFO(&err, "Input files: '%s' and '%s'", argv[1], argv[2]);
        if (pddlFiles(&files, argv[1], argv[2], &err) != 0)
            BOR_TRACE_RET(&err, -1);
    }

    return 0;
}

static int readPDDL(void)
{
    BOR_INFO2(&err, "Reading PDDL ...");
    BOR_INFO(&err, "PDDL option no-adl: %d", opt.not_force_adl);

    if (opt.not_force_adl)
        pddl_cfg.force_adl = 0;

    if (pddlInit(&pddl, files.domain_pddl, files.problem_pddl,
                 &pddl_cfg, &err) != 0){
        BOR_TRACE_RET(&err, -1);
    }

    pddlNormalize(&pddl);
    if (opt.compile_away_cond_eff_pddl)
        pddlCompileAwayCondEff(&pddl);
    pddlCheckSizeTypes(&pddl);

    BOR_INFO(&err, "Number of PDDL Types: %d", pddl.type.type_size);
    BOR_INFO(&err, "Number of PDDL Objects: %d", pddl.obj.obj_size);
    BOR_INFO(&err, "Number of PDDL Predicates: %d", pddl.pred.pred_size);
    BOR_INFO(&err, "Number of PDDL Functions: %d", pddl.func.pred_size);
    BOR_INFO(&err, "Number of PDDL Actions: %d", pddl.action.action_size);
    BOR_INFO(&err, "PDDL Metric: %d", pddl.metric);
    fflush(stdout);
    fflush(stderr);

    return 0;
}

static int liftedMGroups(void)
{
    BOR_INFO2(&err, "");
    BOR_INFO2(&err, "Inference of lifted mutex groups ...");
    BOR_INFO(&err, "Lifted mutex groups option lmg-fd: %d",
             opt.lifted_mgroup_fd);
    BOR_INFO(&err, "Lifted mutex groups option lmg-max-candidates: %d",
             opt.lifted_mgroup_max_candidates);
    BOR_INFO(&err, "Lifted mutex groups option lmg-max-mgroups: %d",
             opt.lifted_mgroup_max_mgroups);

    lifted_mgroups_limits.max_candidates = opt.lifted_mgroup_max_candidates;
    lifted_mgroups_limits.max_mgroups = opt.lifted_mgroup_max_mgroups;
    pddlLiftedMGroupsInit(&lifted_mgroups);
    if (opt.lifted_mgroup_fd){
        pddlLiftedMGroupsInferMonotonicity(&pddl, &lifted_mgroups_limits, NULL,
                                           &lifted_mgroups, &err);
    }else{
        pddlLiftedMGroupsInferFAMGroups(&pddl, &lifted_mgroups_limits,
                                        &lifted_mgroups, &err);
    }
    pddlLiftedMGroupsSetExactlyOne(&pddl, &lifted_mgroups, &err);
    pddlLiftedMGroupsSetStatic(&pddl, &lifted_mgroups, &err);

    if (opt.lifted_mgroup_out != NULL){
        FILE *fout = openFile(opt.lifted_mgroup_out);
        if (fout == NULL){
            fprintf(stderr, "Error: Could not open '%s'\n",
                    opt.lifted_mgroup_out);
            return -1;
        }
        BOR_INFO(&err, "Printing lifted mutex groups to '%s'",
                 opt.lifted_mgroup_out);
        pddlLiftedMGroupsPrint(&pddl, &lifted_mgroups, fout);
        closeFile(fout);
    }

    return 0;
}


static int groundStrips(void)
{
    BOR_INFO2(&err, "");
    BOR_INFO2(&err, "Grounding of STRIPS ...");
    BOR_INFO(&err, "Grounding of STRIPS option no-ground-prune: %d",
             opt.no_ground_prune);
    BOR_INFO(&err, "Grounding of STRIPS option no-ground-prune-pre: %d",
             opt.no_ground_prune_pre);
    BOR_INFO(&err, "Grounding of STRIPS option no-ground-prune-dead-end: %d",
             opt.no_ground_prune_dead_end);

    ground_cfg.lifted_mgroups = &lifted_mgroups;
    ground_cfg.prune_op_pre_mutex = 1;
    ground_cfg.prune_op_dead_end = 1;
    if (opt.no_ground_prune)
        ground_cfg.lifted_mgroups = NULL;
    if (opt.no_ground_prune_pre)
        ground_cfg.prune_op_pre_mutex = 0;
    if (opt.no_ground_prune_dead_end)
        ground_cfg.prune_op_dead_end = 0;

    if (pddlStripsGround(&strips, &pddl, &ground_cfg, &err) != 0){
        BOR_INFO2(&err, "Grounding failed.");
        BOR_TRACE_RET(&err, -1);
    }

    if (opt.compile_away_cond_eff)
        pddlStripsCompileAwayCondEff(&strips);

    BOR_INFO(&err, "Number of Strips Operators: %d", strips.op.op_size);
    BOR_INFO(&err, "Number of Strips Facts: %d", strips.fact.fact_size);

    int count = 0;
    for (int i = 0; i < strips.op.op_size; ++i){
        if (strips.op.op[i]->cond_eff_size > 0)
            ++count;
    }
    BOR_INFO(&err, "Number of Strips Operators"
            " with Conditional Effects: %d", count);
    BOR_INFO(&err, "Goal is unreachable: %d",
            strips.goal_is_unreachable);
    BOR_INFO(&err, "Has Conditional Effects: %d", strips.has_cond_eff);
    fflush(stdout);
    fflush(stderr);

    return 0;
}

static int groundMGroups(void)
{
    BOR_INFO2(&err, "");
    BOR_INFO2(&err, "Grounding mutex groups ...");

    pddlMGroupsGround(&mgroups, &pddl, &lifted_mgroups, &strips);
    pddlMGroupsSetExactlyOne(&mgroups, &strips);
    pddlMGroupsSetGoal(&mgroups, &strips);
    BOR_INFO(&err, "Found %d mutex groups", mgroups.mgroup_size);

    if (opt.mgroup_pre_out != NULL){
        FILE *fout = openFile(opt.mgroup_pre_out);
        if (fout == NULL){
            fprintf(stderr, "Error: Could not open '%s'\n", opt.mgroup_pre_out);
            return -1;
        }
        BOR_INFO(&err, "Printing mutex groups to '%s'", opt.mgroup_pre_out);
        pddlMGroupsPrint(&pddl, &strips, &mgroups, fout);
        closeFile(fout);
    }

    return 0;
}

static int pruneStrips(void)
{
    BOR_INFO2(&err, "");
    BOR_INFO2(&err, "Pruning of STRIPS ...");

    BOR_ISET(rm_fact);
    BOR_ISET(rm_op);

    pddlMutexPairsInitStrips(&mutex, &strips);
    pddlMutexPairsAddMGroups(&mutex, &mgroups);
    if (opt.no_dead_end_op){
        BOR_INFO2(&err, "Pruning dead-end operators disabled");

    }else{
        BOR_INFO2(&err, "Pruning dead-end operators ...");
        pddlFAMGroupsDeadEndOps(&mgroups, &strips, &rm_op);
        BOR_INFO(&err, "Pruning dead-end operators done. Dead end ops: %d",
                 borISetSize(&rm_op));
    }

    if (opt.no_h2){
        BOR_INFO2(&err, "h^2 disabled");

    }else if (strips.has_cond_eff){
        BOR_INFO2(&err, "h^2 disabled because the problem has conditional"
                        " effects.");

    }else{
        if (opt.h2fw){
            if (pddlH2(&strips, &mutex, &rm_fact, &rm_op, &err) != 0){
                BOR_INFO2(&err, "h^2 fw failed.");
                BOR_TRACE_RET(&err, -1);
            }
        }else if (!opt.no_h2){
            if (pddlH2FwBw(&strips, &mgroups, &mutex,
                        &rm_fact, &rm_op, &err) != 0){
                BOR_INFO2(&err, "h^2 fw/bw failed.");
                BOR_TRACE_RET(&err, -1);
            }
        }
    }

    if (strips.has_cond_eff){
        BOR_INFO2(&err, "irrelevance analysis disabled because the problem"
                        " has conditional effects.");

    }else if (!opt.no_irr){
        BOR_ISET(irr_fact);
        BOR_ISET(irr_op);
        BOR_ISET(static_fact);
        if (pddlIrrelevanceAnalysis(&strips, &irr_fact, &irr_op,
                                    &static_fact, &err) != 0){
            BOR_TRACE_RET(&err, -1);
        }
        borISetUnion(&rm_fact, &irr_fact);
        borISetUnion(&rm_op, &irr_op);

        borISetFree(&irr_fact);
        borISetFree(&irr_op);
        borISetFree(&static_fact);
    }

    if (borISetSize(&rm_fact) > 0 || borISetSize(&rm_op) > 0){
        pddlStripsReduce(&strips, &rm_fact, &rm_op);
        if (borISetSize(&rm_fact) > 0){
            pddlMutexPairsReduce(&mutex, &rm_fact);

            pddlMGroupsReduce(&mgroups, &rm_fact);
            pddlMGroupsSetExactlyOne(&mgroups, &strips);
            pddlMGroupsSetGoal(&mgroups, &strips);
        }
    }

    borISetFree(&rm_fact);
    borISetFree(&rm_op);

    BOR_INFO(&err, "Number of Strips Operators: %d", strips.op.op_size);
    BOR_INFO(&err, "Number of Strips Facts: %d", strips.fact.fact_size);

    int count = 0;
    for (int i = 0; i < strips.op.op_size; ++i){
        if (strips.op.op[i]->cond_eff_size > 0)
            ++count;
    }
    BOR_INFO(&err, "Number of Strips Operators with Conditional Effects: %d",
             count);
    BOR_INFO(&err, "Goal is unreachable: %d", strips.goal_is_unreachable);
    BOR_INFO(&err, "Has Conditional Effects: %d", strips.has_cond_eff);
    BOR_INFO(&err, "Mutex pairs after reduction: %d", mutex.num_mutex_pairs);
    BOR_INFO(&err, "Mutex groups after reduction: %d", mgroups.mgroup_size);
    fflush(stdout);
    fflush(stderr);

    if (opt.mgroup_out != NULL){
        FILE *fout = openFile(opt.mgroup_out);
        if (fout == NULL){
            fprintf(stderr, "Error: Could not open '%s'\n", opt.mgroup_out);
            return -1;
        }
        BOR_INFO(&err, "Printing mutex groups to '%s'", opt.mgroup_out);
        pddlMGroupsPrint(&pddl, &strips, &mgroups, fout);
        closeFile(fout);
    }

    return 0;
}

static int toFDR(void)
{
    BOR_INFO2(&err, "");
    BOR_INFO2(&err, "Translating to FDR ...");
    BOR_INFO(&err, "Output file: '%s'", opt.fdr_out);

    fdr_var_flag = PDDL_FDR_VARS_ESSENTIAL_FIRST;
    if (opt.fdr_var_largest)
        fdr_var_flag = PDDL_FDR_VARS_LARGEST_FIRST;
    if (opt.fdr_var_largest_multi)
        fdr_var_flag = PDDL_FDR_VARS_LARGEST_FIRST_MULTI;
    FILE *fout = openFile(opt.fdr_out);
    if (fout == NULL){
        fprintf(stderr, "Error: Could not open file '%s'\n", opt.fdr_out);
        return -1;
    }
    pddlFDRPrintAsFD(&strips, &mgroups, &mutex, fdr_var_flag, fout, &err);
    closeFile(fout);

    return 0;
}

int main(int argc, char *argv[])
{
    borErrWarnEnable(&err, stderr);
    borErrInfoEnable(&err, stderr);

    if (readOpts(&argc, argv) != 0
            || readPDDL() != 0
            || liftedMGroups() != 0
            || groundStrips() != 0
            || groundMGroups() != 0
            || pruneStrips() != 0
            || toFDR() != 0){
        if (borErrIsSet(&err)){
            fprintf(stderr, "Error: ");
            borErrPrint(&err, 1, stderr);
        }
        return -1;
    }

    optsClear();
    pddlMutexPairsFree(&mutex);
    pddlMGroupsFree(&mgroups);
    pddlStripsFree(&strips);
    pddlLiftedMGroupsFree(&lifted_mgroups);
    pddlFree(&pddl);
    return 0;
}



