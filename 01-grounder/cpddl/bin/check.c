#include <stdio.h>
#include <pddl/pddl.h>
#include <opts.h>

struct options {
    int help;
    int quiet;
    int force_adl;
    int compile_away_cond_eff;
    int compile_away_cond_eff_pddl;
} o;

int main(int argc, char *argv[])
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    pddl_t pddl;
    bor_err_t err = BOR_ERR_INIT;
    pddl_strips_t strips;

    bzero(&o, sizeof(o));
    optsAddDesc("help", 'h', OPTS_NONE, &o.help, NULL,
                "Print this help.");
    optsAddDesc("quiet", 'q', OPTS_NONE, &o.quiet, NULL,
                "Disable logging and error output.");
    optsAddDesc("adl", 'a', OPTS_NONE, &o.force_adl, NULL,
                "Force :adl requirement even if it is not specified in the"
                " domain file.");
    optsAddDesc("ce", 0x0, OPTS_NONE, &o.compile_away_cond_eff, NULL,
                "Compile away conditional effects on the STRIPS level"
                " (recommended).");
    optsAddDesc("ce-pddl", 0x0, OPTS_NONE, &o.compile_away_cond_eff_pddl, NULL,
                "Compile away conditional effects on the PDDL level.");

    if (opts(&argc, argv) || o.help || argc != 3){
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
        borErrInfoEnable(&err, stdout);
    }

    cfg.force_adl = 0;
    if (o.force_adl)
        cfg.force_adl = 1;

    if (pddlInit(&pddl, argv[1], argv[2], &cfg, &err) != 0){
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
    }

    pddlStripsFree(&strips);
    pddlFree(&pddl);
    return 0;
}
