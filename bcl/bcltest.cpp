#include <iostream>
#include "xprb_cpp.h"
using namespace std;
using namespace ::dashoptimization;

namespace bcltest {

	void runBclTest() {
		#define NSHARES 10 // Number of shares
		#define NRISK 5 // Number of high-risk shares
		#define NNA 4 // Number of North-American shares
		double RET[] = {5,17,26,12,8,9,7,6,31,21}; // Estimated return in investment
		int RISK[] = {1,2,3,8,9}; // High-risk values among shares
		int NA[] = {0,1,2,3}; // Shares issued in N.-America
		int s;
		XPRBprob p("FolioLP"); // Initialize a new problem in BCL
		XPRBexpr Risk,Na,Return,Cap;
		XPRBvar frac[NSHARES]; // Fraction of capital used per share
		// Create the decision variables
		for(s=0;s<NSHARES;s++) frac[s] = p.newVar("frac");
		// Objective: total return
		for(s=0;s<NSHARES;s++) Return += RET[s]*frac[s];
		p.setObj(Return); // Set the objective function
		// Limit the percentage of high-risk values
		for(s=0;s<NRISK;s++) Risk += frac[RISK[s]];
		p.newCtr("Risk", Risk <= 1.0/3);
		// Minimum amount of North-American values
		for(s=0;s<NNA;s++) Na += frac[NA[s]];
		p.newCtr("NA", Na >= 0.5);
		// Spend all the capital
		for(s=0;s<NSHARES;s++) Cap += frac[s];
		p.newCtr("Cap", Cap == 1);
		// Upper bounds on the investment per share
		for(s=0;s<NSHARES;s++) frac[s].setUB(0.3);
		// Solve the problem
		p.setSense(XPRB_MAXIM);
		p.lpOptimize("");
		// Solution printing
		cout << "Total return: " << p.getObjVal() << endl;
		for(s=0;s<NSHARES;s++)
		cout << s << ": " << frac[s].getSol()*100 << "%" << endl;
		return;
	}
}
