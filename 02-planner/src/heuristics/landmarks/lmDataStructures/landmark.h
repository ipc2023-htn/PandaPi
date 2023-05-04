/*
 * landmark.h
 *
 *  Created on: 12.02.2020
 *      Author: dh
 */

#ifndef HEURISTICS_LANDMARKS_LMDATASTRUCTURES_LANDMARK_H_
#define HEURISTICS_LANDMARKS_LMDATASTRUCTURES_LANDMARK_H_

#include "landmarks/hhLmConstDef.h"

namespace progression {

class landmark {
public:
	landmark(lmConType connection, lmType type, int size);
	landmark();
	virtual ~landmark();

	lmConType connection;
	lmType type;
	int size = 0;
	int* lm = nullptr;

	void printLM();
    static int coutLM(landmark** lm, lmType type, int size);
};

} /* namespace progression */

#endif /* HEURISTICS_LANDMARKS_LMDATASTRUCTURES_LANDMARK_H_ */
