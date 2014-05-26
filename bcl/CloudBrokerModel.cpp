/*
 * CloudBrokerModel.cpp
 *
 *  Created on: May 25, 2014
 *      Author: sindremo
 */

#include "entities.h"
#include "CloudBrokerModel.hpp"
#include "xprb_cpp.h"
#include "xprs.h"
#include <list>
#include <vector>
#include <iostream>

#ifndef EPS
#define EPS 1e-6
#endif

using namespace entities;
using namespace ::dashoptimization;
using namespace std;

namespace cloudbrokeroptimisation {

	/* CloudBrokerModel constructor
	 * - Includes necessary class member initialisations
	 */
	CloudBrokerModel::CloudBrokerModel()
	: master_problem("Cloud Broker Optimisation")
	{
		this->basis = NULL;
	}

	/******  BuildModel *******
	 * params:
	 * - dataContent* data: pointer to data to build model from
	 * - double beta_backupres: min fraction of total backup requirement to reserve on arc
	 *
	 * Builds the MIP-model from the provided dataContent and beta model parameter
	 */
	void CloudBrokerModel::BuildModel(dataContent * input_data, double beta_backupres, bool dedicated_only) {

		/********* SETUP **********/

		this->data = input_data;
		this->dedicated = dedicated_only;
		if(this->dedicated) {
			this->beta = 1.0; // dedicated protection scheme -> beta = 1.0
		} else {
			this->beta = beta_backupres;
		}

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
		 * ] (global mapping list is ordered accordingly)
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
		if(!this->dedicated) {
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
		}

