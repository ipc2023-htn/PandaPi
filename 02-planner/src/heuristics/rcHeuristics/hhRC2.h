//
// Created by dh on 10.03.20.
//

#ifndef PANDAPIENGINE_HHRC2_H
#define PANDAPIENGINE_HHRC2_H

#include <set>
#include <forward_list>
#include "../Heuristic.h"
#include "../../Model.h"
#include "../../intDataStructures/bucketSet.h"
#include "../../intDataStructures/bIntSet.h"
#include "../../intDataStructures/noDelIntSet.h"
#include "hsAddFF.h"
#include "hsLmCut.h"
#include "hsFilter.h"
#include "RCModelFactory.h"

enum eEstimate {
    estDISTANCE, estMIXED, estCOSTS
};


class hhHAPayload : public HeuristicPayload {
public:
    bool reachedByHA = false;
    int numHAs = 0;
    int* HAs;
    bool triggerBoost = false;
};

template<class ClassicalHeuristic>
class hhRC2 : public Heuristic {
private:
    noDelIntSet gset;
    noDelIntSet intSet;
    bucketSet s0set;
    RCModelFactory *factory;
    IntUtil iu;
    const eEstimate estimate = estDISTANCE;
    const bool correctTaskCount = true;
    int bestSeenhVal = INT_MAX;

public:
    ClassicalHeuristic *sasH;
    bool storeCuts = false;
    list<LMCutLandmark *>* cuts = new list<LMCutLandmark *>();
	
    hhRC2(Model *htnModel, int index, eEstimate estimate, bool correctTaskCount) : Heuristic(htnModel, index),
                                                                                   estimate(estimate),
                                                                                   correctTaskCount(correctTaskCount) {

        Model *heuristicModel;
        factory = new RCModelFactory(htnModel);
        if (estimate == estCOSTS) {
            heuristicModel = factory->getRCmodelSTRIPS(0, true); // costs of methods need to be zero
        } else {
            heuristicModel = factory->getRCmodelSTRIPS(1, true); // estimate distance -> method costs 1
            // fixme: this configuration is wired when actions have actual costs
        }

        this->sasH = new ClassicalHeuristic(heuristicModel);
        this->s0set.init(heuristicModel->numStateBits);
        this->gset.init(heuristicModel->numStateBits);
        this->intSet.init(heuristicModel->numStateBits);

        if (storeCuts) {
            if (typeid(ClassicalHeuristic) != typeid(hsLmCut)) {
                storeCuts = false;
                cout
                        << "- the option \"store cuts\" of the RC heuristic can only be used with the inner heuristic LM-Cut. It will be disabled."
                        << endl;
			}
		}
		if (correctTaskCount) {
			htnModel->calcMinimalImpliedX();
        }
    }

    virtual ~hhRC2() {
        delete factory;
    }
	
	string getDescription(){
		return "hhRC2("	+ sasH->getDescription() + ";" + (estimate == estDISTANCE?"distance":"cost") + ";" + (correctTaskCount?"correct count":"") + ")";
	}

    void setHeuristicValue(searchNode *n, searchNode *parent, int action) override {
        doFFStuff(n, parent, action);
    }

    void setHeuristicValue(searchNode *n, searchNode *parent, int absTask, int method) override {
        doFFStuff(n, parent, method + htn->numActions);
    }

    void doFFStuff(searchNode *n, searchNode *parent, int appliedOperator) {
        const bool needPayload = (deferredComputation || (helpfulActions != none));
        if (needPayload) {
            hhHAPayload *myPayload = new hhHAPayload();
            n->hPL[index] = myPayload;
            hhHAPayload *parentPayload = (hhHAPayload *) parent->hPL[index];
            if (deferredComputation) {
                if (parentPayload->ownHVal < 0) {
                    parentPayload->ownHVal = setHeuristicValue(parent);
                }
                n->heuristicValue[index] = parentPayload->ownHVal;
            } else {
                myPayload->ownHVal = setHeuristicValue(n); // not really needed in this case
                n->heuristicValue[index] = myPayload->ownHVal;
            }
            if (helpfulActions != none) {
                myPayload->reachedByHA = iu.containsInt(parentPayload->HAs, 0, parentPayload->numHAs - 1, appliedOperator);
            }
            myPayload->triggerBoost = (n->heuristicValue[index] < bestSeenhVal);
        } else {
            // no deferred computation, no helpful actions
            n->heuristicValue[index] = setHeuristicValue(n);
        }
        if (n->goalReachable) { // only do if still reachable
            n->goalReachable = (n->heuristicValue[index] != UNREACHABLE);
        }
        if (n->heuristicValue[index] < bestSeenhVal) {
            bestSeenhVal = n->heuristicValue[index]; // if deferred, this is actually the parents value
            cout << "Found new best " << sasH->getDescription() << " value: " << bestSeenhVal << endl;
        }
    }


