//
// Created by dh on 29.03.21.
//

#ifndef PANDAPIENGINE_HEURISTIC_H
#define PANDAPIENGINE_HEURISTIC_H

#include "../Model.h"

enum helpfulA {none, onlyActions, onlyMethods, all};

class Heuristic {
protected:
    int index;
    Model* htn;
public:
    int calculated = 0;
    helpfulA helpfulActions = none;
    bool deferredComputation = false;
    Heuristic(Model* htnModel, int index);
    
	// returns textual description of the heuristic for output 
	virtual string getDescription() = 0;

    virtual void setHeuristicValue(searchNode *n, searchNode *parent, int action) = 0;
    virtual void setHeuristicValue(searchNode *n, searchNode *parent, int absTask, int method) = 0;
};


#endif //PANDAPIENGINE_HEURISTIC_H
