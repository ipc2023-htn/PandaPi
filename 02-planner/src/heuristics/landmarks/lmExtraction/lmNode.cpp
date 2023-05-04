//
// Created by Daniel Höller on 04.02.22.
//

#include "lmNode.h"
#include <iostream>

bool lmNode::fulfilled(int numContainedTasks, int* containedTasks, int appliedAction, int appliedMethod,  vector<bool> &state) {
    for (auto atom : this->lm) {
        bool atomFulfilled;
        if (atom->type == action) {
            // todo: this looks odd
            if (atom->isNegated) {
                atomFulfilled = !iu.containsInt(containedTasks, 0, numContainedTasks - 1, atom->lm);
            } else {
                atomFulfilled = (appliedAction == atom->lm);
            }
        } else if (atom->type == task) {
            const bool contained = iu.containsInt(containedTasks, 0, numContainedTasks - 1, atom->lm);
            atomFulfilled = (contained != atom->isNegated);
        } else if (atom->type == fact) {
            atomFulfilled = (state[atom->lm] != atom->isNegated);
        } else if (atom->type == METHOD) {
            atomFulfilled = (appliedMethod == atom->lm);
        } else {
            cout << "ERROR: Unimplemented LM type" << endl;
            exit(-1);
        }
        if ((connection == conjunctive) && (!atomFulfilled)) {
            return false;
        } else if ((connection == disjunctive) && (atomFulfilled)) {
            return true;
        }
    }
    if (connection == conjunctive) {
        return true;
    } else if (connection == disjunctive) {
        return false;
    } else {
        cout << "ERROR: Unimplemented LM connection" << endl;
        exit(-1);
    }
}
