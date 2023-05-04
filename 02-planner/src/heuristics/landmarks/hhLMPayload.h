//
// Created by dh on 31.03.21.
//

#ifndef PANDAPIENGINE_HHLMPAYLOAD_H
#define PANDAPIENGINE_HHLMPAYLOAD_H

#include "../HeuristicPayload.h"
#include <vector>

using namespace std;

class hhLMPayload : public HeuristicPayload {
public:
    vector<bool> fulfilled;
};


#endif //PANDAPIENGINE_HHLMPAYLOAD_H
