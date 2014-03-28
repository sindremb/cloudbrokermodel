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
			static double U_PrimaryBandwidthUsageOnArcForMapping(arc * a, mapping * m);
			static double Q_BackupBandwidthUsageOnArcForMapping(arc * a, mapping * m);
			static double R_RevenueForCustomer(customer * c);
			static double E_PrimaryPathCost(returnPath * k);
			static double E_PerBandwidthCostForArc(arc * a);
			static double F_BandwidthCapacityForArc(arc * a);
	};

	CloudBrokerModel::CloudBrokerModel() :
			// member initialisation
			_prob("Cloud Broker Optimisation")
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

		this->_data = data;

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
				this->_prob.newVar(XPRBnewname("y_serve_customer_%d", cc+1), XPRB_BV, 0, 1)
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
				this->_prob.newVar(
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
					this->_prob.newVar(
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
				this->_prob.newVar(
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

		/* Set expression as objective */
		this->z_objective = this->_prob.newCtr("OBJ", z_obj_expression);
		this->_prob.setObj(this->z_objective);

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
		for(int cc = 0; cc < n_customers; ++cc) {
			customer * c = &data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service * s = &c->services[ss];
				++serviceNumber;

				/* Build lhs terms */
				XPRBexpr map_service_exp;
				for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
					map_service_exp += (*w_itr);
					++w_itr;
				}

				/* create constraint */
				this->serveCustomerCtr.push_back(
					this->_prob.newCtr(
							XPRBnewname("serve_customer_ctr_%d", serviceNumber),
							map_service_exp == (*y_itr)
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
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc * a = &data->network.arcs[aa];

			/* build lhs terms */
			XPRBexpr arc_bw_usage;

			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = &c->services[ss];
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping * m = &s->possible_mappings[mm];
						arc_bw_usage += Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a,m)*(*w_itr);
						++w_itr;
					}
				}
			}

			arc_bw_usage += (*d_itr);

			/* create constraint */
			this->arcCapacityCtr.push_back(
				this->_prob.newCtr(
					XPRBnewname("arc_capacity_ctr_%d_%d", a->startNode, a->endNode),
					arc_bw_usage <= a->bandwidth_cap
				)
			);
			++d_itr;
		}
		cout << "DONE! (" << this->arcCapacityCtr.size() << " created) \n";

		/* SINGLE BACKUP RESERVATION CONSTRAINT
		 * - backup capacity reserved on an arc must be large enough to support any single backup
		 *   path using that arc
		 *
		 *   for: every customer
		 *   	for: every customer's service
		 *   		for: every arc
		 */
		cout <<	"  - SINGLE BACKUP RESERVATION CONSTRAINTS..";
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) { 								/* for every arc		*/
			arc * a = &data->network.arcs[aa];
			serviceNumber = 0;
			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {						/* for every customer 	*/
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {	/* for every service 	*/
					service * s = &c->services[ss];
					++serviceNumber;
					XPRBexpr service_backup_req_on_arc;
					/* build lhs terms */
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping* m = &s->possible_mappings[mm];
						service_backup_req_on_arc += (Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m))*(*w_itr);
						++w_itr;
					}

					/* create constraint */
					this->backupSingleCtr.push_back(
						this->_prob.newCtr(
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
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) { 							/* for every arc		*/
			arc * a = &data->network.arcs[aa];
			XPRBexpr total_backup_req;

			w_itr = this->w_useMappingVars.begin();
			for(int cc = 0; cc < n_customers; ++cc) {
				customer * c = &data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service * s = &c->services[ss];
					for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
						mapping *m = &s->possible_mappings[mm];
						total_backup_req += Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m)*(*w_itr);
						++w_itr;
					}
				}
			}

			this->backupSumCtr.push_back(
				this->_prob.newCtr(
					XPRBnewname("backup_sum_ctr_%d_%d", a->startNode, a->endNode),
					beta_backupres * total_backup_req <= (*d_itr)
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
							for(int aa = 0; aa < n_arcs; ++aa) {
								arc *a = &data->network.arcs[aa];
								XPRBexpr primary_overlap;
								for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
									mapping * m = &s->possible_mappings[mm];
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {
										primary_overlap += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}
								for(unsigned int mm = 0; mm < t->possible_mappings.size(); ++mm) {
									mapping * m = &t->possible_mappings[mm];
									if(Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m) > 0.0) {
										primary_overlap += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}
								primary_overlap -= (*l_itr);
								this->primaryOverlapCtr.push_back(
									this->_prob.newCtr(
										XPRBnewname("primary_overlap_ctr_%d_%d", a->startNode, a->endNode),
										primary_overlap <= 1.0
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
							for(int aa = 0; aa < n_arcs; ++aa) {
								arc *a = &data->network.arcs[aa];
								XPRBexpr backup_overlap;
								for(unsigned int mm = 0; mm < s->possible_mappings.size(); ++mm) {
									mapping * m = &s->possible_mappings[mm];
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {
										backup_overlap += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}
								for(unsigned int mm = 0; mm < t->possible_mappings.size(); ++mm) {
									mapping * m = &t->possible_mappings[mm];
									if(Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m) > 0.0) {
										backup_overlap += *this->mappingVarForMappingNumber(m->globalMappingNumber);
									}
								}
								backup_overlap += (*l_itr);
								this->backupOverlapCtr.push_back(
									this->_prob.newCtr(
										XPRBnewname("backup_overlap_ctr_%d_%d", a->startNode, a->endNode),
										backup_overlap <= 2.0
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
		this->_prob.setSense(XPRB_MAXIM);

		return;
	}

	void CloudBrokerModel::RunModel(bool enforce_integer) {
		if(enforce_integer) {
			this->_prob.mipOptimise();
		} else {
			this->_prob.lpOptimise();
		}
	}

	void CloudBrokerModel::RunModelColumnGeneration() {
		int itercount = 0;
		XPRSsetintcontrol(this->_prob.getXPRSprob(), XPRS_CUTSTRATEGY, 0);	/* Disable automatic cuts - we use our own */
		XPRSsetintcontrol(this->_prob.getXPRSprob(), XPRS_PRESOLVE, 0);		/* Switch presolve off */
		while(itercount < 100) {
			bool foundColumn = false;
			this->RunModel(false);
			XPRBbasis basis = this->_prob.saveBasis();

			CloudBrokerModel::dual_vals duals = this->getDualVals();

			// use duals to generate column(s) for each service
			for(int cc = 0; cc < this->_data->n_customers; ++cc) {
				customer *c = &this->_data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service *s = &c->services[ss];
					if(this->generateMappingColumnBruteForce(s, &duals)) foundColumn = true;
				}
			}

			if(!foundColumn) break; /* no new column was found -> end column generation */

			this->_prob.loadBasis(basis);
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

	void CloudBrokerModel::addMappingToModel(entities::mapping *m, entities::service *s) {

	}

	double CloudBrokerModel::_bruteForceEvalMapping(entities::mapping *m, entities::service *owner, dual_vals *duals) {
		// c
		double eval = m->primary->cost;

		// - A^Ty:
		//   + a_s
		int dual_index = 0;
		for(int cc = 0; cc < this->_data->n_customers; ++cc) {
			customer *c = &this->_data->customers[cc];
			for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
				service *s = &c->services[ss];
				if(s == owner) {
					eval += duals->serveCustomerDuals[dual_index];
				}
				++dual_index;
			}
		}

		// dual price primary

		// dual price backup

		// dual price primary overlap

		// dual price backup overlap
		return 0.0;
	}

	void CloudBrokerModel::OutputResults() {
		/* Problem dimensions */
		int n_customers = this->_data->n_customers;
		int n_arcs = this->_data->network.arcs.size();

		/* Model variable iterators */
		list<XPRBvar>::iterator y_itr;
		list<XPRBvar>::iterator w_itr;
		list<XPRBvar>::iterator d_itr;

		cout << "\n=========== RESULTS =============\n";

		cout << "\nProfits: " << this->_prob.getObjVal() << "\n";

		double backup_costs = 0.0;
		d_itr = this->d_arcBackupUsage.begin();
		for(int aa = 0; aa < n_arcs; ++aa) {
			arc *a = &this->_data->network.arcs[aa];
			backup_costs += a->bandwidth_price * (*d_itr).getSol();
			++d_itr;
		}
		cout << "\nBackup costs: " << backup_costs << "\n";

		y_itr = this->y_serveCustomerVars.begin();
		w_itr = this->w_useMappingVars.begin();
		int serviceNumber = 0;
		for(int cc = 0; cc < n_customers; ++cc) {
			customer *c = &this->_data->customers[cc];
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
				arc *a = &this->_data->network.arcs[aa];
				cout << "(" << a->startNode << ", " << a->endNode << ") : " << (*d_itr).getSol() << "\n";
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
