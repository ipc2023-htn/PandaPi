//
// Created by Daniel Höller on 15.12.22.
//

#ifndef PANDAPIENGINE_LMLOCALLMS_H
#define PANDAPIENGINE_LMLOCALLMS_H


#include "lmGraph.h"
#include "../Model.h"

class LmLocalLMs {
private:
    set<int>* flm = new set<int>;
    set<int>* mlm = new set<int>;
    set<int>* tlm = new set<int>;

    set<int> *genLocalLMs(Model *htn, int task);
    void generateLocalLMs(Model *htn, searchNode *tnI);

public:
    lmGraph *createLMs(Model *htn);
};


#endif //PANDAPIENGINE_LMLOCALLMS_H
