//
// Created by dh on 13.09.21.
//

#include "hhLmOrdered.h"
#include "hhLMPayload.h"
#include "landmarks/lmExtraction/LMsInAndOrGraphs.h"
#include "landmarks/lmExtraction/LmLocalLMs.h"
#include "landmarks/lmExtraction/LmCausal.h"
#include "rcHeuristics/hsLmCut.h"
#include "rcHeuristics/hhRC2.h"
#ifndef CMAKE_NO_ILP
#include "dofHeuristics/dofLmFactory.h"
#endif
#include <sys/time.h>
#include <iomanip>

hhLmOrdered::hhLmOrdered(Model *htn, int i, lmFactoryType lmf, bool useOrderings, bool checkReachability) : Heuristic(htn, i) {
    timeval tp;
    gettimeofday(&tp, NULL);
    long startT = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    cout << "Initializing landmark count heuristic" << endl;
    if (useOrderings) {
        cout << "- using landmark orderings (if supported by the generator) [useOrderings=yes]." << endl;
    } else {
        cout << "- using NO landmark orderings [useOrderings=no]." << endl;
    }
    this->model = htn;
    this->checkReachability = checkReachability;

    if ((lmf == rhw) || (lmf == ao1) || (lmf == ao2)) {
        if (lmf == rhw) {
            cout << "- Creating RHW LMs using FD landmark generation [lmType=fdrhw]" << endl;
        } else if  (lmf == ao1) {
            cout << "- Creating AND/OR (1) LMs using FD landmark generation [lmType=fdao1]" << endl;
        } else if (lmf == ao2) {
            cout << "- Creating AND/OR (2) LMs using FD landmark generation [lmType=fdao2]" << endl;
        }
        LmFdConnector FDcon;
        lmG = FDcon.createLMs(htn, lmf, useOrderings);
    } else if (lmf == nativeAO) {
        cout << "- Creating AND/OR (1) LMs using native implementation [lmType=natao1]" << endl;
        LmCausal factory(htn);
        lmG = factory.calcLMs();
    }  else if (lmf == localLMs) {
        cout << "- Creating local LMs using native implementation [lmType=natlocal]" << endl;
        LmLocalLMs factory;
        lmG = factory.createLMs(htn);
    }  else if (lmf == lmCutLMs) {
        cout << "- Creating LMs using LM Cut [lmType=lmCut]" << endl;
        lmG = getRcLmCLMs(htn);
#ifndef CMAKE_NO_ILP
    } else if (lmf == lmDof) {
        cout << "- Creating LMs using DOF heuristic [lmType=lmDof]" << endl;
        dofLmFactory factory;
        lmG = factory.createLMs(htn);
#endif
    }

    lmG->gSize = htn->gSize;
    lmG->gList = htn->gList;

    gettimeofday(&tp, NULL);
    long currentT = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    cout << "(done). [tGenLms="<< fixed << setprecision(2) << (currentT - startT)<< "]" << endl;
    cout << "- Found " << lmG->numLMs << " landmark(s) [numLMs=" << lmG->numLMs << "]" << endl;
    htn->LMs = lmG;
    htn->ihLM = i;
    lmG->printStatistics();

    // store reachability
    reachableTasks.init(htn->numTasks);
}

lmGraph *hhLmOrdered::getRcLmCLMs(Model *htn) const {
    hhRC2<hsLmCut> *hRC = new hhRC2<hsLmCut>(htn, 0, estDISTANCE, false);
    hRC->storeCuts = true;
    hRC->sasH->storeCuts = true;
    searchNode *tnI = htn->prepareTNi(htn);
    htn->updateReachability(tnI);
    hRC->setHeuristicValue(tnI);
    lmGraph* g = new lmGraph();
    g->numLMs = hRC->cuts->size();
    g->lms = new lmNode[g->numLMs];
    int iLM = 0;
    for (LMCutLandmark *storedcut : *hRC->cuts) {
        g->lms[iLM].connection = disjunctive;
        for (int i = 0; i < storedcut->size; i++) {
            tLmAtom* atom = new tLmAtom();
            if (storedcut->isAction(i)) {
                atom->type = action;
                atom->nameStr = to_string(storedcut->lm[i]) + ":" + htn->taskNames[storedcut->lm[i]];
                atom->lm = storedcut->lm[i];
            } else {
                atom->type = METHOD;
                atom->nameStr = to_string(storedcut->lm[i]) + ":" + htn->methodNames[storedcut->lm[i]];
                atom->lm = storedcut->lm[i];
            }
            atom->isNegated = false;
            g->lms[iLM].lm.push_back(atom);
        }
        iLM++;
    }
    g->initOrderings();
    delete tnI;
    delete hRC;
    return g;
}

