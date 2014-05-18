/*
 * CloudBrokerModel.h
 *
 *  Created on: Mar 20, 2014
 *      Author: sindremo
 */

#ifndef CLOUDBROKERMODEL_H_
#define CLOUDBROKERMODEL_H_

#include "entities.h"
#include "xprb_cpp.h"
#include "xprs.h"
#include <list>
#include <vector>
#include <iostream>

#define CG_BRUTEFORCE 1
#define CG_HEURISTIC_A 2
#define CG_HEURISTIC_B 3

namespace cloudbrokeroptimisation {

	struct dual_vals {
		std::vector<double> 				serveCustomerDuals;	/* every customer, every service of customer */
		std::vector<double> 				arcCapacityDuals;	/* every arc */
		std::vector<double> 				backupSumDuals;		/* every arc */
		std::vector<std::vector<double> >	backupSingleDuals;	/* every arc, every service */
		std::vector<std::vector<std::vector<double> > >	primaryOverlapDuals;/* every arc, every pair of services */
		std::vector<std::vector<std::vector<double> > >	backupOverlapDuals;	/* every arc, every pair of services */
	};

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

		dual_vals getDualVals();

		::dashoptimization::XPRBvar* mappingVarForMappingIndex(int mappingIndex);

		void addMappingToModel(entities::mapping *m, entities::service *s);

		/**** BRUTE FORCE HEURISTIC ****/
		bool generateMappingColumnBruteForce(entities::service *s, dual_vals *duals);
		double _bruteForceEvalMapping(entities::mapping *m, entities::service *s, dual_vals *duals);

		/**** HEURISTIC A ****/
		bool generateMappingHeuristicA(entities::customer *c, entities::service *s, dual_vals *duals);
		std::vector<double> _dualPrimaryArcCostsForService(entities::service *s, dual_vals *duals);
		std::vector<double> _dualBackupArcCostsForService(entities::service *s, dual_vals *duals, entities::returnPath *primary);
		entities::returnPath _returnPathFromArcs(std::list<entities::arc*>* arcs, entities::service* owner, entities::placement* p);
		double _spprc(int n_nodes, int start_node, int end_node,
									std::vector<std::vector<entities::arc*> > *node_arcs,
									std::vector<double> *arc_costs,
									std::vector<int> *arc_restrictions,
									double max_latency,
									int max_restricted_arcs,
									double min_availability,
									std::list<entities::arc*>* used_arcs);

		/**** HEURISTIC B (extension of A) ****/
		bool generateMappingHeuristicB(entities::customer *c, entities::service *s, dual_vals *duals);

	public:
		CloudBrokerModel();
		void BuildModel(entities::dataContent * data, double beta_backupres = 0.3);
		void RunModel(bool enforce_integer = true, int time_limit = 0, const char *lp_alg = " ");
		void RunColumnGeneration(int cg_alg, int cg_maxiters, int cg_maxcount, const char *lp_alg = " ");
		void OutputResultsToStream(std::ostream& stream);
	};
}
#endif /* CLOUDBROKERMODELS_H_ */
