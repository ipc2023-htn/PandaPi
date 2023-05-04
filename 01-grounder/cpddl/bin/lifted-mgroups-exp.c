#include <stdio.h>
#include <pddl/pddl.h>
#include <opts.h>

struct options {
    int help;
    int compile_away_cond_eff;
    int compile_away_cond_eff_pddl;
    int max_candidates;
    int max_mgroups;
} o;

int main(int argc, char *argv[])
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    bor_err_t err = BOR_ERR_INIT;
    pddl_strips_t strips;
    pddl_files_t files;

    bzero(&o, sizeof(o));
    o.max_candidates = 10000;
    o.max_mgroups = 10000;

    optsAddDesc("help", 'h', OPTS_NONE, &o.help, NULL,
                "Print this help.");
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

    if (opts(&argc, argv) || o.help || (argc != 3 && argc != 2)){
        fprintf(stderr, "Usage: %s [OPTIONS] domain.pddl problem.pddl\n",
                argv[0]);
        fprintf(stderr, "  OPTIONS:\n");
        optsPrint(stderr, "    ");
        fprintf(stderr, "\n");
        return -1;
    }

    borErrWarnEnable(&err, stderr);
    borErrInfoEnable(&err, stderr);

    cfg.force_adl = 1;

    if (argc == 2){
        if (pddlFiles1(&files, argv[1], &err) != 0){
            fprintf(stderr, "Error: ");
            borErrPrint(&err, 1, stderr);
            return -1;
        }
    }else{ // argc == 3
        if (pddlFiles(&files, argv[1], argv[2], &err) != 0){
            fprintf(stderr, "Error: ");
            borErrPrint(&err, 1, stderr);
            return -1;
        }
    }

    if (pddlInit(&pddl, files.domain_pddl, files.problem_pddl,
                 &cfg, &err) != 0){
        fprintf(stderr, "Error: ");
        borErrPrint(&err, 1, stderr);
        return -1;
    }

    pddlNormalize(&pddl);
    if (o.compile_away_cond_eff_pddl)
        pddlCompileAwayCondEff(&pddl);

    BOR_INFO(&err, "Number of PDDL Types: %d", pddl.type.type_size);
    BOR_INFO(&err, "Number of PDDL Objects: %d", pddl.obj.obj_size);
    BOR_INFO(&err, "Number of PDDL Predicates: %d", pddl.pred.pred_size);
    BOR_INFO(&err, "Number of PDDL Functions: %d", pddl.func.pred_size);
    BOR_INFO(&err, "Number of PDDL Actions: %d", pddl.action.action_size);
    BOR_INFO(&err, "Number of PDDL Metric: %d", pddl.metric);
    fflush(stdout);
    fflush(stderr);


    pddl_lifted_mgroups_infer_limits_t limits
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    limits.max_candidates = o.max_candidates;
    limits.max_mgroups = o.max_mgroups;

    pddl_lifted_mgroups_t lifted_mgroups;
    pddl_lifted_mgroups_t fd_lifted_mgroups;
    pddl_lifted_mgroups_t fd_monotonicity_invariants;
    pddlLiftedMGroupsInit(&lifted_mgroups);
    pddlLiftedMGroupsInit(&fd_lifted_mgroups);
    pddlLiftedMGroupsInit(&fd_monotonicity_invariants);

    pddlLiftedMGroupsInferFAMGroups(&pddl, &limits, &lifted_mgroups, &err);
    fprintf(stdout, "Lifted MGroups:\n");
    pddlLiftedMGroupsPrint(&pddl, &lifted_mgroups, stdout);
    fflush(stdout);

    pddlLiftedMGroupsInferMonotonicity(&pddl, &limits,
                                       &fd_monotonicity_invariants,
                                       &fd_lifted_mgroups, &err);
    fprintf(stdout, "FD Lifted MGroups:\n");
    pddlLiftedMGroupsPrint(&pddl, &fd_lifted_mgroups, stdout);
    fflush(stdout);
    fprintf(stdout, "FD Monotonicity Invariants:\n");
    pddlLiftedMGroupsPrint(&pddl, &fd_monotonicity_invariants, stdout);
    fflush(stdout);

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    if (pddlStripsGround(&strips, &pddl, &ground_cfg, &err) != 0){
        BOR_INFO2(&err, "Grounding failed.");
        fprintf(stderr, "Error: ");
        borErrPrint(&err, 1, stderr);
        return -1;
    }

    if (o.compile_away_cond_eff)
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

    pddl_mgroups_t mgroups;
    pddl_mgroups_t fd_mgroups;
    pddlMGroupsGround(&mgroups, &pddl, &lifted_mgroups, &strips);
    fprintf(stdout, "Ground MGroups:\n");
    pddlMGroupsPrint(&pddl, &strips, &mgroups, stdout);
    fflush(stdout);

    pddlMGroupsGround(&fd_mgroups, &pddl, &fd_lifted_mgroups, &strips);
    fprintf(stdout, "Ground FD MGroups:\n");
    pddlMGroupsPrint(&pddl, &strips, &fd_mgroups, stdout);
    fflush(stdout);

    fprintf(stdout, "Facts:");
    for (int fact_id = 0; fact_id < strips.fact.fact_size; ++fact_id)
        fprintf(stdout, " (%s)", strips.fact.fact[fact_id]->name);
    fprintf(stdout, "\n");
    fflush(stdout);

    fprintf(stdout, "MGroups Cover Number: %d\n",
            pddlMGroupsCoverNumber(&mgroups, strips.fact.fact_size));
    fflush(stdout);
    fprintf(stdout, "FD MGroups Cover Number: %d\n",
            pddlMGroupsCoverNumber(&fd_mgroups, strips.fact.fact_size));
    fflush(stdout);

    if (!strips.has_cond_eff){
        pddl_famgroup_config_t fam_cfg = PDDL_FAMGROUP_CONFIG_INIT;
        pddl_mgroups_t fam_groups;
        pddlMGroupsInitEmpty(&fam_groups);
        pddlFAMGroupsInfer(&fam_groups, &strips, &fam_cfg, &err);
        fprintf(stdout, "Maximal FAM Groups:\n");
        pddlMGroupsPrint(&pddl, &strips, &fam_groups, stdout);
        fflush(stdout);
        pddlMGroupsFree(&fam_groups);

        pddlMGroupsInitCopy(&fam_groups, &mgroups);
        pddlFAMGroupsInfer(&fam_groups, &strips, &fam_cfg, &err);
        fprintf(stdout, "Maximal FAM Groups Incremental:\n");
        pddlMGroupsPrint(&pddl, &strips, &fam_groups, stdout);
        fflush(stdout);
        pddlMGroupsFree(&fam_groups);

        pddlMGroupsInitCopy(&fam_groups, &fd_mgroups);
        pddlFAMGroupsInfer(&fam_groups, &strips, &fam_cfg, &err);
        fprintf(stdout, "Maximal FAM Groups Incremental FD:\n");
        pddlMGroupsPrint(&pddl, &strips, &fam_groups, stdout);
        fflush(stdout);

        fprintf(stdout, "FAMGroups Cover Number: %d\n",
                pddlMGroupsCoverNumber(&fam_groups, strips.fact.fact_size));

        pddlMGroupsFree(&fam_groups);
    }


    pddlAddObjectTypes(&pddl);
    pddl_lifted_mgroups_t ot_lifted_mgroups;
    pddlLiftedMGroupsInit(&ot_lifted_mgroups);
    pddlLiftedMGroupsInferFAMGroups(&pddl, &limits, &ot_lifted_mgroups, &err);
    fprintf(stdout, "Obj-Type Lifted MGroups:\n");
    pddlLiftedMGroupsPrint(&pddl, &ot_lifted_mgroups, stdout);
    fflush(stdout);

    pddl_mgroups_t ot_mgroups;
    pddlMGroupsGround(&ot_mgroups, &pddl, &ot_lifted_mgroups, &strips);
    fprintf(stdout, "Ground Obj-Type MGroups:\n");
    pddlMGroupsPrint(&pddl, &strips, &ot_mgroups, stdout);
    fflush(stdout);


    pddlMGroupsFree(&ot_mgroups);
    pddlMGroupsFree(&mgroups);
    pddlMGroupsFree(&fd_mgroups);
    pddlStripsFree(&strips);

    pddlLiftedMGroupsFree(&ot_lifted_mgroups);
    pddlLiftedMGroupsFree(&lifted_mgroups);
    pddlLiftedMGroupsFree(&fd_lifted_mgroups);
    pddlLiftedMGroupsFree(&fd_monotonicity_invariants);
    pddlFree(&pddl);

    BOR_INFO2(&err, "DONE");
    return 0;
}

