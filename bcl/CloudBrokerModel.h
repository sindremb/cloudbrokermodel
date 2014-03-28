/*
 * bcltest.h
 *
 *  Created on: Mar 20, 2014
 *      Author: sindremo
 */

#ifndef CLOUDBROKERMODELS_H_
#define CLOUDBROKERMODELS_H_

#include "entities.h"
#include "xprb_cpp.h"
#include "xprs.h"
#include <list>
#include <vector>

namespace cloudbrokermodels {

	class CloudBrokerModel {
	private:
		::dashoptimization::XPRBprob master_problem;

		::dashoptimization::XPRBctr z_objective;

		std::list< ::dashoptimization::XPRBvar> y_serveCustomerVars;
		std::list< ::dashoptimization::XPRBvar> l_servicesOverlapVars;
		std::list< ::dashoptimization::XPRBvar> d_arcBackupUsage;
		std::list< ::dashoptimization::XPRBvar> w_useMappingVars;

		std::list< ::dashoptimization::XPRBctr> serveCustomerCtr;	/* every customer, every service of customer */
		std::list< ::dashoptimization::XPRBctr> arcCapacityCtr;		/* every arc */
		std::list< ::dashoptimization::XPRBctr> backupSumCtr;		/* every arc */
		std::list< ::dashoptimization::XPRBctr> backupSingleCtr;	/* every arc, every service */
		std::list< ::dashoptimization::XPRBctr> primaryOverlapCtr;	/* every arc, every pair of services */
		std::list< ::dashoptimization::XPRBctr> backupOverlapCtr;	/* every arc, every pair of services */

		entities::dataContent *data;

		::dashoptimization::XPRBvar* mappingVarForMappingNumber(int mappingNumber);

		struct dual_vals {
					std::vector<double> serveCustomerDuals;	/* every customer, every service of customer */
					std::vector<double> arcCapacityDuals;	/* every arc */
					std::vector<double> backupSumDuals;		/* every arc */
					std::vector<double> backupSingleDuals;	/* every arc, every service */
					std::vector<double> primaryOverlapDuals;/* every arc, every pair of services */
					std::vector<double> backupOverlapDuals;	/* every arc, every pair of services */

				};

		dual_vals getDualVals();

		void addMappingToModel(entities::mapping *m, entities::service *s);

		double _bruteForceEvalMapping(entities::mapping *m, entities::service *s, dual_vals *duals);
		bool generateMappingColumnBruteForce(entities::service *s, dual_vals *duals);


	public:
		CloudBrokerModel();
		void BuildModel(entities::dataContent * data, double beta_backupres = 0.3);
		void AddMapping(int serviceNumber, entities::mapping * m);
		void RunModel(bool enforce_integer = true);
		void RunModelColumnGeneration();
		void OutputResults();
	};
}
#endif /* CLOUDBROKERMODELS_H_ */
