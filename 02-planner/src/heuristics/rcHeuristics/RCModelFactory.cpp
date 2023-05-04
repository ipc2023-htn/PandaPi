/*
 * RCModelFactory.cpp
 *
 *  Created on: 09.02.2020
 *      Author: dh
 */

#include "RCModelFactory.h"

namespace progression {

RCModelFactory::RCModelFactory(Model* htn) {
	this->htn = htn;
}

RCModelFactory::~RCModelFactory() {
	// TODO Auto-generated destructor stub
}

Model* RCModelFactory::getRCmodelSTRIPS() {
	return this->getRCmodelSTRIPS(1, true);
}

    static const char *const cTDRstr = "tdr_";

    static const char *const cBURstr = "bur_";

Model* RCModelFactory::getRCmodelSTRIPS(int costsMethodActions, bool useTDR) {
	Model* rc = new Model();
	rc->isHtnModel = false;
    rc->numStateBits = htn->numStateBits + htn->numTasks;
    rc->numVars = htn->numVars + htn->numTasks; // first part might be SAS+
    if (useTDR) {
        rc->numStateBits += htn->numActions;
        rc->numVars += htn->numActions;
    }

	rc->numActions = htn->numActions + htn->numMethods;
	rc->numTasks = rc->numActions;

	rc->precLists = new int*[rc->numActions];
	rc->addLists = new int*[rc->numActions];
	rc->delLists = new int*[rc->numActions];

	rc->numPrecs = new int[rc->numActions];
	rc->numAdds = new int[rc->numActions];
	rc->numDels = new int[rc->numActions];

	// add new prec and add effect to actions
	for(int i = 0; i < htn->numActions; i++) {
		rc->numPrecs[i] = htn->numPrecs[i];
		if (useTDR) {
            rc->numPrecs[i] += 1;
        }
		rc->precLists[i] = new int[rc->numPrecs[i]];
		for(int j = 0; j < htn->numPrecs[i]; j++) {
			rc->precLists[i][j] = htn->precLists[i][j];
		}
        if (useTDR) {
            rc->precLists[i][rc->numPrecs[i] - 1] = t2tdr(i);
        }

		rc->numAdds[i] = htn->numAdds[i] + 1;
		rc->addLists[i] = new int[rc->numAdds[i]];
		for(int j = 0; j < rc->numAdds[i] - 1; j++) {
			rc->addLists[i][j] = htn->addLists[i][j];
		}
		rc->addLists[i][rc->numAdds[i] - 1] = t2bur(i, useTDR);

		rc->numDels[i] = htn->numDels[i];
		rc->delLists[i] = new int[rc->numDels[i]];
		for(int j = 0; j < rc->numDels[i]; j++) {
			rc->delLists[i][j] = htn->delLists[i][j];
		}
	}

	// create actions for methods
	for(int im = 0; im < htn->numMethods; im++) {
		int ia = htn->numActions + im;

		rc->numPrecs[ia] = htn->numDistinctSTs[im];
		rc->precLists[ia] = new int[rc->numPrecs[ia]];
		for(int ist = 0; ist < htn->numDistinctSTs[im]; ist++) {
		    int st = htn->sortedDistinctSubtasks[im][ist];
			rc->precLists[ia][ist] = t2bur(st, useTDR);
		}

		rc->numAdds[ia] = 1;
		rc->addLists[ia] = new int[1];
		rc->addLists[ia][0] = t2bur(htn->decomposedTask[im], useTDR);
		rc->numDels[ia] = 0;
		rc->delLists[ia] = nullptr;
	}

	// set names of state features
	rc->factStrs = new string[rc->numStateBits];
	for(int i = 0; i < htn->numStateBits; i++) {
		rc->factStrs[i] = htn->factStrs[i];
	}
    if (useTDR) {
        for (int i = 0; i < htn->numActions; i++) {
            rc->factStrs[t2tdr(i)] = cTDRstr + htn->taskNames[i];
        }
    }
	for(int i = 0; i < htn->numTasks; i++) {
		rc->factStrs[t2bur(i, useTDR)] = cBURstr + htn->taskNames[i];
	}

	// set action names
	rc->taskNames = new string[rc->numTasks];
	for(int i = 0; i < htn->numActions; i++) {
		rc->taskNames[i] = htn->taskNames[i];
	}
	for(int im = 0; im < htn->numMethods; im++) {
		int ia = htn->numActions + im;
		rc->taskNames[ia] = htn->methodNames[im] + "@" + htn->taskNames[htn->decomposedTask[im]];
	}

	// set variable names
	rc->varNames = new string[rc->numVars];
	for(int i = 0; i < htn->numVars; i++) {
		rc->varNames[i] = htn->varNames[i];
	}
    if (useTDR) {
        for (int i = 0; i < htn->numActions; i++) {
            // todo: the index transformation does not use the functions
            // defined above and needs to be redone when changing them
            int inew = htn->numVars + i; // be careful, this seems to be the variable, not the state bit -> that's why it is not used!
            rc->varNames[inew] = cTDRstr + htn->taskNames[i];
        }
    }
	for(int i = 0; i < htn->numTasks; i++) {
		// todo: transformation needs to be redone when changing them
		int inew = htn->numVars + i;
        if (useTDR) {
            inew += htn->numActions ;
        }
		rc->varNames[inew] = cBURstr + htn->taskNames[i];
	}

	// set indices of first and last bit
	rc->firstIndex = new int[rc->numVars];
	rc->lastIndex = new int[rc->numVars];
	for (int i = 0; i < htn->numVars; i++) {
		rc->firstIndex[i] = htn->firstIndex[i];
		rc->lastIndex[i] = htn->lastIndex[i];
	}
	for (int i = htn->numVars; i < rc->numVars; i++) {
		rc->firstIndex[i] = rc->lastIndex[i - 1] + 1;
		rc->lastIndex[i] = rc->firstIndex[i];
	}

	// set action costs
	rc->actionCosts = new int[rc->numActions];
	for (int i = 0; i < htn->numActions; i++) {
		rc->actionCosts[i] = htn->actionCosts[i];
	}
	for(int i = htn->numActions; i < rc->numActions; i++) {
		rc->actionCosts[i] = costsMethodActions;
	}

	set<int> precless;
	for(int i = 0; i < rc->numActions; i++) {
        if (rc->numPrecs[i] == 0) {
            precless.insert(i);
        }
    }
    rc->numPrecLessActions = precless.size();
	rc->precLessActions = new int[rc->numPrecLessActions];
	int j = 0;
	for(int pl : precless) {
		rc->precLessActions[j++] = pl;
	}

	rc->isPrimitive = new bool[rc->numActions];
	for(int i = 0; i < rc->numActions; i++)
		rc->isPrimitive[i] = true;

	createInverseMappings(rc);

	set<int> s;
	for(int i = 0; i < htn->s0Size; i++) {
		s.insert(htn->s0List[i]);
	}
    if (useTDR) {
        for (int i = 0; i < htn->numActions; i++) {
            s.insert(t2tdr(i));
        }
    }
	rc->s0Size = s.size();
	rc->s0List = new int[rc->s0Size];
	int i = 0;
	for (int f : s) {
		rc->s0List[i++] = f;
	}
	rc->gSize = 1 + htn->gSize;
	rc->gList = new int[rc->gSize];
	for(int i = 0; i < htn->gSize; i++) {
		rc->gList[i] = htn->gList[i];
	}
	rc->gList[rc->gSize - 1] = t2bur(htn->initialTask, useTDR);

#ifndef NDEBUG
	for(int i = 0; i < rc->numActions; i++) {
        set<int> prec;
        for(int j = 0; j < rc->numPrecs[i]; j++) {
            prec.insert(rc->precLists[i][j]);
        }
        assert(prec.size() == rc->numPrecs[i]); // precondition contained twice?

        set<int> add;
        for(int j = 0; j < rc->numAdds[i]; j++) {
            add.insert(rc->addLists[i][j]);
        }
        assert(add.size() == rc->numAdds[i]); // add contained twice?

        set<int> del;
        for(int j = 0; j < rc->numDels[i]; j++) {
            del.insert(rc->delLists[i][j]);
        }
        assert(del.size() == rc->numDels[i]); // del contained twice?
	}

	// are subtasks represented in preconditions?
	for(int i = 0; i < htn->numMethods; i++) {
	    for(int j = 0; j < htn->numSubTasks[i]; j++) {
	        int f = t2bur(htn->subTasks[i][j], useTDR);
	        bool contained = false;
	        int mAction = htn->numActions + i;
	        for(int k = 0; k < rc->numPrecs[mAction]; k++) {
	            if(rc->precLists[mAction][k] == f) {
	                contained = true;
                    break;
	            }
	        }
	        assert(contained); // is subtask contained in the respective action's preconditions?
	    }
	}

	// are preconditions represented in subtasks?
	for(int i = htn->numActions; i < rc->numActions; i++) {
	    int m = i - htn->numActions;
	    for(int j = 0; j < rc->numPrecs[i]; j++) {
	        int f = rc->precLists[i][j];
	        int task = f - htn->numStateBits;
            if (useTDR) {
                task -= + htn->numActions;
            }
	        bool contained = false;
	        for(int k = 0; k < htn->numSubTasks[m]; k++) {
	            int subtask = htn->subTasks[m][k];
	            if(subtask == task) {
	                contained = true;
                    break;
	            }
	        }
            assert(contained);
	    }
	}
#endif
	return rc;
}


void RCModelFactory::createInverseMappings(Model* c){
	set<int>* precToActionTemp = new set<int>[c->numStateBits];
	for (int i = 0; i < c->numActions; i++) {
		for (int j = 0; j < c->numPrecs[i]; j++) {
			int f = c->precLists[i][j];
			precToActionTemp[f].insert(i);
		}
	}

	c->precToActionSize = new int[c->numStateBits];
	c->precToAction = new int*[c->numStateBits];

	for (int i = 0; i < c->numStateBits; i++) {
		c->precToActionSize[i] = precToActionTemp[i].size();
		c->precToAction[i] = new int[c->precToActionSize[i]];
		int cur = 0;
		for (int ac : precToActionTemp[i]) {
			c->precToAction[i][cur++] = ac;
		}
	}

	delete[] precToActionTemp;
}

/*
 * The original state bits are followed by one bit per action that is set iff
 * the action is reachable from the top. Then, there is one bit for each task
 * indicating that task has been reached bottom-up.
 */
int RCModelFactory::t2tdr(int task) {
	return htn->numStateBits + task;
}

int RCModelFactory::t2bur(int task, bool useTDR) {
    if (useTDR) {
        return htn->numStateBits + htn->numActions + task;
    } else {
        return htn->numStateBits + task;
    }
}

pair<int, int> RCModelFactory::rcStateFeature2HtnIndex(string s) {
//	cout << "searching index for \"" << s << "\"" << endl;
//	s = s.substr(0, s.length() - 2); // ends with "()" due to grounded representation
	int type = -1;
	int index = -1;

	if ((s.rfind(cBURstr, 0) == 0)) {
        if (s == "bur___noop[]") {
            s = "bur___noop";
        }

		type = fTask;
		s = s.substr(4, s.length() - 4);
		for (int i = 0; i < htn->numTasks; i++) {
//            cout << htn->taskNames[i] << endl;
//            cout << su.toLowerString(su.cleanStr(htn->taskNames[i])) << endl;
			if (s.compare(htn->taskNames[i]) == 0) {
				index = i;
#ifndef NDEBUG
				// the name has been cleaned, check whether it is unique
				for(int j = i + 1; j < htn->numTasks; j++) {
					assert(s.compare(su.toLowerString(su.cleanStr(htn->taskNames[j]))) != 0);
				}
#endif
				break;
			}
		}
	} else {
		type = fFact;
		for (int i = 0; i < htn->numStateBits; i++) {
//            cout << htn->factStrs[i] << endl;
//            cout << su.toLowerString(su.cleanStr(htn->factStrs[i])) << endl;
			if (s.compare(htn->factStrs[i]) == 0) {
				index = i;
#ifndef NDEBUG
				// the name has been cleaned, check whether it is unique
				for (int j = i + 1; j < htn->numStateBits; j++) {
					assert(s.compare(su.toLowerString(su.cleanStr(htn->factStrs[j]))) != 0);
				}
#endif
				break;
			}
		}
	}
	//cout << "Type " << type << endl;
	//cout << "Index " << index << endl;
    return make_pair(type, index);
}


} /* namespace progression */
