/*
 * CloudBrokerModel.hpp
 *
 *  Created on: May 25, 2014
 *      Author: sindremo
 */

#ifndef CLOUDBROKERMODEL_HPP_
#define CLOUDBROKERMODEL_HPP_

#include "entities.h"
#include "xprb_cpp.h"
#include "xprs.h"

#include <iostream>

namespace cloudbrokeroptimisation {

	struct dual_vals {
		std::vector<double> 				serveCustomerDuals;	/* every customer, every service of customer */
		std::vector<double> 				arcCapacityDuals;	/* every arc */
		std::vector<double> 				backupSumDuals;		/* every arc */
		std::vector<std::vector<double> >	backupSingleDuals;	/* every arc, every service */
		std::vector<std::vector<std::vector<double> > >	primaryOverlapDuals;/* every arc, every pair of services */
		std::vector<std::vector<std::vector<double> > >	backupOverlapDuals;	/* every arc, every pair of services */
	};

	class Parameters {
		public:
			static double U_PrimaryBandwidthUsageOnArcForMapping(entities::arc *a, entities::mapping *m);
			static double Q_BackupBandwidthUsageOnArcForMapping(entities::arc *a, entities::mapping *m);
			static double R_RevenueForCustomer(entities::customer *c);
			static double E_PrimaryPathCost(entities::returnPath *k);
			static double E_PerBandwidthCostForArc(entities::arc *a);
			static double F_BandwidthCapacityForArc(entities::arc *a);
	};

	class CloudBrokerModel {
		private:
			/*	MODEL PROBLEM OBJECT		*/
			::dashoptimization::XPRBprob master_problem;

			/*	MODEL OBJECTIVE FUNCTION	*/
			::dashoptimization::XPRBctr z_objective;

			/*  MODEL VARIABLES 			*/
			std::vector< ::dashoptimization::XPRBvar > 					y_serveCustomerVars; 	/* for every customer*/
			std::vector< std::vector< ::dashoptimization::XPRBvar > > 	l_servicesOverlapVars;	/* for every pair of two customers */
			std::vector< ::dashoptimization::XPRBvar>					lambda_arcBackupRes;	/* for every arc */
			std::list< ::dashoptimization::XPRBvar >					w_useMappingVars;		/* for every service, for every mapping of service */

			/*	MODEL CONSTRAINTS 			*/
			std::vector< ::dashoptimization::XPRBctr> 								 serveCustomerCtr;	/* for every service of customer */
			std::vector< ::dashoptimization::XPRBctr> 								 arcCapacityCtr;	/* for every arc */
			std::vector< ::dashoptimization::XPRBctr> 								 backupSumCtr;		/* for every arc */
			std::vector< std::vector< ::dashoptimization::XPRBctr> > 				 backupSingleCtr;	/* for every arc, for every service */
			std::vector< std::vector< std::vector < ::dashoptimization::XPRBctr> > > primaryOverlapCtr;	/* for every arc, for every pair of services */
			std::vector< std::vector< std::vector < ::dashoptimization::XPRBctr> > > backupOverlapCtr;	/* for every arc, for every pair of services */

			/*	PROBLEM DATA				*/
			entities::dataContent *data;

			/*	PROBLEM CONFIG 				*/
			double beta;
			bool dedicated;

			/*  PROBLEM SOLVE STATE			*/
			::dashoptimization::XPRBbasis basis;
			bool basis_stored;

			/**** Helper functions ****/
			::dashoptimization::XPRBvar* mappingVarForMappingIndex(int mappingIndex);


		public:
			CloudBrokerModel();
			void BuildModel(entities::dataContent * data, double beta_backupres = 0.3, bool dedicated = false);
			void RunModel(bool enforce_integer = true, int time_limit = 0, const char *lp_alg = " ");
			void OutputResultsToStream(std::ostream& stream);

			dual_vals GetDualVals();	// dual values of solved model
			double GetBeta();			// beta value used to build this model
			void AddMappingToModel(entities::mapping *m, entities::service *s);

			double GetObjectiveValue();	// value of models objective function
			void SaveBasis();			// attempts to store current basis
			bool LoadSavedBasis();		// attempts to load previously stored basis (returned bool determines success)
			void SetColumnGenerationConfiguration(bool active);	// sets solve configuration to column generation applicable config or not depending on input
		};

}

#endif /* CLOUDBROKERMODEL_HPP_ */
