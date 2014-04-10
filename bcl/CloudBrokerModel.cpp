#include "CloudBrokerModel.h"
#include "entities.h"

#include "xprb_cpp.h"
#include "xprs.h"

#include <list>
#include <iostream>
#include <algorithm>
#include <iostream>

#ifndef EPS
#define EPS 1e-6
#endif

#ifndef Inf
#define Inf 1e10
#endif

using namespace entities;
using namespace ::dashoptimization;
using namespace std;

namespace cloudbrokermodels {

	/*
	 * TEMPORARILY STOLEN FROM PATHGENERATOR: should make classes from entities, and add as method to returnPath ??
	 * Calculates the P(A)P(B|A) availability term for paths *a and *b, wrapped in a pathCombo struct
	 */
	pathCombo _pathComboForPaths(returnPath *a, returnPath *b) {
		pathCombo combo;
		combo.a = a;
		combo.b = b;
		combo.exp_b_given_a = a->exp_availability; // intitial: P(A)

		// *P(B|A)
		// -  find all arcs unique to *b
		vector<arc*> unique;
		for (list<arc*>::const_iterator i = b->arcs_up.begin(), end = b->arcs_up.end(); i != end; ++i) {
			bool found = false;
			for (list<arc*>::const_iterator j = a->arcs_up.begin(), end = a->arcs_up.end(); j != end; ++j) {
				if(*i == *j) {
					found = true;
					break;
				}
			}
			if(!found) {
				unique.push_back(*i);
			}
		}
		// - multiply with P(B|A) by multiplying availability of all unique arcs
		for(unsigned int i = 0; i < unique.size(); ++i) {
			combo.exp_b_given_a *= unique[i]->exp_availability;
		}

		return combo;
	}

	/****** Parameters: ******
	 * - Utility class used to translate data entities to model parameters
	 * 	 Note: implementation at bottom of source file
	 */
	class Parameters {
		public:
			static double U_PrimaryBandwidthUsageOnArcForMapping(arc *a, mapping *m);
			static double Q_BackupBandwidthUsageOnArcForMapping(arc *a, mapping *m);
			static double R_RevenueForCustomer(customer *c);
			static double E_PrimaryPathCost(returnPath *k);
			static double E_PerBandwidthCostForArc(arc *a);
			static double F_BandwidthCapacityForArc(arc *a);
	};

	/********** Label struct ************
	 * Data structure for 'labels' used in the labeling algorithm for solving the
	 * SPPRC in heuristic A for column generation
	 */
	struct label {
		double cost;
		double latency;
		double availability;
		int restricted_arcs_count;
		int end_node;
		arc *last_arc;
		label *parent;
		bool isdominated;
	};

	/* CloudBrokerModel constructor
	 * - Includes necessary class member initialisations
	 */
	CloudBrokerModel::CloudBrokerModel() :
			/* --- class member initialisations ---- */
			master_problem("Cloud Broker Optimisation")
	{

	}

