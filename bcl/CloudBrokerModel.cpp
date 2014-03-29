#include "CloudBrokerModel.h"
#include "entities.h"
#include "xprb_cpp.h"
#include "xprs.h"
#include <list>
#include <iostream>
#include <algorithm>

using namespace entities;
using namespace ::dashoptimization;
using namespace std;

namespace cloudbrokermodels {

	/*
	 * TEMPORARILY STOLEN FROM PATHGENERATOR: should make classes from entities, and add as method to returnPath ??
	 */
	pathCombo _pathComboForPaths(returnPath * a, returnPath * b) {
		pathCombo combo;
		combo.a = a;
		combo.b = b;
		combo.exp_b_given_a = a->exp_availability;

		// calculate prop b up given a up
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

	CloudBrokerModel::CloudBrokerModel() :
			// member initialisation
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

		/* Problem dimensions */
		int n_customers = data->n_customers;
		int n_services = data->n_services;
		int n_arcs = data->network.arcs.size();
		int n_mappings = data->n_mappings;

		vector<service*> services_by_global_index;
		services_by_global_index.reserve(n_services);
		for(int cc = 0; cc < n_customers; cc++) {
			customer * c = &data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ss++) {
				service * s = &c->services[ss];
				services_by_global_index[s->globalServiceIndex] = s;
			}
		}

		/* w variable iterator */
		list<XPRBvar>::iterator w_itr;

		/********** CREATE VARIABLES **********/

		cout << " - creating variables..\n";

		/* Serve Customer variables
		 * for: every customer
		 */
		y_serveCustomerVars.reserve(n_customers);
		for(int cc = 0; cc < n_customers; cc++) {
			y_serveCustomerVars.push_back(
				master_problem.newVar(XPRBnewname("y_serve_customer_%d", cc+1), XPRB_BV, 0, 1)
			);
		}
		cout << "   - created " << y_serveCustomerVars.size() << " y-variables\n";

		/* Use Mapping variables
		 *
		 * for: every customer
		 *  for: every service of customer
		 *   for: every mapping of service
		 */
		for(int mm = 0; mm < n_mappings; mm++) {
			w_useMappingVars.push_back(
				master_problem.newVar(
					XPRBnewname("m_use_mapping_%d", mm+1),
					XPRB_BV, 0, 1
				)
			);
		}
		cout << "   - created " << w_useMappingVars.size() << " w-variables\n";

