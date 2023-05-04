//
// Created by dh on 04.02.22.
//

#ifndef PANDAPIENGINE_LMNODE_H
#define PANDAPIENGINE_LMNODE_H

#include "landmarks/hhLmConstDef.h"
//#include "../../../Model.h"
#include "../../../intDataStructures/IntUtil.h"
#include <vector>
#include <string>

using namespace std;

class lmNode {
private:
    IntUtil iu;
public:
    lmConType connection = conjunctive;
    vector<tLmAtom*> lm;

    bool fulfilled(int numContainedTasks, int* containedTasks, int appliedAction, int appliedMethod, vector<bool> &state);
};


#endif //PANDAPIENGINE_LMNODE_H
