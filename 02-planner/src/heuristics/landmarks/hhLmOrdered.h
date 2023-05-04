//
// Created by dh on 13.09.21.
//

#ifndef PANDAPIENGINE_HHLMORDERED_H
#define PANDAPIENGINE_HHLMORDERED_H


#include <Heuristic.h>
#include <landmarks/lmExtraction/lmGraph.h>
#include <landmarks/lmExtraction/LmFdConnector.h>
#include "../../intDataStructures/bucketSet.h"

enum secondH {secNone, secFF, secAdd};

class hhLmOrdered : public Heuristic {
private:
    Model *model;

    // returns textual description of the heuristic for output
    string getDescription();

    void setHeuristicValue(searchNode *n, searchNode *parent, int action);
    void setHeuristicValue(searchNode *n, searchNode *parent, int absTask, int method);

//    int initialLM = -1;

    IntUtil iu;
    bucketSet reachableTasks;
    bool checkReachability = false;
    int bestSeenhVal = INT_MAX;

public:
    hhLmOrdered(Model *htn, int i, lmFactoryType lmf, bool useOrderings, bool checkReachability);

    lmGraph *lmG;
    int deadends = 0;

    void foo(searchNode *n, int usedMethod);

    void markHelpful(searchNode *n, searchNode *parent, int op);

    void updateReachability(const searchNode *n);

    bool allLmsReachable(searchNode *n, hhLMPayload *myPL);

    lmGraph *getRcLmCLMs(Model *htn) const;
};


#endif //PANDAPIENGINE_HHLMORDERED_H