		/* Arc Backup Usage variable
		 * for: every arc a
		 */
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];
			lambda_arcBackupRes.push_back(
				master_problem.newVar(
					XPRBnewname("lambda_backup_res_%d_%d", a->startNode, a->endNode),
					XPRB_PL, 0, a->bandwidth_cap
				)
			);
		}
		cout << "   - created " << lambda_arcBackupRes.size() << " lambda-variables\n";

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
			z_obj_expression -= Parameters::E_PerBandwidthCostForArc(a)*lambda_arcBackupRes[aa];
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

				/* add serve customer var */
				map_service_expr += y_serveCustomerVars[cc];

				/* for every mapping of service*/
				for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {

					/* subtract mapping selection var
					* - mapping selection var w is created in same order, can therefore loop over global w list
					*/
					map_service_expr -= (*w_itr);

					++w_itr;
				}

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
			arc_bw_usage += lambda_arcBackupRes[aa];

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
		 *   		for: every customer's service
		 *
		 *	When using dedicated protection scheme -> beta = 1.0 -> this constraint is made irrelevant by SUM BACKUP RESERVATION CONSTRAINT
		 */
		if(!this->dedicated) {
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
						service_backup_req_on_arc -= lambda_arcBackupRes[aa];

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
		}

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
			total_backup_req_expr-= lambda_arcBackupRes[aa];

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
		 *
		 * NOTE: not needed for dedicated protection scheme as backup collision will never happen
		 */
		if(!this->dedicated) {
			cout << "  - PRIMARY OVERLAP CONSTRAINTS..";
			int primaryOverlapCtrCount = 0;

			/* for every LINK
			* ( for every arc where startNode < endNode )
			*/
			primaryOverlapCtr.resize(data->n_arcs/2); // half as many links as arcs
			int link_index = 0;
			for(int aa = 0; aa < data->n_arcs; ++aa) {

				arc *a = &data->arcs[aa];
				if(a->startNode < a->endNode) { // one arc per link: arcs where startNode < endNode

					primaryOverlapCtr[link_index].resize(data->n_services-1);

					/* for every pair of two services (s, t) */
					for(int ss = 0; ss < data->n_services-1; ++ss) {
						service *s = &data->services[ss];
						primaryOverlapCtr[link_index][ss].reserve(data->n_services-1-ss);
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
							primaryOverlapCtr[link_index][ss].push_back(
								master_problem.newCtr(
									XPRBnewname(
										"primary_overlap_ctr_%d_%d_%d_%d",
										a->startNode, a->endNode, ss, tt
									),
									primary_overlap_expr <= 1.0
								)
							);
							++primaryOverlapCtrCount;
						}
					}

					++link_index;
				}
			}
			cout << "DONE! (" << primaryOverlapCtrCount << " created)\n";
		}

		/* BACKUP OVERLAP CONSTRAINT
		 * - if two services' primary paths overlap, their backup paths can not have bandwidth requirements at
		 *   the same arc
		 *
		 *   for: every pair of two services services
		 *
		 * NOTE: not needed for dedicated protection scheme as backup collision will never happen
		 */
		if(!this->dedicated) {
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
		}

		// set problem to maximise objective
		master_problem.setSense(XPRB_MAXIM);

		return;
	}

	void CloudBrokerModel::RunModel(bool enforce_integer, int time_limit, const char *opt_alg) {

		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_MAXTIME, time_limit);

		if(enforce_integer) {
			if(string(opt_alg) == " ") {
				master_problem.mipOptimise();
			} else {
				master_problem.mipOptimise(opt_alg);
			}
		} else {
			if(string(opt_alg) == " ") {
				master_problem.lpOptimise();
			} else {
				master_problem.lpOptimise(opt_alg);
			}
		}
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
			backup_costs += a->bandwidth_price * lambda_arcBackupRes[aa].getSol();
		}
		stream << "\nBackup costs = " << backup_costs << "\n";

		stream << "\nBackup beta = " << beta << "\n";

		stream << "\nNumber of mappings = " << data->n_mappings << "\n";

		double total_backup_requirement = 0.0;
		int service_count = 0;
		int customer_count = 0;
		int backup_count = 0;
		for(int cc = 0; cc < data->n_customers; ++cc) {
			customer *c = &data->customers[cc];
			if(y_serveCustomerVars[cc].getSol() > 0.01) {
				customer_count++;
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
							service_count++;
							stream <<  "  -> mapping #" << m->globalMappingIndex+1 << "\n";
							stream << "   - primary path: " << m->primary->globalPathIndex+1 << ", cost: " << m->primary->cost << "\n";
							stream << "    - nodes up: " << m->primary->arcs_up.front()->startNode+1;
							for(list<arc*>::iterator a_itr = m->primary->arcs_up.begin(), a_end = m->primary->arcs_up.end(); a_itr != a_end; ++a_itr) {
								stream << "->" << (*a_itr)->endNode+1;
							}
							stream << "\n";
							if(m->backup != NULL) {
								backup_count++;
								stream << "   - backup path: " << m->backup->globalPathIndex+1 << "\n";
								stream << "    - nodes up: " << m->backup->arcs_up.front()->startNode+1;
								for(list<arc*>::iterator a_itr = m->backup->arcs_up.begin(), a_end = m->backup->arcs_up.end(); a_itr != a_end; ++a_itr) {
									stream << "->" << (*a_itr)->endNode+1;
									total_backup_requirement += Parameters::Q_BackupBandwidthUsageOnArcForMapping(*a_itr, m)
											+ Parameters::Q_BackupBandwidthUsageOnArcForMapping((*a_itr)->return_arc, m);
								}
								stream << "\n";
							}
						}
					}
				}
			}
		}

		double total_backup_reserved = 0.0;
		stream << "\nArc backup reservations (non-zero)\n";
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			if(lambda_arcBackupRes[aa].getSol() > 0.00001) {
				arc *a = &data->arcs[aa];
				total_backup_reserved += lambda_arcBackupRes[aa].getSol();
				stream << "(" << a->startNode+1 << "," << a->endNode+1 << ") = " << lambda_arcBackupRes[aa].getSol() << "\n";
			}
		}

		stream << "\nTotal customers served: " << customer_count
				<< "\nTotal services provided: " << service_count
				<< "\nNumber of services with backup: " << backup_count
				<< " ("<< (service_count > 0 ? backup_count * 100.0 / service_count : 100.0) << "%)\n";

		double coverage = total_backup_requirement >= EPS ? total_backup_reserved * 100.0 / total_backup_requirement : 100.0;
		stream << "\nTotal backup capacity required: " << total_backup_requirement
				<< "\nTotal backup capacity reserved: " << total_backup_reserved
				<< "\nBackup coverage: " << coverage << "%\n";
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

	dual_vals CloudBrokerModel::GetDualVals() {

		dual_vals duals;

		/********* serveCustomerCtr dual values **********/
		// make room for dual values in vector
		duals.serveCustomerDuals.resize(data->n_services, 0.0);
		// for every service
		for(int ss = 0; ss < data->n_services; ++ss) {
			// dual val for serveCustomerCtr for this service
			duals.serveCustomerDuals[ss] = serveCustomerCtr[ss].getDual();
		}

		/********* all arc related dual values ************/
		// make room for dual values (first dimension)
		duals.arcCapacityDuals.resize(data->n_arcs, 0.0);
		duals.backupSumDuals.resize(data->n_arcs, 0.0);
		duals.backupSingleDuals.resize(data->n_arcs);
		duals.primaryOverlapDuals.resize(data->n_arcs/2); // per link only
		duals.backupOverlapDuals.resize(data->n_arcs);

		// for every arc
		for(int aa = 0; aa < data->n_arcs; ++aa) {

			// dual val for arcCapacityCtr for this arc
			duals.arcCapacityDuals[aa] = arcCapacityCtr[aa].getDual();
			// dual val for backupSumCtr for this arc
			duals.backupSumDuals[aa] = backupSumCtr[aa].getDual();

			// make room for dual values (second dimension)
			duals.backupSingleDuals[aa].resize(data->n_services, 0.0);
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
				duals.backupOverlapDuals[aa][ss].resize(data->n_services-1-ss);

				// - for every second service t
				for(int tt = ss+1; tt < data->n_services; ++tt) {
					// dual val for backupOverlapCtr for arc a and service pair (s, t)
					duals.backupOverlapDuals[aa][ss][tt-ss-1] = backupOverlapCtr[aa][ss][tt-ss-1].getDual();
				}
			}
		}

		// for every link (arcs where startNode < endNode)
		int link_index = 0;
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc *a = &data->arcs[aa];
			if(a->startNode < a->endNode) {

				// make room for dual variables (second dimension)
				duals.primaryOverlapDuals[link_index].resize(data->n_services-1);

				// for every pair of two services (s, t)
				// - for every first service s
				for(int ss = 0; ss < data->n_services-1; ++ss) {

					// make room for dual values (third dimension)
					duals.primaryOverlapDuals[link_index][ss].resize(data->n_services-1-ss);

					// - for every second service t
					for(int tt = ss+1; tt < data->n_services; ++tt) {

						// dual val for primaryOverlapCtr for arc a and service pair (s, t)
						duals.primaryOverlapDuals[link_index][ss][tt-ss-1] = primaryOverlapCtr[link_index][ss][tt-ss-1].getDual();
					}

				}

				++link_index;
			}
		}

		return duals;
	}

	void CloudBrokerModel::AddMappingToModel(entities::mapping *m, entities::service *owner) {

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
		serveCustomerCtr[owner->globalServiceIndex] -= *w;

		// for every arc [/ link (primary overlap ctr)]
		int link_index = 0;
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			// get primary path usage on arc
			double u_primary_usage = Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m);
			// get backup usage on arc
			double q_backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);

			// add mapping primary path usage to arc's 'arcCap'-constraints
			arcCapacityCtr[aa] += u_primary_usage * (*w);

			// add mapping backup usage to arc's 'backupSingle'-constraint
			if(!this->dedicated) { // constraint only exists for shared protection scheme
				backupSingleCtr[aa][owner->globalServiceIndex] += q_backup_usage * (*w);
			}

			// add mapping backup usage (*beta) to arc's 'backupSum'-constraint
			backupSumCtr[aa] += beta * q_backup_usage * (*w);

			if(!this->dedicated) { // constraints only exist for shared protection scheme

				// primary ovelap ctr: only one per link -> only arcs where startNode < endNode
				if(a->startNode < a->endNode) {
					// if primary path uses capacity on arc
					if(u_primary_usage >= EPS) {
						// add mapping selection var to 'primaryOverlap'-constraints for every service pair where owner service takes part
						int tt, ss;
						ss = owner->globalServiceIndex;
						for(tt = ss+1; tt < data->n_services; ++tt) {
							primaryOverlapCtr[link_index][ss][tt-ss-1] += *w;
						}
						tt = owner->globalServiceIndex;
						for(ss = 0; ss < tt; ++ss) {
							primaryOverlapCtr[link_index][ss][tt-ss-1] += *w;
						}

					}

					++link_index;
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
	}


	double CloudBrokerModel::GetBeta() {
		return this->beta;
	}

	double CloudBrokerModel::GetObjectiveValue() {
		return this->master_problem.getObjVal();
	}
	void CloudBrokerModel::SaveBasis() {
		this->basis = master_problem.saveBasis();
		this->basis_stored = true;
	}
	bool CloudBrokerModel::LoadSavedBasis() {
		if(!this->basis_stored) {
			return false;
		}
		master_problem.loadBasis(basis);
		return true;
	}
	void CloudBrokerModel::SetColumnGenerationConfiguration(bool active) {
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_CUTSTRATEGY, active ? 0 : 1);	/* disable/enable automatic cuts */
		XPRSsetintcontrol(master_problem.getXPRSprob(), XPRS_PRESOLVE, active ? 0 : 1);		/* disable/enable presolve*/
		master_problem.setMsgLevel(active ? 1 : 3);											/* disable/enable default XPRS messages (disables: error messages only) */
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
