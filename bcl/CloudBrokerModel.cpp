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
	void CloudBrokerModel::BuildModel(dataContent * data, double beta_backupres) {

		/********* SETUP **********/

		this->data = data;
		this->beta = beta_backupres;

		/* Problem dimensions */
		int n_customers = data->n_customers;
		int n_services = data->n_services;
		int n_arcs = data->network.arcs.size();
		int n_mappings = data->n_mappings;

		/* Model variable iterators */
		list<XPRBvar>::iterator y_itr;
		list<XPRBvar>::iterator w_itr;
		list<XPRBvar>::iterator d_itr;
		list<XPRBvar>::iterator l_itr;

		/********** CREATE VARIABLES **********/

		cout << " - creating variables..\n";

		/* Serve Customer variables
		 * for: every customer
		 */
		for(int cc = 0; cc < n_customers; cc++) {
			this->y_serveCustomerVars.push_back(
				this->master_problem.newVar(XPRBnewname("y_serve_customer_%d", cc+1), XPRB_BV, 0, 1)
			);
		}
		cout << "   - created " << this->y_serveCustomerVars.size() << " y-variables\n";

		/* Use Mapping variables
		 * for: every mapping
		 * [shorthand for:
		 *    for: every customer
		 * 	   for: every service of customer
		 * 	    for: every mapping of service
		 * ]
		 */
		for(int mm = 0; mm < n_mappings; mm++) {
			this->w_useMappingVars.push_back(
				this->master_problem.newVar(
					XPRBnewname("m_use_mapping_%d", mm+1),
					XPRB_BV, 0, 1
				)
			);
		}
		cout << "   - created " << this->w_useMappingVars.size() << " w-variables\n";

		/* Services overlap variables
		 * for: every pair of two services (s1, s2)
		 *      - assume services are ordered, each combination of services where s1 < s2
		 */
		for(int ss = 0; ss < n_services; ss++) {
			for(int tt = ss+1; tt < n_services; tt++) {
				this->l_servicesOverlapVars.push_back(
					this->master_problem.newVar(
						XPRBnewname("l_service_overlaps_%d_%d", ss+1, tt+1),
						XPRB_BV, 0, 1
					)
				);
			}
		}
		cout << "   - created " << this->l_servicesOverlapVars.size() << " l-variables\n";

		/* Arc Backup Usage variable
		 * for: every arc (i,j)
		 */
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];
			this->d_arcBackupUsage.push_back(
				this->master_problem.newVar(
					XPRBnewname("d_backup_usage_%d_%d", a->startNode, a->endNode),
					XPRB_PL, 0, a->bandwidth_cap
				)
			);
		}
		cout << "   - created " << this->d_arcBackupUsage.size() << " d-variables\n";

		/******* CREATE OBJECTIVE FUNCTION ********/

		cout << " - creating objective function..";

		XPRBexpr z_obj_expression;

		/*	First term:
		 *  - sum revenue from served customers
		 */
		y_itr = this->y_serveCustomerVars.begin();
		for(int cc = 0; cc < n_customers; cc++) {
			customer * c = &data->customers[cc];
			z_obj_expression += Parameters::R_RevenueForCustomer(c)*(*y_itr);
			++y_itr;
		}

		/*	Second term:
		 *  - sum cost from all used primary paths
		 */
		w_itr = this->w_useMappingVars.begin();
		for(int cc = 0; cc < n_customers; cc++) {
			customer * c = &data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ss++) {
				service * s = &c->services[ss];
				for(unsigned int mm = 0; mm < s->possible_mappings.size(); mm++) {
					mapping * m = &s->possible_mappings[mm];
					z_obj_expression -= Parameters::E_PrimaryPathCost(m->primary)*(*w_itr);
					++w_itr;
				}
			}
		}

		/* Third term:
		 * - sum costs of total reserved capacity for backup paths
		 */
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];
			z_obj_expression -= Parameters::E_PerBandwidthCostForArc(a)*(*d_itr);
			++d_itr;
		}

		/* create final objective function */
		this->z_objective = this->master_problem.newCtr("OBJ", z_obj_expression);
		/* set objective function for problem */
		this->master_problem.setObj(this->z_objective);

		/******* CREATE CONSTRAINTS *******/
		cout << "DONE!\n - creating constraints..\n";

		/* SERVE CUSTOMER CONSTRAINT
		 * - If customer is to be served - and generate revenue - all services of that customer
		 *   must be assigned a mapping
		 * for: every customer
		 * 	for: every service of customer
		 */
		cout << "  - SERVE CUSTOMER CONSTRAINTS..";
		w_itr = this->w_useMappingVars.begin();
		y_itr = this->y_serveCustomerVars.begin();
		int serviceNumber = 0;

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
				for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {

					/* add mapping selection var */
					map_service_expr += (*w_itr);

					++w_itr;
				}

				/* subtract serve customer var */
				map_service_expr -= (*y_itr);

				/* create final constraint */
				this->serveCustomerCtr.push_back(
					this->master_problem.newCtr(
							XPRBnewname("serve_customer_ctr_%d", serviceNumber),
							map_service_expr == 0.0
					)
				);
			}
			++y_itr;
		}
		cout << "DONE! (" << this->serveCustomerCtr.size() << " created) \n";

		/* ARC CAPACITY CONSTRAINT
		 * - the sum of capacity used by chosen primary paths over link a and bandwidth reserved for
		 *   backup must not exceed the capacity of the arc
		 *   for: every arc a
		 */
		cout <<	"  - ARC CAPACITY CONSTRAINTS..";

		/* for every arc */
		d_itr = this->d_arcBackupUsage.begin();
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
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping * m = &s->possible_mappings[mm];

						/* add bandwidth used by used mappings on arc */
						arc_bw_usage += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a,m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* add bandwidth reserved for backup on arc */
			arc_bw_usage += (*d_itr);

			/* create final constraint */
			this->arcCapacityCtr.push_back(
				this->master_problem.newCtr(
					XPRBnewname("arc_capacity_ctr_%d_%d", a->startNode, a->endNode),
					arc_bw_usage <= Parameters::F_BandwidthCapacityForArc(a)
				)
			);
			++d_itr;
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
		d_itr = this->d_arcBackupUsage.begin();
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
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping* m = &s->possible_mappings[mm];

						/* add backup requirement on arc for mapping */
						service_backup_req_on_arc += (Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m))*(*w_itr);

						++w_itr;
					}

					/* subtract */

					/* create final constraint */
					this->backupSingleCtr.push_back(
						this->master_problem.newCtr(
							XPRBnewname("backup_single_ctr_%d_%d_%d", a->startNode, a->endNode, serviceNumber),
								service_backup_req_on_arc <= (*d_itr)
						)
					);
				}
			}
			++d_itr;
		}
		cout << "DONE! (" << this->backupSingleCtr.size() << " created) \n";

		/* SUM BACKUP RESERVATION CONSTRAINT
		 * - a proportion of total backup requirement on an arc must be reserved
		 * for: every arc
		 */
		cout << "  - SUM BACKUP RESERVATION CONSTRAINTS..";

		/* for every arc */
		d_itr = this->d_arcBackupUsage.begin();
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
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping *m = &s->possible_mappings[mm];

						/* add backup req on arc for used mappings */
						total_backup_req_expr += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m)*(*w_itr);

						++w_itr;
					}
				}
			}

			/* subtract backup reservation on arc variable*/
			total_backup_req_expr-= (*d_itr);

			/* create final constraint */
			this->backupSumCtr.push_back(
				this->master_problem.newCtr(
					XPRBnewname("backup_sum_ctr_%d_%d", a->startNode, a->endNode),
					beta_backupres * total_backup_req_expr <= 0.0
				)
			);

			++d_itr;
		}
		cout << "DONE! (" << this->backupSumCtr.size() << " created)\n";

		/* PRIMARY OVERLAP CONSTRAINT
		 * - if for any arc, the chosen mappings of two services uses that arc, the
		 *   two services are said to overlap
		 *
		 *   for: every pair of two services services
		 */
		cout << "  - PRIMARY OVERLAP CONSTRAINTS..";

		/* for every pair of two services */
		l_itr = this->l_servicesOverlapVars.begin();
		int snum1 = 0;
		for(int cc1 = 0; cc1 < n_customers; ++cc1) {
			customer * c1 = &data->customers[cc1];
			for(unsigned int ss = 0; ss < c1->services.size(); ++ss) {
				service *s = &c1->services[ss];
				++snum1;
				int snum2 = 0;
				for(int cc2 = 0; cc2 < n_customers; ++cc2) {
					customer * c2 = &data->customers[cc2];
					for(unsigned int tt = 0; tt < c2->services.size(); ++tt) {
						service *t = &c2->services[tt];
						++snum2;
						if(snum1 < snum2) {

							/* for every arc */
							for(int aa = 0; aa < n_arcs; ++aa) {
								arc *a = &data->network.arcs[aa];

								/* NEW CONSTRAINT: lhs expression */
								XPRBexpr primary_overlap_expr;

								/* for every mapping of service s using arc for primary */
								for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
									mapping * m = &s->possible_mappings[mm];
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {

										/* add mapping selection var for mapping */
										primary_overlap_expr += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}

								/* for every mapping of service t using arc for primary */
								for(unsigned int mm = 0; mm < t->possible_mappings.size(); ++mm) {
									mapping * m = &t->possible_mappings[mm];
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {

										/* add mapping selection var for mapping */
										primary_overlap_expr += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}

								/* subtract primary overlap var for service pair (s, t)*/
								primary_overlap_expr -= (*l_itr);

								/* create final constraint */
								this->primaryOverlapCtr.push_back(
									this->master_problem.newCtr(
										XPRBnewname("primary_overlap_ctr_%d_%d", a->startNode, a->endNode),
										primary_overlap_expr <= 1.0
									)
								);
							}
							++l_itr;
						}
					}
				}
			}
		}
		cout << "DONE! (" << this->primaryOverlapCtr.size() << " created)\n";

		/* BACKUP OVERLAP CONSTRAINT
		 * - if two services' primary paths overlap, their backup paths can not have bandwidth requirements at
		 *   the same arc
		 *
		 *   for: every pair of two services services
		 */
		cout << "  - BACKUP OVERLAP CONSTRAINTS..";
		l_itr = this->l_servicesOverlapVars.begin();
		snum1 = 0;

		/* for every pair of two services */
		for(int cc1 = 0; cc1 < n_customers; ++cc1) {
			customer * c1 = &data->customers[cc1];
			for(unsigned int ss = 0; ss < c1->services.size(); ++ss) {
				service *s = &c1->services[ss];
				++snum1;
				int snum2 = 0;
				for(int cc2 = 0; cc2 < n_customers; ++cc2) {
					customer * c2 = &data->customers[cc2];
					for(unsigned int tt = 0; tt < c2->services.size(); ++tt) {
						service *t = &c2->services[tt];
						++snum2;
						if(snum1 < snum2) {

							/* for every  */
							for(int aa = 0; aa < n_arcs; ++aa) {
								arc *a = &data->network.arcs[aa];

								/* NEW CONSTRAINT: lhs expression */
								XPRBexpr backup_overlap_expr;

								/* for every mapping of service s using arc */
								for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
									mapping * m = &s->possible_mappings[mm];
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {

										/* add mapping selection var for mapping */
										backup_overlap_expr += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}

								/* for every mapping of service t using arc */
								for(unsigned int mm = 0; mm < t->possible_mappings.size(); ++mm) {
									mapping * m = &t->possible_mappings[mm];
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {

										/* add mapping selection var for mapping */
										backup_overlap_expr += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}

								/* add primary overlap var for service pair (s, t) */
								backup_overlap_expr += (*l_itr);

								/* create final constraint */
								this->backupOverlapCtr.push_back(
									this->master_problem.newCtr(
										XPRBnewname("backup_overlap_ctr_%d_%d", a->startNode, a->endNode),
										backup_overlap_expr <= 2.0
									)
								);
							}
							++l_itr;
						}
					}
				}
			}
		}
		cout << "DONE! (" << this->backupOverlapCtr.size() << " created)\n";

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
		XPRSsetintcontrol(this->master_problem.getXPRSprob(), XPRS_CUTSTRATEGY, 0);	/* Disable automatic cuts - we use our own */
		XPRSsetintcontrol(this->master_problem.getXPRSprob(), XPRS_PRESOLVE, 0);		/* Switch presolve off */
		while(itercount < 100) {
			bool foundColumn = false;
			this->RunModel(false);
			XPRBbasis basis = this->master_problem.saveBasis();

			CloudBrokerModel::dual_vals duals = this->getDualVals();

			// use duals to generate column(s) for each service
			for(int cc = 0; cc < this->data->n_customers; ++cc) {
				customer *c = &this->data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service *s = &c->services[ss];
					if(this->generateMappingColumnBruteForce(s, &duals)) foundColumn = true;
				}
			}

			if(!foundColumn) break; /* no new column was found -> end column generation */

			this->master_problem.loadBasis(basis);
			++itercount;
		}
		this->RunModel(true);
	}

	CloudBrokerModel::dual_vals CloudBrokerModel::getDualVals() {
		CloudBrokerModel::dual_vals duals;

		duals.serveCustomerDuals.resize(this->serveCustomerCtr.size(), 0.0);
		list<XPRBctr>::iterator ctr = this->serveCustomerCtr.begin();
		for(unsigned int i = 0; i < this->serveCustomerCtr.size(); ++i) {
			duals.serveCustomerDuals[i] = (*ctr).getDual();
			++ctr;
		}

		duals.arcCapacityDuals.resize(this->arcCapacityCtr.size(), 0.0);
		ctr = this->arcCapacityCtr.begin();
		for(unsigned int i = 0; i < this->arcCapacityCtr.size(); ++i) {
			duals.arcCapacityDuals[i] = (*ctr).getDual();
			++ctr;
		}

		duals.backupSingleDuals.resize(this->backupSingleCtr.size(), 0.0);
		ctr = this->backupSingleCtr.begin();
		for(unsigned int i = 0; i < this->backupSingleCtr.size(); ++i) {
			duals.backupSingleDuals[i] = (*ctr).getDual();
			++ctr;
		}

		duals.backupSumDuals.resize(this->backupSumCtr.size(), 0.0);
		ctr = this->backupSumCtr.begin();
		for(unsigned int i = 0; i < this->backupSumCtr.size(); ++i) {
			duals.backupSumDuals[i] = (*ctr).getDual();
			++ctr;
		}

		duals.primaryOverlapDuals.resize(this->primaryOverlapCtr.size(), 0.0);
		ctr = this->primaryOverlapCtr.begin();
		for(unsigned int i = 0; i < this->primaryOverlapCtr.size(); ++i) {
			duals.primaryOverlapDuals[i] = (*ctr).getDual();
			++ctr;
		}

		duals.backupOverlapDuals.resize(this->backupOverlapCtr.size(), 0.0);
		ctr = this->backupOverlapCtr.begin();
		for(unsigned int i = 0; i < this->backupOverlapCtr.size(); ++i) {
			duals.backupOverlapDuals[i] = (*ctr).getDual();
			++ctr;
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
		++this->data->n_mappings;
		m->globalMappingNumber = this->data->n_mappings;

		// create new w variable
		this->w_useMappingVars.push_back(
			this->master_problem.newVar(
				XPRBnewname("m_use_mapping_%d", m->globalMappingNumber),
				XPRB_BV, 0, 1
			)
		);

		// pointer to the newly created w variable
		XPRBvar *w = &this->w_useMappingVars.back();

		// objective function
		this->z_objective -= Parameters::E_PrimaryPathCost(m->primary) * (*w);

		// serve customer constraint
		list<XPRBctr>::iterator ctr_itr = this->serveCustomerCtr.begin();
		/* find the serve customer constraint for owner service */
		for(int cc = 0; cc < this->data->n_customers; ++cc) {
			customer *c = &this->data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service *s = &c->services[ss];
				if(s == owner) {

					/* add w variable to owners serve customer constraint */
					*ctr_itr += *w;
				}
				++ctr_itr;
			}
		}

		// arc cap constraint
		/* for every arc */
		ctr_itr = this->arcCapacityCtr.begin();
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/* add w term to arc cap constraint */
			*ctr_itr += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) * (*w);

			++ctr_itr;
		}

		// single backup constraint
		/* for every arc */
		ctr_itr = this->backupSingleCtr.begin();
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/* find owners */
			for(int ss = 0; ss < this->data->n_services; ++ss) {
				/* add w term to single backup constraint for arc */
				*ctr_itr += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * (*w);

				++ctr_itr;
			}
		}

		// sum backup constraint
		/* for every arc */
		ctr_itr = this->backupSumCtr.begin();
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/* add w term to single backup constraint for arc */
			*ctr_itr += this->beta * Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) * (*w);

			++ctr_itr;
		}


		// primary overlap constraint
		/* for every service pair where owner service takes part */
		ctr_itr = this->primaryOverlapCtr.begin();
		int snum1 = 0;
		for(int cc1 = 0; cc1 < this->data->n_customers; ++cc1) {
			customer * c1 = &data->customers[cc1];
			for(unsigned int ss = 0; ss < c1->services.size(); ++ss) {
				service *s = &c1->services[ss];
				++snum1;
				int snum2 = 0;
				for(int cc2 = 0; cc2 < this->data->n_customers; ++cc2) {
					customer * c2 = &data->customers[cc2];
					for(unsigned int tt = 0; tt < c2->services.size(); ++tt) {
						service *t = &c2->services[tt];
						++snum2;
						if(snum1 < snum2) {

							/* for every arc */
							for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {

								if(s == owner || t == owner) {
									arc *a = &this->data->network.arcs[aa];

									/* check if mapping uses arc for primary */
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0) {
										/* add w var to overlap constraint if arc is used */
										*ctr_itr = *w;
									}
								}
								++ctr_itr;
							}
						}
					}
				}
			}
		}

		// backup overlap constraint
		/* for every service pair where owner service takes part */
		ctr_itr = this->backupOverlapCtr.begin();
		snum1 = 0;
		for(int cc1 = 0; cc1 < this->data->n_customers; ++cc1) {
			customer * c1 = &data->customers[cc1];
			for(unsigned int ss = 0; ss < c1->services.size(); ++ss) {
				service *s = &c1->services[ss];
				++snum1;
				int snum2 = 0;
				for(int cc2 = 0; cc2 < this->data->n_customers; ++cc2) {
					customer * c2 = &data->customers[cc2];
					for(unsigned int tt = 0; tt < c2->services.size(); ++tt) {
						service *t = &c2->services[tt];
						++snum2;
						if(snum1 < snum2) {

							/* for every arc */
							for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {

								if(s == owner || t == owner) {
									arc *a = &this->data->network.arcs[aa];

									/* check if mapping uses arc for backup */
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0) {
										/* add w var to overlap constraint if arc is used */
										*ctr_itr = *w;
									}
								}
								++ctr_itr;
							}
						}
					}
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
		int dual_index = 0;
		/* find the serve customer constraint for owner service */
		for(int cc = 0; cc < this->data->n_customers; ++cc) {
			customer *c = &this->data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service *s = &c->services[ss];
				if(s == owner) {

					/* add dual cost from owner's serve customer constraint */
					At_y += duals->serveCustomerDuals[dual_index];
				}
				++dual_index;
			}
		}

		// dual price primary
		/* for every arc */
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/* add dual cost for use on this arc for this mapping */
			At_y += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) * duals->arcCapacityDuals[aa];
		}

		// dual price sum backup
		/* for every arc */
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/* add dual cost for backup use on arc for mapping */
			double backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);
			At_y += this->beta * backup_usage * duals->backupSumDuals[aa];
		}

		// dual price single backup
		/* for every arc */
		dual_index = 0;
		for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
			arc * a = &this->data->network.arcs[aa];

			/*find owner service's dual val for arc*/
			for(int ss = 0; ss < this->data->n_services; ++ss) {

				double backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);
				At_y += backup_usage * duals->backupSingleDuals[dual_index];

				++dual_index;
			}
		}


		// dual price overlaps
		/* for every service pair where owner service takes part */
		int snum1 = 0;
		dual_index = 0;
		for(int cc1 = 0; cc1 < this->data->n_customers; ++cc1) {
			customer * c1 = &data->customers[cc1];
			for(unsigned int ss = 0; ss < c1->services.size(); ++ss) {
				service *s = &c1->services[ss];
				++snum1;
				int snum2 = 0;
				for(int cc2 = 0; cc2 < this->data->n_customers; ++cc2) {
					customer * c2 = &data->customers[cc2];
					for(unsigned int tt = 0; tt < c2->services.size(); ++tt) {
						service *t = &c2->services[tt];
						++snum2;
						if(snum1 < snum2) {
							if(s == owner || t == owner) {

								/* for every arc */
								for(unsigned int aa = 0; aa < this->data->network.arcs.size(); ++aa) {
									arc *a = &this->data->network.arcs[aa];

									/* check if mapping uses arc for primary */
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0) {
										/* add primary dual price for service pair on arc */
										At_y += duals->primaryOverlapDuals[dual_index];
									}

									/* check if mapping uses arc for backup */
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0) {
										/* add backup dual price for service pair on arc */
										At_y += duals->backupOverlapDuals[dual_index];
									}
									++dual_index;
								}

							} else {
								// skip all duals for this service pair
								dual_index += this->data->network.arcs.size();
							}
						}
					}
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
		list<XPRBvar>::iterator y_itr;
		list<XPRBvar>::iterator w_itr;
		list<XPRBvar>::iterator d_itr;

		cout << "\n=========== RESULTS =============\n";

		cout << "\nProfits: " << this->master_problem.getObjVal() << "\n";

		double backup_costs = 0.0;
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc *a = &this->data->network.arcs[aa];
			backup_costs += a->bandwidth_price * (*d_itr).getSol();
			++d_itr;
		}
		cout << "\nBackup costs: " << backup_costs << "\n";

		y_itr = this->y_serveCustomerVars.begin();
		w_itr = this->w_useMappingVars.begin();
		int serviceNumber = 0;
		for(int cc = 0; cc < n_customers; ++cc) {
			customer *c = &this->data->customers[cc];
			if((*y_itr).getSol() > 0.01) {
				cout << "\nCustomer #" << cc+1 << " is being served (" << (*y_itr).getSol() << ")\n";
				cout << "- Revenue: " << c->revenue << "\n";
			}
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service *s = &c->services[ss];
				++serviceNumber;
				for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
					if((*w_itr).getSol() > 0.01) {
						mapping *m = &s->possible_mappings[mm];
						cout << " - Service #" << serviceNumber << " -> mapping #" << m->globalMappingNumber << "\n";
						cout << "  - primary path: " << m->primary->pathNumber << ", cost: " << m->primary->cost << "\n";
						if(m->backup != NULL) {
							cout << "  - backup path: " << m->backup->pathNumber << "\n";
						}
					}
					++w_itr;
				}
			}
			++y_itr;
		}

		cout << "\nArc backup reservations (non-zero)\n";
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) {
			if((*d_itr).getSol() > 0.00001) {
				arc *a = &this->data->network.arcs[aa];
				cout << "(" << a->startNode+1 << ", " << a->endNode+1 << ") : " << (*d_itr).getSol() << "\n";
			}
			++d_itr;
		}
	}

	XPRBvar* CloudBrokerModel::mappingVarForMappingNumber(int mappingNumber) {
		list<XPRBvar>::iterator w_itr = this->w_useMappingVars.begin();
		for(int i = 1; i < mappingNumber; i++) {
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
