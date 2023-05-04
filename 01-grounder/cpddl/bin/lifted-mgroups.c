#include <stdio.h>
#include <pddl/pddl.h>
#include <opts.h>

struct options {
    int help;
    int quiet;
    int force_adl;
    int compile_away_cond_eff;
    int compile_away_cond_eff_pddl;
    int max_candidates;
    int max_mgroups;
    int ground;
    int fd;
    int fd_monotonicity;
    int ground_prune;
    int ground_prune_pre;
    int ground_prune_dead_end;
} o;

int main(int argc, char *argv[])
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    bor_err_t err = BOR_ERR_INIT;
    pddl_strips_t strips;
    pddl_lifted_mgroups_t lifted_mgroups;
    pddl_lifted_mgroups_t monotonicity_invariants;
    pddl_files_t files;

    bzero(&o, sizeof(o));
    o.max_candidates = 10000;
    o.max_mgroups = 10000;

    optsAddDesc("help", 'h', OPTS_NONE, &o.help, NULL,
                "Print this help.");
    optsAddDesc("quiet", 'q', OPTS_NONE, &o.quiet, NULL,
                "Disable logging and error output.");
    optsAddDesc("adl", 'a', OPTS_NONE, &o.force_adl, NULL,
                "Force :adl requirement even if it is not specified in the"
                " domain file. (default: off)");
    optsAddDesc("ce", 0x0, OPTS_NONE, &o.compile_away_cond_eff, NULL,
                "Compile away conditional effects on the STRIPS level"
                " (recommended). (default: off)");
    optsAddDesc("ce-pddl", 0x0, OPTS_NONE, &o.compile_away_cond_eff_pddl, NULL,
                "Compile away conditional effects on the PDDL level."
                " (default: off)");
    optsAddDesc("max-candidates", 0x0, OPTS_INT, &o.max_candidates, NULL,
                "Maximum number of mutex group candidates. (default: 10000)");
    optsAddDesc("max-mgroups", 0x0, OPTS_INT, &o.max_mgroups, NULL,
                "Maximum number of mutex group. (default: 10000)");
    optsAddDesc("ground", 'g', OPTS_NONE, &o.ground, NULL,
                "Ground lifted mutex groups. (default: off)");
    optsAddDesc("fd", 0x0, OPTS_NONE, &o.fd, NULL,
                "Find Fast-Downward type of invariants. (default: off)");
    optsAddDesc("fd-mono", 0x0, OPTS_NONE, &o.fd_monotonicity, NULL,
                "Find Fast-Downward monotonicit invariants -- implies --fd."
                " (default: off)");
    optsAddDesc("prune", 0x0, OPTS_NONE, &o.ground_prune, NULL,
                "Use lifted mutex groups for pruning during grounding.  This"
                " takes effect only if -g is specified. (default: off)");
    optsAddDesc("prune-pre", 0x0, OPTS_NONE, &o.ground_prune_pre, NULL,
                "Use lifted mutex groups for pruning during grounding by"
                " checking preconditions.  This takes effect only if -g is"
                " specified. (default: off)");

    if (opts(&argc, argv) || o.help || (argc != 3 && argc != 2)){
        fprintf(stderr, "Usage: %s [OPTIONS] domain.pddl problem.pddl\n",
                argv[0]);
        fprintf(stderr, "  OPTIONS:\n");
        optsPrint(stderr, "    ");
        fprintf(stderr, "\n");
        return -1;
    }

    if (o.quiet){
        borErrWarnEnable(&err, NULL);
        borErrInfoEnable(&err, NULL);
    }else{
        borErrWarnEnable(&err, stderr);
        borErrInfoEnable(&err, stderr);
    }

    cfg.force_adl = 0;
    if (o.force_adl)
        cfg.force_adl = 1;

    if (o.fd_monotonicity)
        o.fd = 1;

    if (o.ground_prune){
        o.ground_prune_pre = 1;
        o.ground_prune_dead_end = 1;
    }

    if (argc == 2){
        if (pddlFiles1(&files, argv[1], &err) != 0){
            if (!o.quiet){
                fprintf(stderr, "Error: ");
                borErrPrint(&err, 1, stderr);
            }
            return -1;
        }
    }else{ // argc == 3
        if (pddlFiles(&files, argv[1], argv[2], &err) != 0){
            if (!o.quiet){
                fprintf(stderr, "Error: ");
                borErrPrint(&err, 1, stderr);
            }
            return -1;
        }
    }

    if (pddlInit(&pddl, files.domain_pddl, files.problem_pddl,
                 &cfg, &err) != 0){
        if (!o.quiet){
            fprintf(stderr, "Error: ");
            borErrPrint(&err, 1, stderr);
        }
        return -1;
    }

    pddlNormalize(&pddl);
    if (o.compile_away_cond_eff_pddl)
        pddlCompileAwayCondEff(&pddl);

    if (!o.quiet){
        BOR_INFO(&err, "Number of PDDL Types: %d", pddl.type.type_size);
        BOR_INFO(&err, "Number of PDDL Objects: %d", pddl.obj.obj_size);
        BOR_INFO(&err, "Number of PDDL Predicates: %d", pddl.pred.pred_size);
        BOR_INFO(&err, "Number of PDDL Functions: %d", pddl.func.pred_size);
        BOR_INFO(&err, "Number of PDDL Actions: %d", pddl.action.action_size);
        BOR_INFO(&err, "Number of PDDL Metric: %d", pddl.metric);
        fflush(stdout);
        fflush(stderr);
    }

    pddl_lifted_mgroups_infer_limits_t limits
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    limits.max_candidates = o.max_candidates;
    limits.max_mgroups = o.max_mgroups;

    pddlLiftedMGroupsInit(&lifted_mgroups);
    pddlLiftedMGroupsInit(&monotonicity_invariants);
    if (o.fd){
        pddl_lifted_mgroups_t *mono = NULL;
        if (o.fd_monotonicity)
            mono = &monotonicity_invariants;
        pddlLiftedMGroupsInferMonotonicity(&pddl, &limits, mono,
                                           &lifted_mgroups, &err);
    }else{
        pddlLiftedMGroupsInferFAMGroups(&pddl, &limits, &lifted_mgroups, &err);
    }

    for (int li = 0; li < lifted_mgroups.mgroup_size; ++li){
        fprintf(stdout, "M:%d: ", li);
        pddlLiftedMGroupPrint(&pddl, lifted_mgroups.mgroup + li, stdout);
    }

    for (int li = 0; li < monotonicity_invariants.mgroup_size; ++li){
        const pddl_lifted_mgroup_t *m = monotonicity_invariants.mgroup + li;
        fprintf(stdout, "I:%d: ", li);
        pddlLiftedMGroupPrint(&pddl, m, stdout);
    }

    if (o.ground){
        pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
        if (o.ground_prune_pre || o.ground_prune_dead_end){
            ground_cfg.lifted_mgroups = &lifted_mgroups;
            ground_cfg.prune_op_pre_mutex = o.ground_prune_pre;
            ground_cfg.prune_op_dead_end = o.ground_prune_dead_end;
        }
        if (pddlStripsGround(&strips, &pddl, &ground_cfg, &err) != 0){
            if (!o.quiet){
                BOR_INFO2(&err, "Grounding failed.");
                fprintf(stderr, "Error: ");
                borErrPrint(&err, 1, stderr);
            }
            return -1;
        }

        if (o.compile_away_cond_eff)
            pddlStripsCompileAwayCondEff(&strips);

        if (!o.quiet){
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
        }

        pddl_mgroups_t mgroups;
        pddlMGroupsGround(&mgroups, &pddl, &lifted_mgroups, &strips);
        for (int gi = 0; gi < mgroups.mgroup_size; ++gi){
            const pddl_mgroup_t *m = mgroups.mgroup + gi;
            fprintf(stdout, "G:%d:%d ", gi, m->lifted_mgroup_id);
            pddlMGroupPrint(&pddl, &strips, m, stdout);
        }

        pddlMGroupsFree(&mgroups);
        pddlStripsFree(&strips);
    }

    pddlLiftedMGroupsFree(&lifted_mgroups);
    pddlLiftedMGroupsFree(&monotonicity_invariants);
    pddlFree(&pddl);

    BOR_INFO2(&err, "DONE");
    return 0;
}

