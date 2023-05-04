/*
 * LmFdConnector.h
 *
 *  Created on: 09.02.2020
 *      Author: dh
 */

#ifndef HEURISTICS_LANDMARKS_LMFDCONNECTOR_H_
#define HEURISTICS_LANDMARKS_LMFDCONNECTOR_H_

#include <list>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include "../../../Model.h"
#include "../../../intDataStructures/StringUtil.h"
#include "../../rcHeuristics/RCModelFactory.h"
#include "lmGraph.h"
#include "../hhLmConstDef.h"

namespace progression {

enum lmFactoryType {rhw, ao1, ao2, nativeAO, localLMs, lmCutLMs, lmCutLMrecomp, lmDof};

class LmFdConnector {
public:
	LmFdConnector();
	virtual ~LmFdConnector();

    lmGraph* createLMs(Model* htn, lmFactoryType lmf, bool useOrderings);

//	int numLMs = -1;
//	int numConjunctive = -1;
//	landmark** landmarks = nullptr;

//	int getNumLMs();
//	landmark** getLMs();
    void printDOT(progression::Model *pModel, lmGraph* g);
    string toString2(Model *m, lmNode &lms);

private:
	StringUtil su;

    lmGraph* readFDLMs(string f, RCModelFactory* factory, bool useOrderings);
//	int getIndex(string f, Model* rc);
    string revertRenaming(string s);
    Model *htn;

        int getNodeID(string &basicString);
    };

} /* namespace progression */

#endif /* HEURISTICS_LANDMARKS_LMFDCONNECTOR_H_ */