		/* Services overlap variables
		 * for: every pair of two services (s1, s2)
		 *      - assume services are ordered, each combination of services where s1 < s2
		 */
		int l_count = 0;
		/* for every service s (except last service) */
		l_servicesOverlapVars.resize(n_services-1);
		for(int ss = 0; ss < n_services-1; ss++) {

			/* for every service t | t > s */
			l_servicesOverlapVars[ss].reserve(n_services-1-ss);
			for(int tt = ss+1; tt < n_services; tt++) {

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
		 * for: every arc (i,j)
		 */
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];
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
		for(int cc = 0; cc < n_customers; cc++) {
			customer * c = &data->customers[cc];
			z_obj_expression += Parameters::R_RevenueForCustomer(c)*y_serveCustomerVars[cc];
		}

		/*	Second term:
		 *  - sum cost from all used primary paths
		 */
		w_itr = w_useMappingVars.begin();
		for(int cc = 0; cc < n_customers; cc++) {
			customer * c = &data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ss++) {
				service * s = &c->services[ss];
				for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
					z_obj_expression -= Parameters::E_PrimaryPathCost(m_itr->primary) * (*w_itr);
					++w_itr;
				}
			}
		}

		/* Third term:
		 * - sum costs of total reserved capacity for backup paths
		 */
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];
			z_obj_expression -= Parameters::E_PerBandwidthCostForArc(a)*d_arcBackupUsage[aa];
		}

		/* create final objective function */
		z_objective = master_problem.newCtr("OBJ", z_obj_expression);
		/* set objective function for problem */
		master_problem.setObj(z_objective);

		/******* CREATE CONSTRAINTS *******/
		cout << "DONE!\n - creating constraints..\n";

		/* SERVE CUSTOMER CONSTRAINT
		 * - If customer is to be served - and generate revenue - all services of that customer
		 *   must be assigned a mapping
		 * for: every customer
		 * 	for: every service of customer
		 */
		cout << "  - SERVE CUSTOMER CONSTRAINTS..";
		w_itr = w_useMappingVars.begin();
		int serviceNumber = 0;
		serveCustomerCtr.reserve(n_services);
		/* for every customer */
		for(int cc = 0; cc < n_customers; ++cc) {
			customer * c = &data->customers[cc];

			/* for every service of customer */
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service * s = &c->services[ss];
				++serviceNumber;

				/* NEW CONSTRAINT: lhs expression */
				XPRBexpr map_service_expr;

				/* for every mapping of service*/
				for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {

					/* add mapping selection var */
					map_service_expr += (*w_itr);

					++w_itr;
				}

				/* subtract serve customer var */
				map_service_expr -= y_serveCustomerVars[cc];

				/* create final constraint */
				this->serveCustomerCtr.push_back(
					this->master_problem.newCtr(
							XPRBnewname("serve_customer_ctr_%d", serviceNumber),
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
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];

			/* NEW CONSTRAINT: lhs expression*/
			XPRBexpr arc_bw_usage;

			/* for every mapping */
			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = &c->services[ss];
					for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);

						/* add bandwidth used by used mappings on arc */
						arc_bw_usage += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a,m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* add bandwidth reserved for backup on arc */
			arc_bw_usage += d_arcBackupUsage[aa];

			/* create final constraint */
			this->arcCapacityCtr.push_back(
				this->master_problem.newCtr(
					XPRBnewname("arc_capacity_ctr_%d_%d", a->startNode, a->endNode),
					arc_bw_usage <= Parameters::F_BandwidthCapacityForArc(a)
				)
			);
		}
		cout << "DONE! (" << this->arcCapacityCtr.size() << " created) \n";

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
		backupSingleCtr.resize(n_arcs);
		int backupSingleCount = 0;
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];

			/* for every service */
			serviceNumber = 0;
			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {	/* for every service 	*/
					service * s = &c->services[ss];
					++serviceNumber;

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr service_backup_req_on_arc;

					/* for every mapping */
					for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);

						/* add backup requirement on arc for mapping */
						service_backup_req_on_arc += (Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m))*(*w_itr);

						++w_itr;
					}

					/* subtract */
					service_backup_req_on_arc -= d_arcBackupUsage[aa];

					/* create final constraint */
					this->backupSingleCtr[aa].push_back(
						this->master_problem.newCtr(
							XPRBnewname("backup_single_ctr_%d_%d_%d", a->startNode, a->endNode, serviceNumber),
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
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];

			/* NEW CONSTRAINT: lhs expression*/
			XPRBexpr total_backup_req_expr;

			/* for every mapping */
			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = &c->services[ss];
					for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);

						/* add backup req on arc for used mappings */
						total_backup_req_expr += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* subtract backup reservation on arc variable*/
			total_backup_req_expr-= d_arcBackupUsage[aa];

			/* create final constraint */
			this->backupSumCtr.push_back(
				this->master_problem.newCtr(
					XPRBnewname("backup_sum_ctr_%d_%d", a->startNode, a->endNode),
					beta_backupres * total_backup_req_expr <= 0.0
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
		primaryOverlapCtr.resize(n_arcs);
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc *a = &data->network.arcs[aa];
			primaryOverlapCtr[aa].resize(n_services-1);

			/* for every pair of two services (s, t) */
			for(int ss = 0; ss < n_services-1; ++ss) {
				service *s = services_by_global_index[ss];
				primaryOverlapCtr[aa][ss].reserve(n_services-1-ss);
				for(int tt = ss+1; tt < n_services; ++tt) {
					service *t = services_by_global_index[tt];

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr primary_overlap_expr;

					/* sum over all mappings for service s using arc a */
					for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);
						if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {
							primary_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* sum over all mappings for service t using arc a */
					for (list<mapping>::iterator m_itr = t->mappings.begin(), m_end = t->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);
						if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {
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
		backupOverlapCtr.resize(n_arcs);
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc *a = &data->network.arcs[aa];
			backupOverlapCtr[aa].resize(n_services-1);

			/* for every pair of two services (s, t) */
			for(int ss = 0; ss < n_services-1; ++ss) {
				service *s = services_by_global_index[ss];
				backupOverlapCtr[aa][ss].reserve(n_services-1-ss);
				for(int tt = ss+1; tt < n_services; ++tt) {
					service *t = services_by_global_index[tt];

					/* NEW CONSTRAINT: lhs expression */
					XPRBexpr backup_overlap_expr;

					/* sum over all mappings for service s using arc a */
					for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);
						if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {

							/* add mapping selection var for mapping */
							backup_overlap_expr += *mappingVarForMappingIndex(m->globalMappingIndex);
						}
					}

					/* sum over all mappings for service t using arc a */
					for (list<mapping>::iterator m_itr = t->mappings.begin(), m_end = t->mappings.end(); m_itr != m_end; ++m_itr) {
						mapping * m = &(*m_itr);
						if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {

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
		this->master_problem.setSense(XPRB_MAXIM);

		return;
	}

	void CloudBrokerModel::RunModel(bool enforce_integer) {
		if(enforce_integer) {
			this->master_problem.mipOptimise();
		} else {
			this->master_problem.lpOptimise();
		}
	}

	void CloudBrokerModel::RunModelColumnGeneration() {
		int itercount = 0;
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_CUTSTRATEGY, 0);	/* Disable automatic cuts - we use our own */
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_PRESOLVE, 0);		/* Switch presolve off */
		master_problem.setMsgLevel(1);
		while(itercount < 100) {
			bool foundColumn = false;
			cout << "running lp-relaxation\n";
			this->RunModel(false);
			XPRBbasis basis = master_problem.saveBasis();

			CloudBrokerModel::dual_vals duals = this->getDualVals();

			cout << "generating columns..\n" << std::endl;
			// use duals to generate column(s) for each service
			for(int cc = 0; cc < this->data->n_customers; ++cc) {
				customer *c = &this->data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service *s = &c->services[ss];
					if(this->generateMappingColumnBruteForce(s, &duals)) foundColumn = true;
				}
			}

			if(!foundColumn) break; /* no new column was found -> end column generation */

			master_problem.loadBasis(basis);
			++itercount;
		}
		cout << "running MIP-model\n";
		this->RunModel(true);
	}

	CloudBrokerModel::dual_vals CloudBrokerModel::getDualVals() {
		CloudBrokerModel::dual_vals duals;

		duals.serveCustomerDuals.resize(data->n_services, 0.0);
		for(int ss = 0; ss < data->n_services; ++ss) {
			duals.serveCustomerDuals[ss] = serveCustomerCtr[ss].getDual();
		}

		duals.arcCapacityDuals.resize(data->network.arcs.size(), 0.0);
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			duals.arcCapacityDuals[aa] = arcCapacityCtr[aa].getDual();
		}

		duals.backupSingleDuals.resize(data->network.arcs.size());
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			duals.backupSingleDuals[aa].resize(data->n_services, 0.0);
			for(int ss = 0; ss < data->n_services; ++ss) {
				duals.backupSingleDuals[aa][ss] = backupSingleCtr[aa][ss].getDual();
			}
		}

		duals.backupSumDuals.resize(data->network.arcs.size(), 0.0);
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			duals.backupSumDuals[aa] = backupSumCtr[aa].getDual();
		}

		duals.primaryOverlapDuals.resize(data->network.arcs.size());
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			duals.primaryOverlapDuals[aa].resize(data->n_services-1);
			for(int ss = 0; ss < data->n_services-1; ++ss) {
				duals.primaryOverlapDuals[aa][ss].resize(data->n_services-1-ss);
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					duals.primaryOverlapDuals[aa][ss][tt-ss-1] = primaryOverlapCtr[aa][ss][tt-ss-1].getDual();
				}
			}
		}

		duals.backupOverlapDuals.resize(data->network.arcs.size());
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			duals.backupOverlapDuals[aa].resize(data->n_services-1);
			for(int ss = 0; ss < data->n_services-1; ++ss) {
				duals.backupOverlapDuals[aa][ss].resize(data->n_services-1-ss);
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					duals.backupOverlapDuals[aa][ss][tt-ss-1] = backupOverlapCtr[aa][ss][tt-ss-1].getDual();
				}
			}
		}

		return duals;
	}

	bool CloudBrokerModel::generateMappingColumnBruteForce(entities::service *s, dual_vals *duals){

		mapping bestFound;
		double best_eval = 0.0;

		// to be implemented
		for(unsigned int pp = 0; pp < s->possible_placements.size(); ++pp) {
			placement *p = &s->possible_placements[pp];
			for(unsigned int kk = 0; kk < p->paths.size(); ++kk) {
				returnPath *k = &p->paths[kk];
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
						returnPath * b = &p->paths[bb];
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

		if(best_eval > 0.00001) {
			this->addMappingToModel(&bestFound, s);
			return true;
		}
		return false;
	}

	void CloudBrokerModel::addMappingToModel(entities::mapping *m, entities::service *owner) {

		// increment number of mappings count, add a global number to mapping
		owner->mappings.push_back(*m);
		m->globalMappingIndex = this->data->n_mappings;
		++this->data->n_mappings;

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

		// add mapping primary path usage to all 'arcCap'-constraints
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];
			arcCapacityCtr[aa] += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) * (*w);
		}

		// single backup constraint
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];
			backupSingleCtr[aa][owner->globalServiceIndex] += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * (*w);
		}

		// sum backup constraint
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];
			backupSumCtr[aa] += beta * Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * (*w);
		}

		// primary overlap constraint
		/* for every arc where mapping requires primary capacity*/
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc *a = &data->network.arcs[aa];
			if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0) {

				/* for every service pair where owner service takes part */
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
		}

		// backup overlap constraint
		/* for every arc where mapping requires backup capacity*/
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc *a = &data->network.arcs[aa];
			if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0) {

				/* for every service pair where owner service takes part */
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

	double CloudBrokerModel::_bruteForceEvalMapping(entities::mapping *m, entities::service *owner, dual_vals *duals) {
		/**** c ****/
		double c = - m->primary->cost;

		/**** A^Ty: *****/
		double At_y = 0.0;

		//   - a_s
		// add dual cost from owner's serve customer constraint
		At_y += duals->serveCustomerDuals[owner->globalServiceIndex];

		// dual price primary
		/* for every arc */
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			arc * a = &data->network.arcs[aa];

			/* add dual cost for use on this arc for this mapping */
			At_y += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) * duals->arcCapacityDuals[aa];
		}

		// dual price sum backup
		/* for every arc */
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			arc * a = &data->network.arcs[aa];

			/* add dual cost for backup use on arc for mapping */
			At_y += beta * Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * duals->backupSumDuals[aa];
		}

		// dual price single backup
		/* for every arc */
		for(unsigned int aa = 0; aa < data->network.arcs.size(); ++aa) {
			arc * a = &data->network.arcs[aa];
			At_y += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * duals->backupSingleDuals[aa][owner->globalServiceIndex];
		}

		// primary overlap constraint
		/* for every arc where mapping requires primary capacity*/
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc *a = &data->network.arcs[aa];
			if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0) {

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
		}

		// backup overlap constraint
		/* for every arc where mapping requires backup capacity*/
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc *a = &data->network.arcs[aa];
			if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0) {

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

	void CloudBrokerModel::OutputResults() {
		/* Problem dimensions */
		int n_customers = this->data->n_customers;
		int n_arcs = this->data->network.arcs.size();

		/* Model variable iterators */
		list<XPRBvar>::iterator w_itr;

		cout << "\n=========== RESULTS =============\n";

		cout << "\nProfits: " << this->master_problem.getObjVal() << "\n";

		double backup_costs = 0.0;
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc *a = &this->data->network.arcs[aa];
			backup_costs += a->bandwidth_price * d_arcBackupUsage[aa].getSol();
		}
		cout << "\nBackup costs: " << backup_costs << "\n";

		w_itr = this->w_useMappingVars.begin();
		int serviceNumber = 0;
		for(int cc = 0; cc < n_customers; ++cc) {
			customer *c = &this->data->customers[cc];
			if(y_serveCustomerVars[cc].getSol() > 0.01) {
				cout << "\nCustomer #" << cc+1 << " is being served (" << y_serveCustomerVars[cc].getSol() << ")\n";
				cout << "- Revenue: " << c->revenue << "\n";
			}
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service *s = &c->services[ss];
				++serviceNumber;
				for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
					mapping * m = &(*m_itr);
					if((*w_itr).getSol() > 0.01) {
						cout << " - Service #" << serviceNumber << " -> mapping #" << m->globalMappingIndex+1 << "\n";
						cout << "  - primary path: " << m->primary->pathNumber << ", cost: " << m->primary->cost << "\n";
						if(m->backup != NULL) {
							cout << "  - backup path: " << m->backup->pathNumber << "\n";
						}
					}
					++w_itr;
				}
			}
		}

		cout << "\nArc backup reservations (non-zero)\n";
		for(int aa = 0; aa < n_arcs; ++aa) {
			if(d_arcBackupUsage[aa].getSol() > 0.00001) {
				arc *a = &this->data->network.arcs[aa];
				cout << "(" << a->startNode+1 << ", " << a->endNode+1 << ") : " << d_arcBackupUsage[aa].getSol() << "\n";
			}
		}
	}

	XPRBvar* CloudBrokerModel::mappingVarForMappingIndex(int mappingIndex) {
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
