/*
 * CloudBrokerModel.h
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

		std::vector< ::dashoptimization::XPRBvar > 					y_serveCustomerVars; 	/* for every customer*/
		std::vector< std::vector< ::dashoptimization::XPRBvar > > 	l_servicesOverlapVars;	/* for every pair of two customers */
		std::vector< ::dashoptimization::XPRBvar>					d_arcBackupUsage;		/* for every arc */
		std::list< ::dashoptimization::XPRBvar >					w_useMappingVars;		/* for every service, for every mapping of service */

		std::vector< ::dashoptimization::XPRBctr> 								 serveCustomerCtr;	/* for every service of customer */
		std::vector< ::dashoptimization::XPRBctr> 								 arcCapacityCtr;	/* for every arc */
		std::vector< ::dashoptimization::XPRBctr> 								 backupSumCtr;		/* for every arc */
		std::vector< std::vector< ::dashoptimization::XPRBctr> > 				 backupSingleCtr;	/* for every arc, for every service */
		std::vector< std::vector< std::vector < ::dashoptimization::XPRBctr> > > primaryOverlapCtr;	/* for every arc, for every pair of services */
		std::vector< std::vector< std::vector < ::dashoptimization::XPRBctr> > > backupOverlapCtr;	/* for every arc, for every pair of services */

		entities::dataContent *data;

		double beta;

		struct dual_vals {
					std::vector<double> 				serveCustomerDuals;	/* every customer, every service of customer */
					std::vector<double> 				arcCapacityDuals;	/* every arc */
					std::vector<double> 				backupSumDuals;		/* every arc */
					std::vector<std::vector<double> >	backupSingleDuals;	/* every arc, every service */
					std::vector<std::vector<std::vector<double> > >	primaryOverlapDuals;/* every arc, every pair of services */
					std::vector<std::vector<std::vector<double> > >	backupOverlapDuals;	/* every arc, every pair of services */

				};

		dual_vals getDualVals();

		::dashoptimization::XPRBvar* mappingVarForMappingIndex(int mappingIndex);

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
