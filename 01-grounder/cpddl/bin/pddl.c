#include <stdio.h>
#include <pddl/pddl.h>

int main(int argc, char *argv[])
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    bor_err_t err = BOR_ERR_INIT;

    if (argc != 3){
        fprintf(stderr, "Usage: %s domain.pddl problem.pddl\n", argv[0]);
        return -1;
    }

    borErrWarnEnable(&err, stderr);
    borErrInfoEnable(&err, stderr);
    cfg.force_adl = 1; // TODO: parametrize
    if (pddlInit(&pddl, argv[1], argv[2], &cfg, &err) != 0){
        borErrPrint(&err, 1, stderr);
        return -1;
    }

    pddlNormalize(&pddl);
    pddlPrintDebug(&pddl, stdout);

    pddlFree(&pddl);
    return 0;
}


