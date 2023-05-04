//
// Created by Daniel Höller on 15.12.22.
// Elkawkagy et al.'s recursive local landmarks
//

#include <sys/time.h>
#include "LmLocalLMs.h"

lmGraph *LmLocalLMs::createLMs(Model *htn) {
    searchNode *tnI = htn->prepareTNi(htn);
    generateLocalLMs(htn, tnI);
    delete tnI;

    // create graph
    lmGraph* g = new lmGraph();
    g->numLMs = flm->size() + mlm->size() + tlm->size();
    g->lms = new lmNode[g->numLMs];
    int i = 0;
    for (int lm: *flm) {
        g->lms[i].connection = conjunctive;
        tLmAtom* atom = new tLmAtom();
        atom->type = fact;
        atom->lm = lm;
        atom->isNegated = false;
        g->lms[i].lm.push_back(atom);
        i++;
    }
    for (int lm: *mlm) {
        g->lms[i].connection = conjunctive;
        tLmAtom* atom = new tLmAtom();
        atom->type = METHOD;
        atom->lm = lm;
        atom->isNegated = false;
        g->lms[i].lm.push_back(atom);
        i++;
    }
    for (int lm: *tlm) {
        g->lms[i].connection = conjunctive;
        tLmAtom* atom = new tLmAtom();
        atom->type = task;
        atom->lm = lm;
        atom->isNegated = false;
        g->lms[i].lm.push_back(atom);
        i++;
    }
    g->initOrderings();
    return g;
}

void LmLocalLMs::generateLocalLMs(Model* htn, searchNode* tnI){
    timeval tp;
    gettimeofday(&tp, NULL);
    long startT = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    set<int> initialTasks;
    set<int> done;
    vector<planStep*> todoList;
    for (int i = 0; i < tnI->numPrimitive; i++)
        todoList.push_back(tnI->unconstraintPrimitive[i]);
    for (int i = 0; i < tnI->numAbstract; i++)
        todoList.push_back(tnI->unconstraintAbstract[i]);
    while (!todoList.empty()) {
        planStep* ps = todoList.back();
        todoList.pop_back();
        done.insert(ps->id);
        initialTasks.insert(ps->task);
        for (int i = 0; i < ps->numSuccessors; i++) {
            planStep* succ = ps->successorList[i];
            bool included = done.find(succ->id) != done.end();
            if (!included)
                todoList.push_back(succ);
        }
    }

    set<int>* lms = new set<int>();
    set<int>* collect = new set<int>();
    for (set<int>::iterator it = initialTasks.begin(); it != initialTasks.end(); ++it) {
        int task = *it;
        set<int>* newLMs = genLocalLMs(htn, task);
        for (set<int>::iterator it2 = newLMs->begin(); it2 != newLMs->end(); ++it2) {
            collect->insert(*it2);
            lms->insert(*it2);
        }
        delete newLMs;
    }
    while(!collect->empty()) {
        set<int>* lastRound = collect;
        collect = new set<int>();
        for (set<int>::iterator it = lastRound->begin(); it != lastRound->end(); ++it) {
            set<int>* newLMs = genLocalLMs(htn, *it);
            for (set<int>::iterator it2 = newLMs->begin(); it2 != newLMs->end(); ++it2) {
                int lm = *it2;
                if(lms->find(lm) == lms->end()){
                    collect->insert(lm);
                    lms->insert(lm);
                }
            }
        }
        delete lastRound;
    }

    tlm->clear();
    flm->clear();
    mlm->clear();

    for (int lm : *lms) {
        tlm->insert(lm);
    }

    gettimeofday(&tp, NULL);
    long endT = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    cout << "- time for LM extraction (ms) : " << (endT - startT) << endl;
}

set<int>* LmLocalLMs::genLocalLMs(Model* htn, int task){
    set<int>* res = new set<int>();
    set<int>* res2 = new set<int>();
    bool first = true;
    for(int i = 0; i < htn->numMethodsForTask[task]; i++) {
        int m = htn->taskToMethods[task][i];
        if(first){
            for(int iST =0; iST < htn->numSubTasks[m]; iST++){
                int st = htn->subTasks[m][iST];
                res->insert(st);
            }
            first = false;
        } else {
            res2->clear();
            for(int iST =0; iST < htn->numSubTasks[m]; iST++){
                int st = htn->subTasks[m][iST];
                if(res->find(st) != res->end())
                    res2->insert(st);
            }
            set<int>* temp = res;
            res = res2;
            res2 = temp;
        }
    }
    return res;
}