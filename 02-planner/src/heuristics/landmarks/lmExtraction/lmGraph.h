//
// Created by dh on 21.09.21.
//

#ifndef PANDAPIENGINE_LMGRAPH_H
#define PANDAPIENGINE_LMGRAPH_H

#include "../hhLmConstDef.h"

#include "lmNode.h"
#include "HeuristicPayload.h"
#include "landmarks/hhLMPayload.h"

//enum lmProgr {Buechner, recountGN, obeyOrderings};

class lmGraph {
private:
//    unordered_set<int> reached;
public:
    int *gList = nullptr;
    int gSize = 0;
    lmNode* lms;
    int numLMs;

    vector<bool> isTrueInGoal;

    vector<int>* predAll = nullptr;
    vector<int>* succAll = nullptr;

    vector<int>* predGredNec = nullptr;
    vector<int>* succGredNec = nullptr;
    vector<int>* predNat = nullptr;
    vector<int>* succNat = nullptr;
    vector<int>* predResonable = nullptr;
    vector<int>* succResonable = nullptr;
    void addOrdering(int pred, int succ, LMORD type);

    hhLMPayload *initLM(int numContainedTasks, int* containedTasks, vector<bool> &state);

    hhLMPayload* progressLMs(int numContainedTasks, int* containedTasks, vector<bool> &state, int appliedAction, int appliedMethod, hhLMPayload* parentPL);

    void initOrderings();

    void showDot();
    void showDot(hhLMPayload* hPL);
    string toString2(lmNode node);

    hhLMPayload *copyHPL(hhLMPayload *hpl);

    bool isLeaf(int i, hhLMPayload *pPayload);

    void printStatistics();
};


#endif //PANDAPIENGINE_LMGRAPH_H
