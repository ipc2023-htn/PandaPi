//
// Created by dh on 24.01.22.
//

#ifndef PANDAPIENGINE_HHLMCONSTDEF_H
#define PANDAPIENGINE_HHLMCONSTDEF_H

#include <string>
using namespace std;

//const int FACTLM = 0;
//const int TASKLM = 1;

enum lmType {fact, action, task, METHOD, LMCUT};

struct tLmAtom {
    bool isNegated = false;
    lmType type;
    int lm;
    string nameStr;
};

enum lmConType {atom, conjunctive, disjunctive};

enum LMORD {ORD_NECESSARY = 0, ORD_GREEDY_NECESSARY = 1, ORD_NATURAL = 2, ORD_REASONABLE = 3, ORD_OBEDIENT_REASONABLE = 4};

#endif //PANDAPIENGINE_HHLMCONSTDEF_H