    int setHeuristicValue(searchNode *n) {
        calculated++;
        int hval = 0;

        // get facts holding in s0
        s0set.clear();
        for (int i = 0; i < htn->numStateBits; i++) {
            if (n->state[i]) {
                s0set.insert(i);
            }
        }

        // add reachability facts and HTN-related goal
        for (int i = 0; i < n->numAbstract; i++) {
            // add reachability facts
            for (int j = 0; j < n->unconstraintAbstract[i]->numReachableT; j++) {
                int t = n->unconstraintAbstract[i]->reachableT[j];
                if (t < htn->numActions)
                    s0set.insert(factory->t2tdr(t));
            }
        }
        for (int i = 0; i < n->numPrimitive; i++) {
            // add reachability facts
            for (int j = 0; j < n->unconstraintPrimitive[i]->numReachableT; j++) {
                int t = n->unconstraintPrimitive[i]->reachableT[j];
                if (t < htn->numActions)
                    s0set.insert(factory->t2tdr(t));
            }
        }

        // generate goal
        gset.clear();
        for (int i = 0; i < htn->gSize; i++) {
            gset.insert(htn->gList[i]);
        }

        for (int i = 0; i < n->numContainedTasks; i++) {
            int t = n->containedTasks[i];
            gset.insert(factory->t2bur(t, true));
        }

        hval = this->sasH->getHeuristicValue(s0set, gset);
        if (helpfulActions != none) {
            hsAddFF * hFF = ((hsAddFF * ) this->sasH);
            hhHAPayload* payload = (hhHAPayload*)n->hPL[index];
            payload->HAs = new int[hFF->markedOps.getSize()];
            payload->numHAs = 0;
            int i = 0;
            for (int op = hFF->markedOps.getFirst(); op >= 0; op = hFF->markedOps.getNext()) {
                if ((helpfulActions == onlyActions) && (op < htn->numActions)) {
                    payload->HAs[i++] = op;
                } else if ((helpfulActions == onlyMethods) && (op >= htn->numActions)) {
                    payload->HAs[i++] = op;
                } else if (helpfulActions == all) {
                    payload->HAs[i++] = op;
                }
            }
            payload->numHAs = i;
            assert(i == payload->numHAs);
            iu.sort(payload->HAs, 0, payload->numHAs - 1); // will be used for search later on -> sort
        }

        // the indices of the methods need to be transformed to fit the scheme of the HTN model (as opposed to the rc model)
        if ((storeCuts) && (hval != UNREACHABLE)) {
            this->cuts = this->sasH->cuts;
            for (LMCutLandmark *storedcut : *cuts) {
                iu.sort(storedcut->lm, 0, storedcut->size - 1);
                /*for (int i = 0; i < storedcut->size; i++) {
                    if ((i > 0) && (storedcut->lm[i] >= htn->numActions) && (storedcut->lm[i - 1] < htn->numActions))
                        cout << "| ";
                    cout << storedcut->lm[i] << " ";
                }
                cout << "(there are " << htn->numActions << " actions)" << endl;*/

                // looking for index i s.t. lm[i] is a method and lm[i - 1] is an action
                int leftmostMethod;
                if (storedcut->lm[0] >= htn->numActions) {
                    leftmostMethod = 0;
                } else if (storedcut->lm[storedcut->size - 1] < htn->numActions) {
                    leftmostMethod = storedcut->size;
                } else { // there is a border between actions and methods
                    int rightmostAction = 0;
                    leftmostMethod = storedcut->size - 1;
                    while (rightmostAction != (leftmostMethod - 1)) {
                        int middle = (rightmostAction + leftmostMethod) / 2;
                        if (storedcut->lm[middle] < htn->numActions) {
                            rightmostAction = middle;
                        } else {
                            leftmostMethod = middle;
                        }
                    }
                }
                storedcut->firstMethod = leftmostMethod;
                /*if (leftmostMethod > storedcut->size -1)
                    cout << "leftmost method: NO METHOD" << endl;
                else
                    cout << "leftmost method: " << leftmostMethod << endl;*/
                for (int i = storedcut->firstMethod; i < storedcut->size; i++) {
                    storedcut->lm[i] -= htn->numActions; // transform index
                }
            }
        }

        /*
        for(LMCutLandmark* lm :  *cuts) {
            cout << "cut: {";
            for (int i = 0; i < lm->size; i++) {
                if(lm->isAction(i)) {
                    cout << this->htn->taskNames[lm->lm[i]];
                } else {
                    cout << this->htn->methodNames[lm->lm[i]];
                }
                if (i < lm->size - 1) {
                    cout << ", ";
                }
            }
            cout << "}" << endl;
        }*/

        if (correctTaskCount) {
            if (hval != UNREACHABLE) {
                for (int i = 0; i < n->numContainedTasks; i++) {
                    if (n->containedTaskCount[i] > 1) {
                        int task = n->containedTasks[i];
                        int count = n->containedTaskCount[i];
                        assert(task < htn->numTasks);
						if (estimate == estDISTANCE) {
                            hval += (htn->minImpliedDistance[task] * (count - 1));
                        } else if (estimate == estCOSTS) {
                            hval += (htn->minImpliedCosts[task] * (count - 1));
                        }
                    }
                }
            }
        }
        return hval;
    }
};


#endif //PANDAPIENGINE_HHRC2_H
