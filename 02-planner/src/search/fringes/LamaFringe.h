//
// Created by Daniel Höller on 20.10.22.
//

#ifndef PANDAPIENGINE_LAMAFRINGE_H
#define PANDAPIENGINE_LAMAFRINGE_H


class LamaFringe;

#include "../../ProgressionNetwork.h"
#include <queue>
#include <cassert>

struct wrapper {
    int containedInFringes = 0;
    int gVal = 0;
    bool popped = false;
    int id = -1;
    int* hVal = nullptr;
    int hRand;
    searchNode* n = nullptr;
    ~wrapper() {
        delete [] hVal;
        // do not delete n, this is done in the search procedure
    }
};

struct LamaComperator {
    int index;
    LamaComperator(int hIndex) : index(hIndex){}
    bool operator()(const wrapper* a, const wrapper* b) const;
};

class LamaFringe {
public:
    LamaFringe(int iLM, int iFF, bool useFFpref) :
    useFF(iFF >= 0),
    useLM(iLM >= 0),
    useFFpref(useFFpref),
    iLM(iLM),
    iFF(iFF),
    fFFnormal(LamaComperator(iFF)),
    fFFprefered(LamaComperator(iFF)),
    fLM(LamaComperator(iLM)) {
        assert((!useFFpref) || useFF);
        int numFringes = 0;
        if (useLM) {
            numFringes++;
        }
        if (useFF) {
            numFringes++;
        }
        if (useFFpref) {
            numFringes++;
        }
        assert(numFringes > 0);
        numHs = -1;
        if (iFF > numHs) {
            numHs = iFF;
        }
        if (iLM > numHs) {
            numHs = iLM;
        }
        numHs++; // this shall be the size, not the max index
    };

    aStar ffAStarOption = progression::gValNone;
    aStar lmAStarOption = progression::gValNone;
    int ffAStarWeight = 1;
    int lmAStarWeight = 1;

    bool isEmpty();
    searchNode* pop();
    void push(searchNode* n);
    int size();

    void printTypeInfo();

private:
    const bool useFF;
    const bool useLM;
    const bool useFFpref;
    const int iLM;
    const int iFF;
    const int iFFp = 2;
    const int boost = 1000;
    int numNodes = 0;
    int nextID = INT_MIN;
    int numHs;

    priority_queue<wrapper*, vector<wrapper*>, LamaComperator > fFFnormal;
    priority_queue<wrapper*, vector<wrapper*>, LamaComperator > fFFprefered;
    priority_queue<wrapper*, vector<wrapper*>, LamaComperator > fLM;
    int prioFFnormal = 0;
    int prioFFprefered = 0;
    int prioLM = 0;

    int determineFringe();

    void cleanUp(priority_queue<wrapper *, vector<wrapper *>, LamaComperator> &queue);
};


#endif //PANDAPIENGINE_LAMAFRINGE_H
