//
// Created by Daniel Höller on 20.10.22.
//

#include <climits>
#include "LamaFringe.h"
#include "rcHeuristics/hhRC2.h"


bool LamaFringe::isEmpty() {
    return (fFFnormal.empty() && fLM.empty() && fFFprefered.empty());
}

searchNode *LamaFringe::pop() { // might become empty
    assert(!this->isEmpty());
    wrapper *w = nullptr;
    const int iFringe = determineFringe();
    if (iFringe == iLM) {
        w = fLM.top();
        fLM.pop();
        prioLM--;
    } else if (iFringe == iFF) {
        w = fFFnormal.top();
        fFFnormal.pop();
        prioFFnormal--;
    } else { // ff preferred
        w = fFFprefered.top();
        fFFprefered.pop();
        prioFFprefered--;
    }

    assert(!w->popped); // node has not been returned before
    searchNode* n = w->n;
    w->popped = true;
    if (--w->containedInFringes == 0) {
        delete w; // this is the wrapper
    }
//    cout << "FF:" << fFFnormal.size() << " FFp:" << fFFprefered.size() << " LM:" << fLM.size() << endl;
    cleanUp(fFFnormal);
    cleanUp(fFFprefered);
    cleanUp(fLM);
//    cout << "FF:" << fFFnormal.size() << " FFp:" << fFFprefered.size() << " LM:" << fLM.size() << endl;
    return n;
}

void LamaFringe::push(searchNode *n) {
    wrapper* w = new wrapper();
    w->id = nextID++;
    w->hRand = rand();
    w->n = n;
    numNodes++;

    w->hVal = new int [numHs];
    for (int i = 0; i < numHs; i++) {
        w->hVal[i] = 0;
    }

//    cout << "node " << n->hRand << endl;
//    cout << "     " << n->heuristicValue[0] << endl;
//    cout << "     " << n->heuristicValue[1] << endl;
    if (useLM) {
        switch (lmAStarOption){
            case gValNone:  /* nothing to do */ break;
            case gValPathCosts: w->hVal[iLM] = n->modificationDepth; break;
            case gValActionCosts: w->hVal[iLM] = n->actionCosts; break;
            case gValActionPathCosts: w->hVal[iLM] = n->mixedModificationDepth; break;
        }
        w->hVal[iLM] += (lmAStarWeight * n->heuristicValue[iLM]);
        fLM.push(w);
        w->containedInFringes++;
    }
    if (useFF) {
//        w->gVal = n->modificationDepth;
        switch (ffAStarOption){
            case gValNone:  /* nothing to do */ break;
            case gValPathCosts: w->hVal[iFF] = n->modificationDepth; break;
            case gValActionCosts: w->hVal[iFF] = n->actionCosts; break;
            case gValActionPathCosts: w->hVal[iFF] = n->mixedModificationDepth; break;
        }
        w->hVal[iFF] += (ffAStarWeight * n->heuristicValue[iFF]);
        fFFnormal.push(w);
        w->containedInFringes++;
        if (useFFpref) {
            hhHAPayload *payload = ((hhHAPayload *) n->hPL[iFF]);
            if (payload->triggerBoost) {
                prioFFprefered += boost;
            }
            if (payload->reachedByHA) {
                fFFprefered.push(w);
                w->containedInFringes++;
            }
        }
    }
//    cout << "FF:" << fFFnormal.size() << " FFp:" << fFFprefered.size() << " LM:" << fLM.size() << endl;
}

int LamaFringe::size() {
    return numNodes;
}

void LamaFringe::printTypeInfo() {
//    cout << "LAMA Configuration" << endl;
//    if (useFF) {
//        cout << "- using FF heuristic [hFF=yes]" << endl;
//    }
//    if (useFFpref) {
//        cout << "- using FF preferred operators [hFFprefop=yes]" << endl;
//        cout << "- using boost " << boost << " [hFFprefopBoost=" << boost << "]" << endl;
//    }
//    if (useLM) {
//        cout << "- using LM heuristic [hLM=yes]" << endl;
//    }
}

int LamaFringe::determineFringe() {
    int max = INT_MIN;
    int use = -1;
    if ((useLM) && (!fLM.empty()) && (prioLM >= max)) {
        max = prioLM;
        use = iLM;
    }
    if ((useFF) && (!fFFnormal.empty()) && (prioFFnormal >= max)) {
        max = prioFFnormal;
        use = iFF;
    }
    if ((useFFpref) && (!fFFprefered.empty()) && (prioFFprefered >= max)) {
        use = iFFp;
    }
    return use;
}

/*
 * This function pops elements from the fringe until:
 * (1) the top element has not been popped before, or
 * (2) the fringe is empty
 */
void LamaFringe::cleanUp(priority_queue<wrapper *, vector<wrapper *>, LamaComperator> &queue) {
    while (!queue.empty()) {
        wrapper *n = queue.top();
        if (n->popped) {
            queue.pop(); // this node has been returned before from a different fringe -> discard
            if (--n->containedInFringes == 0) {
                // counts in how many fringes it has been popped -> if it was the last one, delete the wrapper
                delete n;
            }
        } else {
            break; // the top element was not returned before
        }
    }
}

bool LamaComperator::operator()(const wrapper *a, const wrapper *b) const {
    assert(a != nullptr);
    assert(b != nullptr);
//    cout << "compare " << a->hRand << " and " << b->hRand << endl;
//    cout << a->heuristicValue[index] << endl;
//    cout << b->heuristicValue[index] << endl;
    if (a->hVal[index] != b->hVal[index]) {
        return a->hVal[index] > b->hVal[index];
//    } else if (a->gVal != b->gVal){
//        return a->gVal < b->gVal;
    } else {
//        const int otherH = index + 1 % 2; // use the other heuristic as tiebreaker
//        if (a->heuristicValue[otherH] != b->heuristicValue[otherH]) {
//            return a->heuristicValue[otherH] > b->heuristicValue[otherH];
//        } else {
            return a->id > b->id;
//            return a->hRand > b->hRand;
//        }
    }
}
