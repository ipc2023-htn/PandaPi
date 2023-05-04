/*
 * LmFdConnector.cpp
 *
 *  Created on: 09.02.2020
 *      Author: dh
 */

#include "LmFdConnector.h"
#include <sys/time.h>
#include <iomanip>

namespace progression {

LmFdConnector::LmFdConnector() {
	// TODO Auto-generated constructor stub

}

LmFdConnector::~LmFdConnector() {
	// TODO Auto-generated destructor stub
}

lmGraph* LmFdConnector::createLMs(Model* htn, lmFactoryType lmf, bool  useOrderings) {
    this->htn = htn;
	RCModelFactory* factory = new RCModelFactory(htn);
	Model* rc = factory->getRCmodelSTRIPS(1, false);
	cout << "- Writing PDDL model to generate FD landmarks..." << flush;
    timeval tp;
    gettimeofday(&tp, NULL);
    long startT = tp.tv_sec * 1000 + tp.tv_usec / 1000;
//    rc->writeToPDDL("/home/dh/Schreibtisch/FD-LM-extraction/d.pddl", "/home/dh/Schreibtisch/FD-LM-extraction/p.pddl");
    rc->writeToFDFormat("fd.in");
    gettimeofday(&tp, NULL);
    long currentT = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	cout << "(done). [tWriteLmModel="<< fixed << setprecision(2) << (currentT - startT)<< "]" << endl;

	cout << "- Calling FD landmark generator..." << flush;
    if (lmf == rhw) {
        system("\"callFDfromPANDA.sh\" \"rhw\"");
    } else if (lmf == ao1) {
        system("\"callFDfromPANDA.sh\" \"ao1\"");
    } else if (lmf == ao2) {
        system("\"callFDfromPANDA.sh\" \"ao2\"");
    }

	cout << "- Reading FD landmarks..." << endl;
    lmGraph* g = this->readFDLMs("fd.out", factory, useOrderings);
	//this->readFDLMs("/home/dh/Schreibtisch/temp/lmc-out.txt", factory);
//	cout << "(done)." << endl;

//    printDOT(htn, g);
    return g;
}

lmGraph* LmFdConnector::readFDLMs(string f, RCModelFactory* factory, bool useOrderings) {
    lmGraph* g = new lmGraph();
    list<landmark*> lms;
	const char *cstr = f.c_str();
	ifstream domainFile(cstr);
	string line;
	getline(domainFile, line);

    //
    // parse file
    //
	while (line.compare("digraph G {") != 0) {
		getline(domainFile, line);
	}
    unordered_map<string, int> strID2id;
    unordered_map<string, int> strType2tid;
//    int id = 0;
    int tID = 0;
    vector<int*> tOrderings;
    vector<string> tLms;
	while(true) {
		getline(domainFile, line);
        if (line.compare("}") == 0) {
            break;
        }
//        cout << line << endl;
        int marker = line.find(" -> ", 0);
        if (marker >= 0) { // this is an ordering
            if (!useOrderings) {
                continue;
            }
            int start = 0;
            while (line[start] == ' ') {
                start++;
            }
            string startNode = line.substr(start, marker - start);
            int startID = getNodeID(startNode);
            //cout << startNode << endl;
            start = marker + 4;
            int end = start;
            while (line[end] != ' ') {
                end++;
            }
            string endNode = line.substr(start, end - start);
            int endID = getNodeID(endNode);
            //cout << endNode << endl;
            start = line.find("label=\"") + 7;
            end = start;
            while (line[end] != '\"') {
                end++;
            }
            string otype = line.substr(start, end - start);
            //cout << otype << endl;
            if (strType2tid.find(otype) == strType2tid.end()) {
                strType2tid[otype] = tID++;
            }
            int *ordering = new int[3];
            ordering[0] = startID;
            ordering[1] = endID;
//            cout << line << endl;
//            cout << "to: " << startID << " -> " << endID << endl;

            if (otype.compare("nec") == 0) {
                ordering[2] = ORD_NECESSARY;
            } else if (otype.compare("gn") == 0) {
                ordering[2] = ORD_GREEDY_NECESSARY;
            } else if (otype.compare("n") == 0) {
                ordering[2] = ORD_NATURAL;
            } else if (otype.compare("r") == 0) {
                ordering[2] = ORD_REASONABLE;
            } else if (otype.compare("o_r") == 0) {
                ordering[2] = ORD_OBEDIENT_REASONABLE;
            } else {
                cout << "ERROR: ordering type " << otype
                     << " returned by the FD translation not supported. [TERMINATING]" << endl;
                exit(-1);
            }
            tOrderings.push_back(ordering);
        } else { // this is a landmark
            int start = 0;
            while (line[start] == ' ') {
                start++;
            }
            int end = start;
            while (line[end] != ' ') {
                end++;
            }
//            cout << line << endl;
            string strID = line.substr(start, end - start);

            int id = getNodeID(strID);
            if (tLms.size() != id) { // this is the position it will
                cout << "ERROR parsing LM \"" << line << "\"" << endl;
            }

            start = 0;
            while (line[start] != '\"') {
                start++;
            }
            start++;
            end = start;
            while (line[end] != '\"') {
                end++;
            }
            string lmName = line.substr(start, end - start);
            //cout << lmName << endl;
            tLms.push_back(lmName);
        }
	}

    //
    // creating LM graph
    //
    // (1) creating LM nodes
    //
    g->numLMs = tLms.size();
    g->lms = new lmNode[g->numLMs];
//    cout << endl;
    for (int i = 0; i < tLms.size(); i++) {
        // LM definition might include more than one atom -> need to split
        bool isConj = false;
        bool isDisj = false;
        string multiAtomStr = tLms[i];

//        cout << multiAtomStr << endl;

        vector<int> atomStart;
        vector<int> atomEnd;
        atomStart.push_back(0);
        for (int j = 0; j < multiAtomStr.size(); j++) {
            if ((multiAtomStr[j] == '&') || (multiAtomStr[j] == '|')) {
                if (multiAtomStr[j] == '&') {
                    assert(!isDisj); // no mixed lms supported
                    isConj = true;
                } else if (multiAtomStr[j] == '|') {
                    assert(!isConj); // no mixed lms supported
                    isDisj = true;
                }
                atomEnd.push_back(j - 1);
                atomStart.push_back(j + 2);
                j++;
            }
        }
        atomEnd.push_back(multiAtomStr.size());

        if (isDisj) {
            g->lms[i].connection = disjunctive;
        } else if (isConj) {
            g->lms[i].connection = conjunctive;
        }
        // now parse atoms and combine to complex LMs
        for (int j = 0; j < atomStart.size(); j++) {
            string oneAtom = multiAtomStr.substr(atomStart[j], (atomEnd[j] - atomStart[j])); // one of the atoms
//            cout << j << ": \"" << oneAtom << "\"" << endl;

            string sType;
            string sLmName;
            if (oneAtom == "<none of those>") {
                sType = "Atom";
                sLmName = oneAtom;
            } else {
                int splitI = 0; // get name of actual atom
                while (oneAtom[splitI] != ' ') {
                    splitI++;
                }
                sType = oneAtom.substr(0, splitI);
                sLmName = oneAtom.substr(splitI + 1, oneAtom.length() - splitI - 1);
                sLmName = revertRenaming(sLmName);
            }
            pair<int, int> lmAtom = factory->rcStateFeature2HtnIndex(sLmName);

//            cout << "sLmName: " << sLmName << endl;

            if (lmAtom.second == -1) {
                cout << "ERROR: the landmark " << sLmName << " returned by the FD translation has not been found in the HTN domain. [TERMINATING]" << endl;
                exit(-1);
            }

            // set lm
            tLmAtom *lma = new tLmAtom();
            g->lms[i].lm.push_back(lma);
            lma->lm = lmAtom.second;

            // set sType
            if (lmAtom.first == factory->fFact) {
                lma->type = fact;
                lma->nameStr = htn->factStrs[lma->lm];
//                cout << "fact: " << htn->factStrs[lma->lm] << endl;
            } else {
                if (lma->lm >= htn->numActions) {
                    lma->type = task;
                } else {
                    lma->type = action;
                }

                lma->nameStr = htn->taskNames[lma->lm];
//                cout << "task: " << htn->taskNames[lma->lm] << endl;
            }

            if ((oneAtom.rfind("Atom") != 0) && (oneAtom.rfind("NegatedAtom") != 0)) {
                cout << "ERROR: the landmark type " << oneAtom << " returned by the FD translation is not supported. [TERMINATING]" << endl;
                exit(-1);
            }
            if (oneAtom.rfind("NegatedAtom") == 0) {
                lma->isNegated = true;
            }
        }
    }

    //
    // (2) creating ordering relations
    //
    g->initOrderings();
    for (int i = 0; i < tOrderings.size(); i++) {
        int* ordering = tOrderings[i];

        lmType typeOfFirst1 = g->lms[ordering[0]].lm[0]->type;
        bool allTheSame1 = true;
//        bool containsAbstract1 = false;
        for (int j = 0; j < g->lms[ordering[0]].lm.size(); j++) {
            if (g->lms[ordering[0]].lm[j]->type != typeOfFirst1) {
                allTheSame1 = false;
                break;
            }
//            if ((g->lms[ordering[0]].lm[j]->type == task) && (g->lms[ordering[0]].lm[j]->lm >= htn->numActions)) {
//                containsAbstract1 = true;
//                break;
//            }
        }
        lmType typeOfFirst2 = g->lms[ordering[1]].lm[0]->type;
        bool allTheSame2 = true;
//        bool containsAbstract2 = false;
        for (int j = 0; j < g->lms[ordering[1]].lm.size(); j++) {
            if (g->lms[ordering[1]].lm[j]->type != typeOfFirst2) {
                allTheSame2 = false;
                break;
            }
//            if ((g->lms[ordering[1]].lm[j]->type == task) && (g->lms[ordering[1]].lm[j]->lm >= htn->numActions)) {
//                containsAbstract2 = true;
//                break;
//            }
        }
        if (!allTheSame1 || !allTheSame2) {
            cout << "WARNING: Problem with lm ordering, mixed LM types" << endl;
            continue;
        } else {
            bool isAbstract1 = (typeOfFirst1 == task);
            bool isAction1   = (typeOfFirst1 == action);
            bool isFact1     = (typeOfFirst1 == fact);
            bool isAbstract2 = (typeOfFirst2 == task);
            bool isAction2   = (typeOfFirst2 == action);
            bool isFact2     = (typeOfFirst2 == fact);
            if (!(isAbstract1 || isFact1 || isAction1)) {
                cout << "WARNING: other LM type" << endl;
            }
            if (!(isAbstract2 || isFact2 || isAction2)) {
                cout << "WARNING: other LM type" << endl;
            }

            if ((isFact1 || isAction1) && (isFact2 || isAction2)) {
                g->addOrdering(ordering[0], ordering[1], (LMORD) ordering[2]);
            } else if ((isAbstract1 || isAction1) && (isAbstract2 || isAction2)) {
                g->addOrdering(ordering[1], ordering[0], (LMORD) ordering[2]);
            } else {
                cout << "WARNING: Discarded LM ordering between";
                if (isAbstract1) {
                    cout << " Abstract ";
                } else if (isAction1) {
                    cout << " Action ";
                } else if (isFact1) {
                    cout << " Fact ";
                } else {
                    cout << " ??? ";
                }
                 cout << "and";
                if (isAbstract2) {
                    cout << " Abstract";
                } else if (isAction2) {
                    cout << " Action";
                } else if (isFact2) {
                    cout << " Fact";
                } else {
                    cout << " ???";
                }
                cout << endl;
            }

//            bool invert = false;
////            bool invert = (isAbstract1) || (isAbstract2);
////            bool invert = (containsAbstract1) || (containsAbstract2);
//            if (invert) {
//                g->addOrdering(ordering[1], ordering[0], (LMORD) ordering[2]);
//            } else {
//                g->addOrdering(ordering[0], ordering[1], (LMORD) ordering[2]);
//            }
        }
        delete[] tOrderings[i];
    }
    return g;
}

void LmFdConnector::printDOT(progression::Model *htn, lmGraph* g) {
    ofstream dotfile;
    dotfile.open ("graph.dot");
    dotfile << "digraph G {" << endl;
    for (int i = 0; i < g->numLMs; i++) {
        dotfile << "  lm" << i << " [label=\"" << toString2(htn, g->lms[i]) << "\"];" << endl;;
    }
    for (int i = 0; i < g->numLMs; i++) {
        for (int j = 0; j < g->succNat[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << g->succNat[i][j] << " [label=\"nat\"];" << endl;
        }
        for (int j = 0; j < g->succGredNec[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << g->succGredNec[i][j] << " [label=\"grednec\"];" << endl;
        }
        for (int j = 0; j < g->succResonable[i].size(); j++) {
            dotfile << "  lm" << i << " -> lm" << g->succGredNec[i][j] << " [label=\"reason\"];" << endl;
        }
    }
    dotfile << "}" << endl;
    dotfile.close();
    system("xdot graph.dot");
}

string LmFdConnector::toString2(Model *m, lmNode &lms) {
    string res = "";
    bool first = true;
    if (lms.lm.size() > 1) {res.append("(");}
    for (auto atom : lms.lm) {
        if (atom->isNegated) res.append("¬");
        if (atom->type == task) {
            res.append(m->taskNames[atom->lm]);
        } else if (atom->type == fact) {
            res.append(m->factStrs[atom->lm]);
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

    string LmFdConnector::revertRenaming(string s) {
        std::string str = s;
        std::replace(str.begin(), str.end(), '(', '[');
        std::replace(str.begin(), str.end(), ')', ']');

        if (str == "<none of those>") {
            return "none-of-them";
        }
        return str;
    }

    int LmFdConnector::getNodeID(string &strID) {
        if (strID.find("lm") != 0) {
            cout << "ERROR parsing LM \"" << strID << "\"" << endl;
            exit(-1);
        } else {
            string temp = strID.substr(2);
            return stoi(temp);
        }
    }

} /* namespace progression */