	/******  BuildModel *******
	 * params:
	 * - dataContent* data: pointer to data to build model from
	 * - double beta_backupres: min fraction of total backup requirement to reserve on arc
	 *
	 * Builds the MIP-model from the provided dataContent and beta model parameter
	 */
	void CloudBrokerModel::BuildModel(dataContent * input_data, double beta_backupres) {

		/********* SETUP **********/

		this->data = input_data;
		this->beta = beta_backupres;

		/* w variable iterator */
		list<XPRBvar>::iterator w_itr;

		/********** CREATE VARIABLES **********/

		cout << " - creating variables..\n";

		/* Serve Customer variables
		 * for: every customer
		 */
		y_serveCustomerVars.reserve(data->n_customers);
		for(int cc = 0; cc < data->n_customers; cc++) {
			y_serveCustomerVars.push_back(
				master_problem.newVar(XPRBnewname("y_serve_customer_%d", cc+1), XPRB_BV, 0, 1)
			);
		}
		cout << "   - created " << y_serveCustomerVars.size() << " y-variables\n";

		/* Use Mapping variables
		 * for: every mapping
		 * [shorthand for:
		 * 	for: every customer
		 *	 for: every service of customer
		 *	  for: every mapping of service
		 * ]
		 */
		for(int mm = 0; mm < data->n_mappings; mm++) {
			w_useMappingVars.push_back(
				master_problem.newVar(
					XPRBnewname("m_use_mapping_%d", mm+1),
					XPRB_BV, 0, 1
				)
			);
		}
		cout << "   - created " << w_useMappingVars.size() << " w-variables\n";

		/* Services overlap variables
		 * for: every pair of two services (s, t)
		 *      - assume services are ordered, each combination of services where s < t
		 */
		int l_count = 0;
		/* for every service s (except last service) */
		l_servicesOverlapVars.resize(data->n_services-1);
		for(int ss = 0; ss < data->n_services-1; ss++) {

			/* for every service t | t > s */
			l_servicesOverlapVars[ss].reserve(data->n_services-1-ss);
			for(int tt = ss+1; tt < data->n_services; tt++) {

				l_servicesOverlapVars[ss].push_back(
					master_problem.newVar(
						XPRBnewname("l_service_overlaps_%d_%d", ss+1, tt+1),
						XPRB_BV, 0, 1
					)
				);
				++l_count;
			}
		}
		cout << "   - created " << l_count << " l-variables\n";

		/* Arc Backup Usage variable
		 * for: every arc a
		 */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];
			d_arcBackupUsage.push_back(
				master_problem.newVar(
					XPRBnewname("d_backup_usage_%d_%d", a->startNode, a->endNode),
					XPRB_PL, 0, a->bandwidth_cap
				)
			);
		}
		cout << "   - created " << d_arcBackupUsage.size() << " d-variables\n";

		/******* CREATE OBJECTIVE FUNCTION ********/

		cout << " - creating objective function..";

		XPRBexpr z_obj_expression;

		/*	First term:
		 *  - sum revenue from served customers
		 */
		for(int cc = 0; cc < data->n_customers; cc++) {
			customer * c = &data->customers[cc];
			z_obj_expression += Parameters::R_RevenueForCustomer(c)*y_serveCustomerVars[cc];
		}

		/*	Second term:
		 *  - sum cost from all used mappings' primary paths
		 */
		w_itr = w_useMappingVars.begin();
		for(int cc = 0; cc < data->n_customers; cc++) {
			customer * c = &data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ss++) {
				service * s = c->services[ss];
				for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
					z_obj_expression -= Parameters::E_PrimaryPathCost((*m_itr)->primary) * (*w_itr);
					++w_itr;
				}
			}
		}

		/* Third term:
		 * - sum costs of total reserved capacity for backup paths on all arcs
		 */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];
			z_obj_expression -= Parameters::E_PerBandwidthCostForArc(a)*d_arcBackupUsage[aa];
		}

		/* create final objective function */
		z_objective = master_problem.newCtr("OBJ", z_obj_expression);
		/* set objective function for problem */
		master_problem.setObj(z_objective);

		/********************* CREATE CONSTRAINTS *************************/

		cout << "DONE!\n - creating constraints..\n";

		/* SERVE CUSTOMER CONSTRAINTS
		 * - If customer is to be served - and generate revenue - all services of that customer
		 *   must be assigned a mapping
		 * for: every customer
		 * 	for: every service of customer
		 */
		cout << "  - SERVE CUSTOMER CONSTRAINTS..";
		w_itr = w_useMappingVars.begin();
		serveCustomerCtr.reserve(data->n_services);
		/* for every customer */
		for(int cc = 0; cc < data->n_customers; ++cc) {
			customer * c = &data->customers[cc];

			/* for every service of customer */
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service * s = c->services[ss];

				/* NEW CONSTRAINT: lhs expression */
				XPRBexpr map_service_expr;

				/* for every mapping of service*/
				for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {

					/* add mapping selection var */
					map_service_expr += (*w_itr);

					++w_itr;
				}

				/* subtract serve customer var */
				map_service_expr -= y_serveCustomerVars[cc];

				/* create final constraint */
				serveCustomerCtr.push_back(
					master_problem.newCtr(
							XPRBnewname("serve_customer_ctr_%d", s->globalServiceIndex+1),
							map_service_expr == 0.0
					)
				);
			}
		}
		cout << "DONE! (" << serveCustomerCtr.size() << " created) \n";

		/* ARC CAPACITY CONSTRAINT
		 * - the sum of capacity used by chosen primary paths over link a and bandwidth reserved for
		 *   backup must not exceed the capacity of the arc
		 *   for: every arc a
		 */
		cout <<	"  - ARC CAPACITY CONSTRAINTS..";

		/* for every arc */
		arcCapacityCtr.reserve(data->n_arcs);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			/* NEW CONSTRAINT: lhs expression*/
			XPRBexpr arc_bw_usage;

			/* sum bandwidth use from all used mappings' primary paths */
			w_itr = w_useMappingVars.begin();
			for(int cc = 0; cc < data->n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = c->services[ss];
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;

						/* add bandwidth used by used mappings on arc */
						arc_bw_usage += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a,m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* add bandwidth reserved for backup paths on arc */
			arc_bw_usage += d_arcBackupUsage[aa];

			/* create final constraint: arc bandwidth usage <= arc capacity */
			arcCapacityCtr.push_back(
				master_problem.newCtr(
					XPRBnewname("arc_capacity_ctr_%d_%d", a->startNode, a->endNode),
					arc_bw_usage <= Parameters::F_BandwidthCapacityForArc(a)
				)
			);
		}
		cout << "DONE! (" << arcCapacityCtr.size() << " created) \n";

		/* SINGLE BACKUP RESERVATION CONSTRAINT
		 * - backup capacity reserved on an arc must be large enough to support any single backup
		 *   path using that arc
		 *
		 *   for: every arc
		 *   	for: every customer
		 *   		for: every customer's servicek
		 */
		cout <<	"  - SINGLE BACKUP RESERVATION CONSTRAINTS..";

		/* for every arc		*/
		backupSingleCtr.resize(data->n_arcs);
		int backupSingleCount = 0;
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			/* for every service */
			backupSingleCtr[aa].reserve(data->n_customers);
			w_itr = w_useMappingVars.begin();
			for(int cc = 0; cc < data->n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {	/* for every service 	*/
					service * s = c->services[ss];

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr service_backup_req_on_arc;

					/* for every mapping of service */
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;

						/* add backup requirement on arc for used mapping */
						service_backup_req_on_arc += (Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m))*(*w_itr);

						++w_itr;
					}

					/* subtract arc backup usage variable */
					service_backup_req_on_arc -= d_arcBackupUsage[aa];

					/* create final constraint */
					backupSingleCtr[aa].push_back(
						master_problem.newCtr(
							XPRBnewname("backup_single_ctr_%d_%d_%d", a->startNode, a->endNode, s->globalServiceIndex+1),
								service_backup_req_on_arc <= 0.0
						)
					);
					++backupSingleCount;
				}
			}
		}
		cout << "DONE! (" << backupSingleCount << " created) \n";

		/* SUM BACKUP RESERVATION CONSTRAINT
		 * - a proportion of total backup requirement on an arc must be reserved
		 * for: every arc
		 */
		cout << "  - SUM BACKUP RESERVATION CONSTRAINTS..";

		/* for every arc */
		backupSumCtr.reserve(data->n_arcs);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			/* NEW CONSTRAINT: lhs expression*/
			XPRBexpr total_backup_req_expr;

			/* for every mapping */
			w_itr = w_useMappingVars.begin();
			for(int cc = 0; cc < data->n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = c->services[ss];
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;

						/* add backup bandwidth req on arc for used mappings (multiplied with beta factor) */
						total_backup_req_expr += beta*Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* subtract backup reservation on arc variable*/
			total_backup_req_expr-= d_arcBackupUsage[aa];

			/* create final constraint */
			backupSumCtr.push_back(
				this->master_problem.newCtr(
					XPRBnewname("backup_sum_ctr_%d_%d", a->startNode, a->endNode),
					total_backup_req_expr <= 0.0
				)
			);
		}
		cout << "DONE! (" << this->backupSumCtr.size() << " created)\n";

		/* PRIMARY OVERLAP CONSTRAINT
		 * - if for any arc, the chosen mappings of two services uses that arc, the
		 *   two services are said to overlap
		 *
		 *   for: every pair of two services services
		 */
		cout << "  - PRIMARY OVERLAP CONSTRAINTS..";
		int primaryOverlapCtrCount = 0;
		/* for every arc a */
		primaryOverlapCtr.resize(data->n_arcs);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs[aa];
			primaryOverlapCtr[aa].resize(data->n_services-1);

			/* for every pair of two services (s, t) */
			for(int ss = 0; ss < data->n_services-1; ++ss) {
				service *s = &data->services[ss];
				primaryOverlapCtr[aa][ss].reserve(data->n_services-1-ss);
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					service *t = &data->services[tt];

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr primary_overlap_expr;

					/* sum over all mappings for service s using arc a */
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;
						if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) >= EPS) {
							primary_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* sum over all mappings for service t using arc a */
					for (list<mapping*>::iterator m_itr = t->mappings.begin(), m_end = t->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;
						if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) >= EPS) {
							primary_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* subtract primary overlap var for service pair (s, t)*/
					primary_overlap_expr -= l_servicesOverlapVars[ss][tt-ss-1];

					/* create final constraint */
					primaryOverlapCtr[aa][ss].push_back(
						master_problem.newCtr(
							XPRBnewname("primary_overlap_ctr_%d_%d", a->startNode, a->endNode),
							primary_overlap_expr <= 1.0
						)
					);
					++primaryOverlapCtrCount;
				}
			}
		}
		cout << "DONE! (" << primaryOverlapCtrCount << " created)\n";

		/* BACKUP OVERLAP CONSTRAINT
		 * - if two services' primary paths overlap, their backup paths can not have bandwidth requirements at
		 *   the same arc
		 *
		 *   for: every pair of two services services
		 */
		cout << "  - BACKUP OVERLAP CONSTRAINTS..";
		int backupOverlapCtrCount = 0;
		/* for every arc a */
		backupOverlapCtr.resize(data->n_arcs);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs[aa];
			backupOverlapCtr[aa].resize(data->n_services-1);

			/* for every pair of two services (s, t) */
			for(int ss = 0; ss < data->n_services-1; ++ss) {
				service *s = &data->services[ss];
				backupOverlapCtr[aa][ss].reserve(data->n_services-1-ss);
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					service *t = &data->services[tt];

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr backup_overlap_expr;

					/* sum over all mappings for service s using arc a */
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;
						if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) >= EPS) {

							/* add mapping selection var for mapping */
							backup_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* sum over all mappings for service t using arc a */
					for (list<mapping*>::iterator m_itr = t->mappings.begin(), m_end = t->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;
						if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) >= EPS) {

							/* add mapping selection var for mapping */
							backup_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* add primary overlap var for service pair (s, t) */
					backup_overlap_expr += l_servicesOverlapVars[ss][tt-ss-1];

					/* create final constraint */
					this->backupOverlapCtr[aa][ss].push_back(
						this->master_problem.newCtr(
							XPRBnewname("backup_overlap_ctr_%d_%d", a->startNode, a->endNode),
							backup_overlap_expr <= 2.0
						)
					);
					++backupOverlapCtrCount;
				}
			}
		}
		cout << "DONE! (" << backupOverlapCtrCount  << " created)\n";

		// set problem to maximise objective
		master_problem.setSense(XPRB_MAXIM);

		return;
	}

	void CloudBrokerModel::RunModel(bool enforce_integer, int time_limit) {
		if(time_limit) {
			XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_MAXTIME, time_limit);
		}

		if(enforce_integer) {
			master_problem.mipOptimise();
		} else {
			master_problem.lpOptimise();
		}
	}

	void CloudBrokerModel::RunModelColumnGeneration(int columnGenerationMethod, int iter_limit, int col_count_limit, int mip_time_limit) {
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_CUTSTRATEGY, 0);	/* Disable automatic cuts */
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_PRESOLVE, 0);		/* Switch presolve off */
		master_problem.setMsgLevel(1);											/* disable default XPRS messages */

		int itercount = 0;
		while((iter_limit < 0 || itercount < iter_limit)
				&& (col_count_limit < 0 || data->n_mappings < col_count_limit)) {
			bool foundColumn = false;
			cout << "\nIteration " << itercount+1 << ":\n-running lp-relaxation..\n";
			this->RunModel(false, -1);
			XPRBbasis basis = master_problem.saveBasis();

			CloudBrokerModel::dual_vals duals = this->getDualVals();

			cout << "-generating columns..\n";
			// use duals to generate column(s) for each service
			for(int cc = 0; cc < this->data->n_customers; ++cc) {
				customer *c = &this->data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				//for(unsigned int ss = 0; ss < 1; ++ss) {
					service *s = c->services[ss];
					if(columnGenerationMethod == 1) {
						if(this->generateMappingColumnBruteForce(s, &duals)) foundColumn = true;
					} else if(columnGenerationMethod == 2) {
						if(this->generateMappingHeuristicA(c, s, &duals)) foundColumn = true;
					} else if(columnGenerationMethod == 3) {
						if(this->generateMappingHeuristicB(c, s, &duals)) foundColumn = true;
					}
				}
			}

			if(!foundColumn) break; /* no new column was found -> end column generation */

			master_problem.loadBasis(basis);
			++itercount;
		}

		cout << "added " << w_useMappingVars.size() << " mappings in total!\n";

		cout << "LP-optimum: " << master_problem.getObjVal() << "\n";


		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_CUTSTRATEGY, 1);	/* Reenable automatic cuts */
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_PRESOLVE, 1);		/* Switch presolve on again */
		master_problem.setMsgLevel(0);
		cout << "running MIP-model..\n";
		this->RunModel(true, mip_time_limit);
	}

	CloudBrokerModel::dual_vals CloudBrokerModel::getDualVals() {
		CloudBrokerModel::dual_vals duals;

		/************ serveCustomerCtr dual values **********/
		duals.serveCustomerDuals.resize(data->n_services, 0.0);
		for(int ss = 0; ss < data->n_services; ++ss) {
			duals.serveCustomerDuals[ss] = serveCustomerCtr[ss].getDual();
		}

		/********* all arc related dual values ************/
		// make room for dual values (first dimension)
		duals.arcCapacityDuals.resize(data->n_arcs, 0.0);
		duals.backupSumDuals.resize(data->n_arcs, 0.0);
		duals.backupSingleDuals.resize(data->n_arcs);
		duals.primaryOverlapDuals.resize(data->n_arcs);
		duals.backupOverlapDuals.resize(data->n_arcs);

		// for every arc
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			// dual val for arcCapacityCtr for this arc
			duals.arcCapacityDuals[aa] = arcCapacityCtr[aa].getDual();

			// dual val for backupSumCtr for this arc
			duals.backupSumDuals[aa] = backupSumCtr[aa].getDual();

			// make room for dual values (second dimension)
			duals.backupSingleDuals[aa].resize(data->n_services, 0.0);
			duals.primaryOverlapDuals[aa].resize(data->n_services-1);
			duals.primaryOverlapDuals[aa].resize(data->n_services-1);
			duals.backupOverlapDuals[aa].resize(data->n_services-1);

			// for every service
			for(int ss = 0; ss < data->n_services; ++ss) {
				// dual val for backupSingleCtr for this arc and service
				duals.backupSingleDuals[aa][ss] = backupSingleCtr[aa][ss].getDual();
			}

			// for every pair of two services (s, t)
			// - for every first service s
			for(int ss = 0; ss < data->n_services-1; ++ss) {

				// make room for dual values (third dimension)
				duals.primaryOverlapDuals[aa][ss].resize(data->n_services-1-ss);
				duals.backupOverlapDuals[aa][ss].resize(data->n_services-1-ss);

				// - for every second service t
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					// dual val for primaryOverlapCtr for arc a and service pair (s, t)
					duals.primaryOverlapDuals[aa][ss][tt-ss-1] = primaryOverlapCtr[aa][ss][tt-ss-1].getDual();
					// dual val for backupOverlapCtr for arc a and service pair (s, t)
					duals.backupOverlapDuals[aa][ss][tt-ss-1] = backupOverlapCtr[aa][ss][tt-ss-1].getDual();
				}
			}
		}

		return duals;
	}

	void CloudBrokerModel::addMappingToModel(entities::mapping *m, entities::service *owner) {

		// increment number of mappings count, add a global number to mapping
		m->globalMappingIndex = data->n_mappings;
		data->mappings.push_back(*m);
		owner->mappings.push_back(&data->mappings.back());
		++data->n_mappings;

		// create new w variable
		w_useMappingVars.push_back(
			master_problem.newVar(
				XPRBnewname("m_use_mapping_%d", m->globalMappingIndex+1),
				XPRB_BV, 0, 1
			)
		);

		// pointer to the newly created w variable
		XPRBvar *w = &w_useMappingVars.back();

		// objective function
		z_objective -= Parameters::E_PrimaryPathCost(m->primary) * (*w);

		// add mapping var to service's 'serveCustomer'-constraint
		serveCustomerCtr[owner->globalServiceIndex] += *w;

		// for every arc
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			// get primary path usage on arc
			double u_primary_usage = Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m);
			// get backup usage on arc
			double q_backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);

			// add mapping primary path usage to arc's 'arcCap'-constraints
			arcCapacityCtr[aa] += u_primary_usage * (*w);

			// add mapping backup usage to arc's 'backupSingle'-constraint
			backupSingleCtr[aa][owner->globalServiceIndex] += q_backup_usage * (*w);

			// add mapping backup usage (*beta) to arc's 'backupSum'-constraint
			backupSumCtr[aa] += beta * q_backup_usage * (*w);

			// if primary path uses capacity on arc
			if(u_primary_usage >= EPS) {
				// add mapping selection var to 'primaryOverlap'-constraints for every service pair where owner service takes part
				int tt, ss;
				ss = owner->globalServiceIndex;
				for(tt = ss+1; tt < data->n_services; ++tt) {
					primaryOverlapCtr[aa][ss][tt-ss-1] += *w;
				}
				tt = owner->globalServiceIndex;
				for(ss = 0; ss < tt; ++ss) {
					primaryOverlapCtr[aa][ss][tt-ss-1] += *w;
				}

			}

			// if backup path uses capacity on arc
			if(q_backup_usage >= EPS) {
				// add mapping selection var to 'backupOverlap'-constraints for every service pair where owner service takes part
				int tt, ss;
				ss = owner->globalServiceIndex;
				for(tt = ss+1; tt < data->n_services; ++tt) {
					backupOverlapCtr[aa][ss][tt-ss-1] += *w;
				}
				tt = owner->globalServiceIndex;
				for(ss = 0; ss < tt; ++ss) {
					backupOverlapCtr[aa][ss][tt-ss-1] += *w;
				}

			}
		}
	}

	bool CloudBrokerModel::generateMappingColumnBruteForce(entities::service *s, dual_vals *duals){

		mapping bestFound;
		double best_eval = 0.0;

		for(unsigned int pp = 0; pp < s->possible_placements.size(); ++pp) {
			placement *p = &s->possible_placements[pp];
			for(unsigned int kk = 0; kk < p->paths.size(); ++kk) {
				returnPath *k = p->paths[kk];
				// check if feasible mapping alone
				if(k->exp_availability >= s->availability_req) {
					// path offers sufficient availability alone -> dont add backup path
					mapping m;
					m.primary = k;
					m.backup = NULL;
					double eval = this->_bruteForceEvalMapping(&m, s, duals);
					if(eval > best_eval) {
						best_eval = eval;
						bestFound = m;
					}
				}
				// OR try combining with other path to placement as backup
				else {
					// path does not offer sufficient availability -> look for possible backup paths
					for (unsigned int bb = 0; bb < p->paths.size(); ++bb) {
						returnPath * b = p->paths[bb];
						// calculate combo availability [ P(A)*P(B|A) ]
						pathCombo combo = _pathComboForPaths(k, b);
						if(k->exp_availability + b->exp_availability - combo.exp_b_given_a >= s->availability_req) {
							// combination of a as primary and b as backup is feasible -> add routing
							mapping m;
							m.primary = k;
							m.backup = b;
							double eval = this->_bruteForceEvalMapping(&m, s, duals);
							if(eval > best_eval) {
								best_eval = eval;
								bestFound = m;
							}
						}
					}
				}
			}
		}

		if(best_eval >= EPS) {
			this->addMappingToModel(&bestFound, s);
			return true;
		}
		return false;
	}

	double CloudBrokerModel::_bruteForceEvalMapping(mapping *m, service *owner, dual_vals *duals) {

		/**** c ****/
		double c = - m->primary->cost; // includes cost of placement and bandwidth usage on arcs

		/**** A^Ty: *****/
		double At_y = 0.0;

		//   - a_s
		// add dual cost from owner's serve customer constraint
		At_y += duals->serveCustomerDuals[owner->globalServiceIndex];


		/* for every arc */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			// get primary path usage on arc
			double u_primary_usage = Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m);
			// get backup usage on arc
			double q_backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);

			/* DUAL PRICE: TOTAL CAPACITY CTR (PRIMARY)
			 * add dual cost for use on this arc for this mapping
			 */
			At_y += u_primary_usage * duals->arcCapacityDuals[aa];

			/* DUAL PRICE: SUM BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping with beta factor
			 */
			At_y += beta * q_backup_usage * duals->backupSumDuals[aa];

			/* DUAL PRICE: SINGLE BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping
			 */
			At_y += q_backup_usage * duals->backupSingleDuals[aa][owner->globalServiceIndex];

			/* DUAL PRICE: PRIMARY OVERLAP CTR (PRIMARY)
			 * add overlap dual price if primary path uses arc for each service pair where owner takes part (usage > 0)
			 */
			if(u_primary_usage >= EPS) {
				/* for every service pair where owner service takes part */
				int tt, ss;
				ss = owner->globalServiceIndex;
				for(tt = ss+1; tt < data->n_services; ++tt) {
					At_y += duals->primaryOverlapDuals[aa][ss][tt-ss-1];
				}
				tt = owner->globalServiceIndex;
				for(ss = 0; ss < tt; ++ss) {
					At_y += duals->primaryOverlapDuals[aa][ss][tt-ss-1];
				}

			}

			/* DUAL PRICE: BACKUP OVERLAP CTR (BACKUP)
			 * add overlap dual price if backup path uses arc for each service pair where owner takes part (usage > 0)
			 */
			if(q_backup_usage >= EPS) {
				/* for every service pair where owner service takes part */
				int tt, ss;
				ss = owner->globalServiceIndex;
				for(tt = ss+1; tt < data->n_services; ++tt) {
					At_y += duals->backupOverlapDuals[aa][ss][tt-ss-1];
				}
				tt = owner->globalServiceIndex;
				for(ss = 0; ss < tt; ++ss) {
					At_y += duals->backupOverlapDuals[aa][ss][tt-ss-1];
				}
			}
		}

		return c - At_y;
	}

	/* Returns a list of costs for each arc in the loaded problem
	 * - The cost returned is the cost associated with choosing an arc in the up-link (thus indirectly
	 *   selecting it's return arc for the down-link.
	 */
	vector<double> CloudBrokerModel::_dualPrimaryArcCostsForService(entities::service *s, dual_vals *duals) {
		vector<double> arc_costs(data->n_arcs, 0.0);

		/* for every arc (with return arc) */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];
			arc * a_return = a->return_arc;
			int aa_return = a_return->globalArcIndex;

			/* REGULAR BANDWIDTH USAGE COST
			 * add cost from bandwidth on this arc arc in up-link and return arc in down-link
			 */
			arc_costs[aa] += a->bandwidth_price * s->bandwidth_req_up
					+ a_return->bandwidth_price * s->bandwidth_req_down;

			/* DUAL PRICE: TOTAL CAPACITY CTR (PRIMARY)
			 * add dual cost for use on this arc in up-link and its return arc in down-link
			 */
			arc_costs[aa] += s->bandwidth_req_up 	* duals->arcCapacityDuals[aa]
			               + s->bandwidth_req_down 	* duals->arcCapacityDuals[aa_return];

			/* DUAL PRICE: PRIMARY OVERLAP CTR (PRIMARY)
			 * add overlap dual price for this arc and return arc, for each service pair where owner takes part
			 */
			int tt, ss;
			ss = s->globalServiceIndex;
			for(tt = ss+1; tt < data->n_services; ++tt) {
				arc_costs[aa] += duals->primaryOverlapDuals[aa		 ][ss][tt-ss-1]
				               + duals->primaryOverlapDuals[aa_return][ss][tt-ss-1];
			}
			tt = s->globalServiceIndex;
			for(ss = 0; ss < tt; ++ss) {
				arc_costs[aa] += duals->primaryOverlapDuals[aa		 ][ss][tt-ss-1]
					           + duals->primaryOverlapDuals[aa_return][ss][tt-ss-1];
			}
		}

		return arc_costs;
	}

	vector<double> CloudBrokerModel::_dualBackupArcCostsForService(entities::service *s, dual_vals *duals, returnPath *primary) {
		vector<double> arc_costs(data->n_arcs, 0.0);

		// backup usage depends on primary usage -> extract primary usage for each arc
		vector<double> primary_usage(data->n_arcs, 0.0);
		for(list<arc*>::iterator a_itr = primary->arcs_up.begin(), a_end = primary->arcs_up.end(); a_itr != a_end; ++a_itr) {
			arc *a = *a_itr;
			primary_usage[a->globalArcIndex] = primary->bandwidth_usage_up;
		}
		for(list<arc*>::iterator a_itr = primary->arcs_down.begin(), a_end = primary->arcs_down.end(); a_itr != a_end; ++a_itr) {
			arc *a = *a_itr;
			primary_usage[a->globalArcIndex] = primary->bandwidth_usage_down;
		}

		/* for every arc (with return arc) */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs.at(aa);
			arc * a_return = a->return_arc;
			int aa_return = a_return->globalArcIndex;

			// extract real bandwidth requirement for backup path on arc/return arc if arc is used
			double usage_up = max(s->bandwidth_req_up - primary_usage[aa], 0.0);
			double usage_down = max(s->bandwidth_req_down - primary_usage[aa_return], 0.0);

			/* DUAL PRICE: SUM BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping with beta factor
			 */
			arc_costs[aa] += beta * duals->backupSumDuals[aa] 			* usage_up
						   + beta * duals->backupSumDuals[aa_return] 	* usage_down; // < 0

			/* DUAL PRICE: SINGLE BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping
			 */
			arc_costs[aa] += duals->backupSingleDuals[aa	   ][s->globalServiceIndex] * usage_up
						   + duals->backupSingleDuals[aa_return][s->globalServiceIndex] * usage_down; // < 0

			/* DUAL PRICE: BACKUP OVERLAP CTR (BACKUP)
			 * add overlap dual price if backup path uses arc for each service pair where owner takes part
			 */
			int tt, ss;
			ss = s->globalServiceIndex;
			for(tt = ss+1; tt < data->n_services; ++tt) {
				if(usage_up >= EPS) {
					arc_costs[aa] += duals->backupOverlapDuals[aa][ss][tt-ss-1]; // < 0
				}
				if(usage_down >= EPS) {
					arc_costs[aa] += duals->backupOverlapDuals[aa_return][ss][tt-ss-1]; // < 0
				}
			}
			tt = s->globalServiceIndex;
			for(ss = 0; ss < tt; ++ss) {
				if(usage_up >= EPS) {
					arc_costs[aa] += duals->backupOverlapDuals[aa][ss][tt-ss-1]; // < 0
				}
				if(usage_down >= EPS) {
					arc_costs[aa] += duals->backupOverlapDuals[aa_return][ss][tt-ss-1]; // < 0
				}
			}


		}
		return arc_costs;  // < 0 for all arcs
	}

	/* _spprc: solved an SPPRC problem using a labeling algorithm
	 * assumptions: the expected availability is the probability of an arc and its return arc to go down, thus
	 * the availability of the path returned is only the product sum of its arc in the up-link (down-link using the
	 * equivalent link in the opposite direction)
	 */
	double CloudBrokerModel::_spprc(int n_nodes, int start_node, int end_node, vector<vector<arc*> > *node_arcs,
										vector<double> *arc_costs, vector<int> *arc_restrictions, double max_latency,
										int max_restricted_arcs, double min_availability, list<arc*>* used_arcs) {

		/******** SETUP ***********/

		list<label> all_labels;
		list<label*> u;
		list<label*> p;

		label initiallabel;
		initiallabel.cost = 0.0;
		initiallabel.latency = 0.0;
		initiallabel.availability = 1.0;
		initiallabel.restricted_arcs_count = 0;
		initiallabel.end_node = start_node;
		initiallabel.last_arc = NULL;
		initiallabel.parent = NULL;
		initiallabel.isdominated = false;

		all_labels.push_back(initiallabel);
		u.push_back(&all_labels.back());

		/************ SPPRC labeling algorithm *************/

		while(u.size() > 0) {
			// PATH EXPANSION
			// - extract label q from U
			label *q = u.front();
			u.pop_front();

			bool found_new = false;

			// need only try expanding if not already dominated or not at end node
			if(!q->isdominated && q->end_node != end_node) {
				// - arcs from this label's (q) end node
				vector<arc*> *arcs = &node_arcs->at(q->end_node);
				// - try expand for each arc from end node
				for(unsigned int aa = 0; aa < arcs->size(); ++aa) {
					arc *a = arcs->at(aa);
					if(q->latency + a->latency + a->return_arc->latency <= max_latency &&
						q->restricted_arcs_count + arc_restrictions->at(a->globalArcIndex) <= max_restricted_arcs &&
						q->availability * a->exp_availability >= min_availability) {
						// resource check ok -> spawn new label
						label child;
						child.parent = q;
						child.last_arc = a;
						child.end_node = a->endNode;
						child.cost = q->cost + arc_costs->at(a->globalArcIndex);
						child.latency = q->latency + a->latency + a->return_arc->latency;
						child.availability = q->availability * a->exp_availability;
						child.restricted_arcs_count = q->restricted_arcs_count + arc_restrictions->at(a->globalArcIndex);
						child.isdominated = false;

						all_labels.push_back(child);
						u.push_back(&all_labels.back());

						found_new = true;
					}
				}
			}
			// - add q to P
			p.push_back(q);

			// need only perform dominations of any new labels were found
			if(found_new) {
				// DOMINATION

				// for every combination of labels (a, b)
				// - every label a not having been dominated
				for(list<label>::iterator l_itr = all_labels.begin(), l_end = all_labels.end(); l_itr != l_end; ++l_itr) {
					label* a = &(*l_itr);
					if(!a->isdominated) {

						// - every label b, succeeding a, that has not been dominated
						list<label>::iterator l_itr2 = l_itr;
						++l_itr2;
						for(list<label>::iterator l_end2 = all_labels.end(); l_itr2 != l_end2; ++l_itr2) {
							label* b = &(*l_itr2);

							// check that labels are not the same, but end at same node
							if(!b->isdominated && a->end_node == b->end_node) {
								// try dominate
								if(b->cost >= a->cost && b->latency >= a->latency && b->availability <= a->availability && b->restricted_arcs_count >= a->restricted_arcs_count) {
									// b is equally bad or worse than a -> dominate b
									b->isdominated = true;
								}
								else if(b->cost <= a->cost && b->latency <= a->latency && b->availability >= a->availability && b->restricted_arcs_count <= a->restricted_arcs_count) {
									// a is worse than b (not equally bad due to above if) -> dominate a
									a->isdominated = true;
								}
								// ELSE: a and b can not dominate / be dominated by each other..
							}

						}
					}
				}
			}

		}

		/********** EXTRACT SOLUTION **********/

		label *bestlabel = NULL;
		double bestcost = Inf;

		for(list<label*>::iterator l_itr = p.begin(), l_end = p.end(); l_itr != l_end; ++l_itr) {
			label* l = *l_itr;
			if(l->end_node == end_node && l->cost <= bestcost) {
				bestlabel = l;
				bestcost = l->cost;
			}
		}

		if(bestlabel != NULL) {

			double costcheck = 0.0;
			label *l = bestlabel;
			while(l->last_arc != NULL) {
				arc *a = l->last_arc;
				used_arcs->push_front(a);
				costcheck += arc_costs->at(a->globalArcIndex);
				l = l->parent;
			}

			return bestlabel->cost;
		}

		// return a very high cost if no path to end node was found
		return Inf;
	}

	returnPath CloudBrokerModel::_returnPathFromArcs(list<arc*>* arcs, service* owner, placement* p) {
		returnPath path;
		path.startNode = arcs->front()->startNode;
		path.endNode = arcs->back()->endNode;
		path.bandwidth_usage_up = owner->bandwidth_req_up;
		path.bandwidth_usage_down = owner->bandwidth_req_down;
		path.exp_availability = 1.0;
		path.cost = p->price;

		for(list<arc*>::iterator a_itr = arcs->begin(), a_end = arcs->end(); a_itr != a_end; ++a_itr) {
			arc* a = *a_itr;
			arc* a_return = a->return_arc;
			path.arcs_up.push_back(a);
			path.arcs_down.push_back(a_return);
			path.cost += a->bandwidth_price * path.bandwidth_usage_up +
						 a_return->bandwidth_price * path.bandwidth_usage_down;
			path.exp_availability *= a->exp_availability;

		}

		return path;
	}

	bool CloudBrokerModel::generateMappingHeuristicA(customer *c, service *s, dual_vals *duals) {

		// extract serve customer dual for this service's customer
		// - this is the only value that can give a positive evaluation for a new mapping
		double serve_customer_dual = duals->serveCustomerDuals[s->globalServiceIndex];
		// - if not a "negative cost", new mapping can not have a positive evaluation, return false
		if(serve_customer_dual >= 0.0) {
			return false;
		}

		// create a list of arcs for each node that originates from that node
		// - and including check for sufficient capacity to support service on arc
		vector<vector<arc*> > node_arcs(data->n_nodes);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs.at(aa);
			if(a->bandwidth_cap >= s->bandwidth_req_up && a->return_arc->bandwidth_cap >= s->bandwidth_req_down) {
				node_arcs.at(a->startNode).push_back(a);
			}
		}

		// calculate arc costs with dual values for a primary path for this service
		vector<double> arc_costs_primary = _dualPrimaryArcCostsForService(s, duals);

		// mark all arcs to be without restriction for primary path
		vector<int> arc_restrictions_primary(data->n_arcs, 0);

		// for each possible placement of this service
		for(unsigned int pp = 0; pp < s->possible_placements.size(); ++pp) {

			placement *p = &s->possible_placements.at(pp);
			int placement_node = data->n_nodes - data->n_providers + p->globalProviderIndex;
			int customer_node = c->globalCustomerIndex;

			// find primary path by shortest path problem
			list<arc*> primary_up_arcs;
			double primary_path_eval  = _spprc(data->n_nodes, customer_node, placement_node, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, 0.0, &primary_up_arcs);

			// see if a path was found at all -> if not, go to next iteration
			if(primary_up_arcs.size() == 0) continue;

			// check if found primary path can be basis for a profitable column
			if(- serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);

				// IF availability req ok
				if(primary.exp_availability > s->availability_req) {
					// create new mapping
					mapping m;

					// add primary path to paths list, then add its pointer to mapping
					data->paths.push_back(primary);
					m.primary = &data->paths.back();
					m.backup = NULL;

					// add new mapping to model and finish
					addMappingToModel(&m, s);

					cout << "---> NEW MAPPING eval: " << - serve_customer_dual - primary_path_eval - p->price << "\n";
					return true;
				} else {
					// ELSE: try finding primary/backup combo

					// extract arc costs with dual values for a backup path
					vector<double> arc_costs_backup = _dualBackupArcCostsForService(s, duals, &primary);

					// mark any arcs used by the primary path to be restricted (in either direction)
					vector<int> arc_restrictions_backup(data->n_arcs, 0);
					for(list<arc*>::iterator a_itr = primary.arcs_up.begin(), a_end = primary.arcs_up.end(); a_itr != a_end; ++a_itr) {
						arc_restrictions_backup[(*a_itr)->globalArcIndex] = 1;
					}
					for(list<arc*>::iterator a_itr = primary.arcs_down.begin(), a_end = primary.arcs_down.end(); a_itr != a_end; ++a_itr) {
						arc_restrictions_backup[(*a_itr)->globalArcIndex] = 1;
					}

					// while backup path has non-negative overlap restriction:
					int max_restricted_arcs_backup = (int)primary.arcs_up.size() -1;
					while(max_restricted_arcs_backup >= 0) {

						// find backup path by shortest path problem
						list<arc*> backup_up_arcs;
						double backup_path_eval = _spprc(data->n_nodes, customer_node, placement_node, &node_arcs,
														 &arc_costs_backup, &arc_restrictions_backup, s->latency_req,
														 max_restricted_arcs_backup, 0.0, &backup_up_arcs);

						if(backup_up_arcs.size() == 0) {
							--max_restricted_arcs_backup;
							continue;
						}

						// IF total evaluation is positive
						if(- serve_customer_dual - primary_path_eval - backup_path_eval - p->price >= EPS) {

							// create a return path object and calculate combo availability
							returnPath backup = _returnPathFromArcs(&backup_up_arcs, s, p);
							pathCombo combo = _pathComboForPaths(&primary, &backup);

							// IF availability req ok
							if(primary.exp_availability + backup.exp_availability - combo.exp_b_given_a >= s->availability_req) {
								// create a new mapping
								mapping m;

								// add found paths to paths list, then add their pointers to the mapping
								data->paths.push_back(primary);
								m.primary = &data->paths.back();
								data->paths.push_back(backup);
								m.backup = &data->paths.back();

								// add mapping to model
								addMappingToModel(&m, s);

								cout << "---> NEW MAPPING eval: " << - serve_customer_dual - primary_path_eval - backup_path_eval - p->price << "\n";
								return true;
							}
						}
						// ELSE -> break loop (any successive tries will have equally good or worse value evaluation)
						else {
							break;
						}

						// if reached -> reduce number of arc overlaps with primary path and try again
						--max_restricted_arcs_backup;
					}
				}
			}
		}
		return false;
	}

	bool CloudBrokerModel::generateMappingHeuristicB(customer *c, service *s, dual_vals *duals) {

		/*********************** SETUP *****************************/

		// extract serve customer dual for this service's customer
		// - this is the only value that can give a positive evaluation for a new mapping
		double serve_customer_dual = duals->serveCustomerDuals[s->globalServiceIndex];
		// - if not a "negative cost", new mapping can not have a positive evaluation, return false
		if(serve_customer_dual >= 0.0) {
			return false;
		}

		// best possible availability for any path in the network
		// - find the single arc with highest availability
		double availability_ubd = 0.0;

		// create a list of arcs for each node that originates from that node
		// - and including check for sufficient capacity to support service on arc
		vector<vector<arc*> > node_arcs(data->n_nodes);
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs.at(aa);
			if(a->bandwidth_cap >= s->bandwidth_req_up && a->return_arc->bandwidth_cap >= s->bandwidth_req_down) {
				if(a->exp_availability > availability_ubd) {
					availability_ubd = a->exp_availability;
				}
				node_arcs.at(a->startNode).push_back(a);
			}
		}

		// calculate arc costs with dual values for a primary path for this service
		vector<double> arc_costs_primary = _dualPrimaryArcCostsForService(s, duals);

		// mark all arcs to be without restriction for primary path
		vector<int> arc_restrictions_primary(data->n_arcs, 0);

		/****************** HEURISTIC START *********************/

		// for each possible placement of this service
		for(unsigned int pp = 0; pp < s->possible_placements.size(); ++pp) {

			placement *p = &s->possible_placements.at(pp);
			int placement_node = data->n_nodes - data->n_providers + p->globalProviderIndex;
			int customer_node = c->globalCustomerIndex;

			// try finding single feasible primary path
			list<arc*> primary_up_arcs;
			double primary_path_eval  = _spprc(data->n_nodes, customer_node, placement_node, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, s->availability_req, &primary_up_arcs);
			// see if a path was found at all and is profitable
			if(primary_up_arcs.size() > 0 && - serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);
				// create new mapping
				mapping m;

				// add primary path to paths list, then add its pointer to mapping
				data->paths.push_back(primary);
				m.primary = &data->paths.back();
				m.backup = NULL;

				// add new mapping to model and finish
				addMappingToModel(&m, s);

				cout << "---> NEW MAPPING (primary only): " << - serve_customer_dual - primary_path_eval - p->price << "\n";
			}

			// find primary path to combine with backup by shortest path problem with resource constraints
			double availability_lbd = max((s->availability_req - availability_ubd) / (1 - availability_ubd), 0.0);
			primary_up_arcs.clear();
			primary_path_eval  = _spprc(data->n_nodes, customer_node, placement_node, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, availability_lbd, &primary_up_arcs);

			// see if a path was found at all -> if not, go to next iteration
			if(primary_up_arcs.size() == 0) continue;

			// check if found primary path can be basis for a profitable column
			if(- serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);

				// if primary if availability feasible, it does not need a backup and will be generated in above code -> skip
				if(primary.exp_availability >= s->availability_req) {
					continue;
				}

				// try finding primary/backup combo
				double min_availability = (s->availability_req - primary.exp_availability) / (1 - primary.exp_availability);

				// extract arc costs with dual values for a backup path
				vector<double> arc_costs_backup = _dualBackupArcCostsForService(s, duals, &primary);

				// mark any arcs used by the primary path to be restricted (in either direction)
				vector<int> arc_restrictions_backup(data->n_arcs, 0);
				for(list<arc*>::iterator a_itr = primary.arcs_up.begin(), a_end = primary.arcs_up.end(); a_itr != a_end; ++a_itr) {
					arc_restrictions_backup[(*a_itr)->globalArcIndex] = 1;
				}
				for(list<arc*>::iterator a_itr = primary.arcs_down.begin(), a_end = primary.arcs_down.end(); a_itr != a_end; ++a_itr) {
					arc_restrictions_backup[(*a_itr)->globalArcIndex] = 1;
				}

				// while backup path has non-negative overlap restriction:
				int max_restricted_arcs_backup = (int)primary.arcs_up.size() -1;
				while(max_restricted_arcs_backup >= 0) {

					// find backup path by shortest path problem
					list<arc*> backup_up_arcs;
					double backup_path_eval = _spprc(data->n_nodes, customer_node, placement_node, &node_arcs,
													 &arc_costs_backup, &arc_restrictions_backup, s->latency_req,
													 max_restricted_arcs_backup, min_availability, &backup_up_arcs);

					if(backup_up_arcs.size() == 0) {
						--max_restricted_arcs_backup;
						continue;
					}

					// IF total evaluation is positive
					if(- serve_customer_dual - primary_path_eval - backup_path_eval - p->price >= EPS) {

						// create a return path object and calculate combo availability
						returnPath backup = _returnPathFromArcs(&backup_up_arcs, s, p);
						pathCombo combo = _pathComboForPaths(&primary, &backup);

						// IF availability req ok
						if(primary.exp_availability + backup.exp_availability - combo.exp_b_given_a >= s->availability_req) {
							// create a new mapping
							mapping m;

							// add found paths to paths list, then add their pointers to the mapping
							data->paths.push_back(primary);
							m.primary = &data->paths.back();
							data->paths.push_back(backup);
							m.backup = &data->paths.back();

							// add mapping to model
							addMappingToModel(&m, s);

							cout << "---> NEW MAPPING (primary+backup): " << - serve_customer_dual - primary_path_eval - backup_path_eval - p->price << "\n";
							return true;
						}
					}
					// ELSE -> break loop (any successive tries will have equally good or worse value evaluation)
					else {
						break;
					}

					// if reached -> reduce number of arc overlaps with primary path and try again
					--max_restricted_arcs_backup;
				}
			}
		}
		return false;
	}

	/******************* OutputResultsToStream **************************
	 * Takes an ostream as input (for instance cout or a file stream) and
	 * outputs the results from running the model.
	 */
	void CloudBrokerModel::OutputResultsToStream(ostream& stream) {

		stream << "\n################### RUN RESULTS ####################\n";

		stream << "\nProfits = " << master_problem.getObjVal() << "\n";

		double backup_costs = 0.0;
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs[aa];
			backup_costs += a->bandwidth_price * d_arcBackupUsage[aa].getSol();
		}
		stream << "\nBackup costs = " << backup_costs << "\n";

		stream << "\nBackup beta = " << beta << "\n";

		stream << "\nNumber of mappings = " << data->n_mappings << "\n";

		for(int cc = 0; cc < data->n_customers; ++cc) {
			customer *c = &data->customers[cc];
			if(y_serveCustomerVars[cc].getSol() > 0.01) {
				stream << "\nCustomer #" << cc+1 << " is being served (" << y_serveCustomerVars[cc].getSol() << ")\n";
				stream << "- Revenue: " << c->revenue << "\n";
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service *s = c->services[ss];
					stream << " - Service #" << s->globalServiceIndex +1 << " - number of potential mappings: " << s->mappings.size() << "\n";
					for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = *m_itr;
						XPRBvar *w = mappingVarForMappingIndex(m->globalMappingIndex);
						if(w == NULL) {
							stream <<  "  !! mapping #" << m->globalMappingIndex+1 << " has no related w_useMapping variable\n";
						}
						else if(w->getSol() > 0.01) {
							stream <<  "  -> mapping #" << m->globalMappingIndex+1 << "\n";
							stream << "   - primary path: " << m->primary->globalPathIndex+1 << ", cost: " << m->primary->cost << "\n";
							stream << "    - nodes up: " << m->primary->arcs_up.front()->startNode+1;
							for(list<arc*>::iterator a_itr = m->primary->arcs_up.begin(), a_end = m->primary->arcs_up.end(); a_itr != a_end; ++a_itr) {
								stream << "->" << (*a_itr)->endNode+1;
							}
							stream << "\n";
							if(m->backup != NULL) {
								stream << "   - backup path: " << m->backup->globalPathIndex+1 << "\n";
								stream << "    - nodes up: " << m->backup->arcs_up.front()->startNode+1;
								for(list<arc*>::iterator a_itr = m->backup->arcs_up.begin(), a_end = m->backup->arcs_up.end(); a_itr != a_end; ++a_itr) {
									stream << "->" << (*a_itr)->endNode+1;
								}
								stream << "\n";
							}
						}
					}
				}
			}
		}

		stream << "\nArc backup reservations (non-zero)\n";
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			if(d_arcBackupUsage[aa].getSol() > 0.00001) {
				arc *a = &data->arcs[aa];
				stream << "(" << a->startNode+1 << "," << a->endNode+1 << ") = " << d_arcBackupUsage[aa].getSol() << "\n";
			}
		}
	}

	XPRBvar* CloudBrokerModel::mappingVarForMappingIndex(int mappingIndex) {
		if(mappingIndex < 0 || mappingIndex >= (int) this->w_useMappingVars.size()) {
			return NULL;
		}

		list<XPRBvar>::iterator w_itr = this->w_useMappingVars.begin();
		for(int i = 0; i < mappingIndex; i++) {
			++w_itr;
		}

		return &(*w_itr);
	}

	double Parameters::U_PrimaryBandwidthUsageOnArcForMapping(arc * a, mapping * m) {
		for(list<arc*>::const_iterator a_itr = m->primary->arcs_up.begin(),
				aend = m->primary->arcs_up.end(); a_itr != aend; ++a_itr) {
			if(a == (*a_itr)) return m->primary->bandwidth_usage_up;
		}
		for(list<arc*>::const_iterator a_itr = m->primary->arcs_down.begin(),
				aend = m->primary->arcs_down.end(); a_itr != aend; ++a_itr) {
			if(a == (*a_itr)) return m->primary->bandwidth_usage_down;
		}
		return 0.0;
	}

	double Parameters::Q_BackupBandwidthUsageOnArcForMapping(arc * a, mapping * m) {
		if(m->backup != NULL) {
			// amount already reserved by primary path on arc
			double primary_req_on_arc = Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m);

			// find gross backup req on arc for mapping
			double backup_req_on_arc = 0.0;
			for(list<arc*>::const_iterator a_itr = m->backup->arcs_up.begin(),
				aend = m->backup->arcs_up.end(); a_itr != aend; ++a_itr) {
				if(a == (*a_itr)) backup_req_on_arc = m->backup->bandwidth_usage_up;
			}
			for(list<arc*>::const_iterator a_itr = m->backup->arcs_down.begin(),
				aend = m->backup->arcs_down.end(); a_itr != aend; ++a_itr) {
				if(a == (*a_itr)) backup_req_on_arc = m->backup->bandwidth_usage_down;
			}
			// return net backup req on arc
			return max(backup_req_on_arc - primary_req_on_arc, 0.0);
		}
		return 0.0;
	}

	double Parameters::R_RevenueForCustomer(customer * c) {
		return c->revenue;
	}

	double Parameters::E_PrimaryPathCost(returnPath * p) {
		return p->cost;
	}

	double Parameters::E_PerBandwidthCostForArc(arc * a) {
		return a->bandwidth_price;
	}

	double Parameters::F_BandwidthCapacityForArc(arc * a) {
		return a->bandwidth_cap;
	}
}
