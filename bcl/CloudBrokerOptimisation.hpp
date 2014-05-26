/*
 * CloudBrokerModel.h
 *
 *  Created on: Mar 20, 2014
 *      Author: sindremo
 */

#ifndef CLOUDBROKERMODEL_H_
#define CLOUDBROKERMODEL_H_

#include "entities.h"
#include "CloudBrokerModel.hpp"
#include <list>
#include <vector>
#include <iostream>

#define CG_BRUTEFORCE 1
#define CG_HEURISTIC_A 2
#define CG_HEURISTIC_B 3

namespace cloudbrokeroptimisation {

	class CloudBrokerOptimiser {
	private:
		CloudBrokerModel model;
		entities::dataContent * data;
	public:
		CloudBrokerOptimiser(entities::dataContent * data, double beta_backupres = 0.3, bool dedicated = false);
		void Solve(bool enforce_integer = true, int time_limit = 0, const char *lp_alg = " ");
		void RunColumnGeneration(int cg_alg, int cg_maxiters, int cg_maxcount, const char *lp_alg = " ");
		void OutputResultsToStream(std::ostream& stream);
	};
}
#endif /* CLOUDBROKERMODELS_H_ */