void hhLmOrdered::setHeuristicValue(searchNode *n, searchNode *parent, int action) {
    foo(n, -1);
}

void hhLmOrdered::setHeuristicValue(searchNode *n, searchNode *parent, int absTask, int method) {
    foo(n, method);
}

void hhLmOrdered::foo(searchNode *n, int usedMethod) {
    calculated++;
    hhLMPayload* myPL = ((hhLMPayload*)n->hPL[index]);

    if ((checkReachability) && (!allLmsReachable(n, myPL))) {
        n->heuristicValue[index] = UNREACHABLE;
        n->goalReachable = false;
        deadends++;
        return;
    }

    n->heuristicValue[index] = 0;
    for (int iLM = 0; iLM < myPL->fulfilled.size(); iLM++) {
        if (!myPL->fulfilled[iLM]) {
            n->heuristicValue[index]++;
        } else if (lmG->isTrueInGoal[iLM]) {
            n->heuristicValue[index]++; // required again because in goal
        } else {
            for (int iChild: lmG->succGredNec[iLM]) {
                if (!myPL->fulfilled[iChild]) {
                    n->heuristicValue[index]++; // required again because of greedy necessary ordering
                    break;
                }
            }
        }
    }
    if (n->heuristicValue[index] < bestSeenhVal) {
        bestSeenhVal = n->heuristicValue[index];
        cout << "Found new best hLMC value: " << bestSeenhVal << endl;
    }
}

void hhLmOrdered::updateReachability(const searchNode *n) {
    reachableTasks.clear();
    for (int i = 0; i < n->numAbstract; i++) {
        for (int j = 0; j < n->unconstraintAbstract[i]->numReachableT; j++) {
            int t = n->unconstraintAbstract[i]->reachableT[j];
            reachableTasks.insert(t);
        }
    }
    for (int i = 0; i < n->numPrimitive; i++) {
        for (int j = 0; j < n->unconstraintPrimitive[i]->numReachableT; j++) {
            int t = n->unconstraintPrimitive[i]->reachableT[j];
            reachableTasks.insert(t);
        }
    }
}

string hhLmOrdered::getDescription() {
    return "Landmark heuristic including ordering relations";
}

bool hhLmOrdered::allLmsReachable(searchNode *n, hhLMPayload *myPL) {
    updateReachability(n);// calculate reachable tasks
//    cout << endl;
//    for (int t = reachableTasks.getFirst(); t >= 0; t = reachableTasks.getNext()) {
//        cout << " " << t;
//    }
//    cout << endl;
//
//    for (int i = 0; i < n->numContainedTasks; i++) {
//        cout << " " << n->containedTasks[i];
//    }
//    cout << endl;
//    cout << endl;

    for (int iLM = 0; iLM < lmG->numLMs; iLM++) {
        if (myPL->fulfilled[iLM]) {
            continue;
        }
        lmNode lm = lmG->lms[iLM];
        bool lmFulfillable;
        if (lm.connection == disjunctive) {
            lmFulfillable = false;
        } else if (lm.connection == conjunctive) {
            lmFulfillable = true;
        }
        for (int iAtom = 0; iAtom < lm.lm.size(); iAtom++) {
            tLmAtom *atom = lm.lm[iAtom];
            bool atomFulfillable = false;
            if (atom->type == task) {
//                cout << "- " << atom->lm << ": " << htn->taskNames[atom->lm] << endl;
//                atomFulfillable = iu.containsInt(n->containedTasks, 0, n->numContainedTasks - 1, atom->lm)
//                                  || reachableTasks.get(atom->lm);
                atomFulfillable = reachableTasks.get(atom->lm);
            } else if (atom->type == METHOD) {
                atomFulfillable = reachableTasks.get(htn->decomposedTask[atom->lm]);
            } else if (atom->type == fact) {
                int f = atom->lm;
                if (n->state[f] != atom->isNegated) {
                    atomFulfillable = true;
                } else { // is there an action reachable that may achieve it?
                    if (!atom->isNegated) {
                        for (int ai = 0; ai < htn->addToActionSize[f]; ai++) {
                            int a = htn->addToAction[f][ai];
                            if (reachableTasks.get(a)) {
                                atomFulfillable = true;
                                break;
                            }
                        }
                    } else { // negated
                        for (int ai = 0; ai < htn->delToActionSize[f]; ai++) {
                            int a = htn->delToAction[f][ai];
                            if (reachableTasks.get(a)) {
                                atomFulfillable = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (atomFulfillable && (lm.connection == disjunctive)) {
                lmFulfillable = true;
                break; // atom loop
            }
            if (!atomFulfillable && (lm.connection == conjunctive)) {
                lmFulfillable = false;
                break; // atom loop
            }
        }
        if (!lmFulfillable){
//            cout << "blub" << endl;
            return false;
        }
    }
    return true;
}
