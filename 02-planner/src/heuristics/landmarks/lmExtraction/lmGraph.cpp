//
// Created by Daniel Höller on 21.09.21.
//

#include "lmGraph.h"
#include "landmarks/hhLMPayload.h"

#include <iostream>
#include <fstream>
#include <set>
#include <cassert>

using namespace std;

void lmGraph::initOrderings() {
    predAll = new vector<int>[numLMs];
    succAll = new vector<int>[numLMs];
    predNat = new vector<int>[numLMs];
    succNat = new vector<int>[numLMs];
    predGredNec = new vector<int>[numLMs];
    succGredNec = new vector<int>[numLMs];
    predResonable = new vector<int>[numLMs];
    succResonable = new vector<int>[numLMs];
}

void lmGraph::addOrdering(int pred, int succ, LMORD type) {
    if ((pred < 0 ) || (succ < 0) || (pred >= numLMs ) || (succ >= numLMs)) {
        cout << "ERROR: invalid index in LM ordering. [TERMINATING]" << endl;
        exit(-1);
    }
    predAll[succ].push_back(pred);
    succAll[pred].push_back(succ);
    if (type == ORD_NATURAL) {
        predNat[succ].push_back(pred);
        succNat[pred].push_back(succ);
    } else if (type == ORD_GREEDY_NECESSARY) {
        predGredNec[succ].push_back(pred);
        succGredNec[pred].push_back(succ);
    } else if (type == ORD_REASONABLE) {
        predResonable[succ].push_back(pred);
        succResonable[pred].push_back(succ);
    } else {
        cout << "TODO: implement different ordering" << endl;
        exit(-1);
    }
}

hhLMPayload *lmGraph::initLM(int numContainedTasks, int *containedTasks, vector<bool> &state) {
    for (int i = 0; i < numLMs; i++) {
        isTrueInGoal.push_back(false);
    }
    if (gSize > 0) {
        set<int> goalSet;
        for (int i = 0; i < gSize; i++) {
            goalSet.insert(gList[i]);
        }
        for (int iLM = 0; iLM < numLMs; iLM++) {
            lmNode lm = lms[iLM];
            bool tig = true;
            for (int iAtom = 0; iAtom < lm.lm.size(); iAtom++) {
                tLmAtom *a = lm.lm[iAtom];
                if ((a->type != fact) || (goalSet.find(a->lm) == goalSet.end()) || (a->isNegated)) {
                    tig = false;
                    break;
                }
            }
            if (tig) {
                isTrueInGoal[iLM] = true;
            }
        }
    }

    hhLMPayload* res = new hhLMPayload();
    for (int i = 0; i < numLMs; i++) {
        res->fulfilled.push_back(false);
    }
    return progressLMs(numContainedTasks, containedTasks, state, -1, -1, res);
}


bool lmGraph::isLeaf(int iLM, hhLMPayload *pl) {
    for (int parent: predAll[iLM]) {
        if (!pl->fulfilled[parent]) {
            return false;
        }
    }
    return true;
}

hhLMPayload *lmGraph::progressLMs(int numContainedTasks, int *containedTasks, vector<bool> &state,
                                  int appliedAction, int appliedMethod, hhLMPayload *parentPL) {
    hhLMPayload *res = copyHPL(parentPL);
    for (int iLM = 0; iLM < numLMs; iLM++) {
        if (!res->fulfilled[iLM]) {
            lmNode n = lms[iLM];
            if (n.fulfilled(numContainedTasks, containedTasks, appliedAction, appliedMethod, state)) {
                if (isLeaf(iLM, parentPL)) {
                    res->fulfilled[iLM] = true;
                }
            }
        }
    }
    return res;
}

void lmGraph::showDot() {
    showDot(nullptr);
}

void lmGraph::showDot(hhLMPayload *hPL) {
    ofstream dotfile;
    dotfile.open ("graph.dot");
    dotfile << "digraph G {" << endl;
    for (int i = 0; i < numLMs; i++) {
        bool inPLus = false;
        if ((hPL != nullptr) && (hPL->fulfilled[i])) {
            inPLus = true;
        }
        bool inMinus = false;
//        if ((hPL != nullptr) && (hPL->lookfor.find(i) != hPL->lookfor.end())) {
//            inMinus = true;
//        }
        string shape = "";
        if (inPLus) {
            shape = ",shape=box";
        }
        string color = "";
        if (inMinus) {
            color = ",style=filled,color=red";
        }

        dotfile << "  lm" << i << " [label=\"" << to_string(lms[i].lm[0]->lm) << ":" << toString2(lms[i]) << "\""  << shape << color << "];" << endl;;
    }
    for (int i = 0; i < numLMs; i++) {
        for (int j = 0; j < succNat[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << succNat[i][j] << " [label=\"nat\"];" << endl;
        }
        for (int j = 0; j < succGredNec[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << succGredNec[i][j] << " [label=\"grednec\"];" << endl;
        }
        for (int j = 0; j < succResonable[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << succGredNec[i][j] << " [label=\"reason\"];" << endl;
        }
    }
    dotfile << "}" << endl;
    dotfile.close();
    system("xdot graph.dot");
}

string lmGraph::toString2(lmNode lms) {
    string res = "";
    bool first = true;
    if (lms.lm.size() > 1) {res.append("(");}
    for (auto atom : lms.lm) {
        if (atom->isNegated) res.append("¬");
        if (atom->type == task) {
            res.append(atom->nameStr);
        } else if (atom->type == action) {
            res.append(atom->nameStr);
        } else if (atom->type == fact) {
            res.append(atom->nameStr);
        } else if (atom->type == METHOD) {
            res.append(atom->nameStr);
        } else {
            res.append("UNKNOWN LM TYPE");
        }
        if (first) {first = false;}
        else {
            if (lms.connection == conjunctive) {
                res.append("AND");
            } else if (lms.connection == disjunctive) {
                res.append("OR");
            } else {
                res.append("UNKNOWN LM CONNECTOR");
            }
        }
    }
    if (lms.lm.size() > 1) {res.append(")");}
    return res;
}

hhLMPayload *lmGraph::copyHPL(hhLMPayload *hpl) {
    hhLMPayload *res = new hhLMPayload();
    for (int i = 0; i < hpl->fulfilled.size(); i++) {
        res->fulfilled.push_back(hpl->fulfilled[i]);
    }
    return res;
}

void lmGraph::printStatistics() {
    int factLMs = 0;
    int actionLMs = 0;
    int methodLMs = 0;
    int taskLMs = 0;
    for (int i = 0; i < numLMs; i++) {
        for (int j = 0; j < lms[i].lm.size(); j++) {
            if (lms[i].lm[j]->type == fact) {
                factLMs++;
            } else if (lms[i].lm[j]->type == action) {
                actionLMs++;
            } else if (lms[i].lm[j]->type == METHOD) {
                methodLMs++;
            } else if (lms[i].lm[j]->type == task) {
                taskLMs++;
            }
        }
    }
    cout << "- LM statistics:" << endl;
    cout << "  - fact LMs:   " << factLMs <<   " [factLMs=" << factLMs << "]" << endl;
    cout << "  - action LMs: " << actionLMs << " [actionLMs=" << actionLMs << "]" << endl;
    cout << "  - method LMs: " << methodLMs << " [methodLMs=" << methodLMs << "]" << endl;
    cout << "  - task LMs:   " << taskLMs <<   " [taskLMs=" << taskLMs << "]" << endl;
}
