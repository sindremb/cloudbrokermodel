/*
 * ColumnGeneration.cpp
 *
 *  Created on: May 25, 2014
 *      Author: sindremo
 */

#include "entities.h"
#include "ColumnGeneration.hpp"
#include "CloudBrokerModel.hpp"
#include "spprc.h"

#include <iostream>

#ifndef EPS
#define EPS 1e-6
#endif

#ifndef Inf
#define Inf 1e10
#endif

using namespace entities;
using namespace std;

namespace cloudbrokeroptimisation {

	/************************* AbstractColumnGenerator **************************/

	/*
	 * AbstractColumnGenerator Constructor
	 */
	AbstractColumnGenerator::AbstractColumnGenerator(CloudBrokerModel * m, entities::dataContent * d)
	{
		this->model = m;
		this->data = d;
	}

	/************************* BruteForceColumnGenerator ***************************/

	BruteForceColumnGenerator::BruteForceColumnGenerator(CloudBrokerModel * m, entities::dataContent * d)
		: AbstractColumnGenerator(m, d)
	{

	}

	bool BruteForceColumnGenerator::GenerateColumnsForService(customer *c, service *s, dual_vals *duals)
	{
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
					double eval = this->_evalMapping(&m, s, duals);
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
						double k_and_b = entities::prob_paths_a_and_b(k, b);
						if(k->exp_availability + b->exp_availability - k_and_b >= s->availability_req) {
							// combination of a as primary and b as backup is feasible -> add routing
							mapping m;
							m.primary = k;
							m.backup = b;
							double eval = this->_evalMapping(&m, s, duals);
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
			cout << "--> NEW MAPPING (BF): " << best_eval << "\n";
			this->model->AddMappingToModel(&bestFound, s);
			return true;
		}
		return false;
	}

	double BruteForceColumnGenerator::_evalMapping(mapping *m, service *owner, dual_vals *duals) {

		double beta = this->model->GetBeta();

		/**** c ****/
		double c = - m->primary->cost; // includes cost of placement and bandwidth usage on arcs

		/**** A^Ty: *****/
		double At_y = 0.0;

		//   a_s
		// subtract dual cost from owner's serve customer constraint
		// all mapping vars have -1 as coefficient in serve customer ctr
		At_y -= duals->serveCustomerDuals[owner->globalServiceIndex];


		/* for every arc [/ link (primary overlap ctr)] */
		int link_index = 0;
		for(int aa = 0; aa < data->n_arcs; ++aa) {
			arc * a = &data->arcs[aa];

			// get primary path usage on arc
			double u_primary_usage = Parameters::U_PrimaryBandwidthUsageOnArcForMapping(a, m);
			// get backup usage on arc
			double q_backup_usage = Parameters::Q_BackupBandwidthUsageOnArcForMapping(a, m);

			/* DUAL PRICE: TOTAL CAPACITY CTR (PRIMARY)
			 * add dual cost for use on this arc for this mapping
			 * mapping coef. is equal to primary path's usage
			 */
			At_y += u_primary_usage * duals->arcCapacityDuals[aa];

			/* DUAL PRICE: SUM BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping with beta factor
			 * mapping coef. is equal to backup path's usage * beta
			 */
			At_y += beta * q_backup_usage * duals->backupSumDuals[aa];

			/* DUAL PRICE: SINGLE BACKUP CTR (BACKUP)
			 * add dual cost for backup use on arc for mapping
			 * mapping coef. is equal to backup path's usage
			 */
			At_y += q_backup_usage * duals->backupSingleDuals[aa][owner->globalServiceIndex];

			/* DUAL PRICE: PRIMARY OVERLAP CTR (PRIMARY)
			 * add overlap dual price if primary path uses LINK for each service pair where owner takes part (usage > 0)
			 * mapping coef. 1 only when actually using LINK for primary, 0 otherwise
			 */
			if(a->startNode < a->endNode) { // per link only: arcs where startNode < endNode
				if(u_primary_usage >= EPS) {
					/* for every service pair where owner service takes part */
					int tt, ss;
					ss = owner->globalServiceIndex;
					for(tt = ss+1; tt < data->n_services; ++tt) {
						At_y += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
					}
					tt = owner->globalServiceIndex;
					for(ss = 0; ss < tt; ++ss) {
						At_y += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
					}
				}
				++link_index;
			}

			/* DUAL PRICE: BACKUP OVERLAP CTR (BACKUP)
			 * add overlap dual price if backup path uses arc for each service pair where owner takes part (usage > 0)
			 * mapping coef. 1 only when actually using arc capacity for backup, 0 otherwise
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

	/************************** Heuristic A / B Common **************************/

	/* Returns a list of costs for each arc in the loaded problem
	 * - The cost returned is the cost associated with choosing an arc in the up-link (thus indirectly
	 *   selecting it's return arc for the down-link.
	 */
	vector<double> _dualPrimaryArcCostsForService(entities::service *s, dual_vals *duals, dataContent *data) {
		vector<double> arc_costs(data->n_arcs, 0.0);

		/* for every arc (with return arc) [for every LINK (primary overlap ctr dual cost)]*/
		int link_index = 0;
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

			if(a->startNode < a->endNode) { // once per link only: arcs where startNode < endNode

				/* DUAL PRICE: PRIMARY OVERLAP CTR (PRIMARY)
				 * add primary overlap dual price for this LINK as cost to both arcs of this link,
				 * for each service pair where owner service takes part
				 */
				int tt, ss;
				ss = s->globalServiceIndex;
				for(tt = ss+1; tt < data->n_services; ++tt) {
					arc_costs[aa	   ] += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
					arc_costs[aa_return] += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
				}
				tt = s->globalServiceIndex;
				for(ss = 0; ss < tt; ++ss) {
					arc_costs[aa	   ] += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
					arc_costs[aa_return] += duals->primaryOverlapDuals[link_index][ss][tt-ss-1];
				}

				++link_index;
			}
		}

		return arc_costs;
	}

	vector<double> _dualBackupArcCostsForService(entities::service *s, dual_vals *duals, returnPath *primary, dataContent * data, double beta) {
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

	returnPath _returnPathFromArcs(list<arc*>* arcs, service* owner, placement* p) {
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

	/************************* HeuristicAColumnGenerator ***************************/

	HeuristicAColumnGenerator::HeuristicAColumnGenerator(CloudBrokerModel * m, entities::dataContent * d)
		: AbstractColumnGenerator(m, d)
	{

	}

	bool HeuristicAColumnGenerator::GenerateColumnsForService(customer *c, service *s, dual_vals *duals) {

		double beta = this->model->GetBeta();

		// extract serve customer dual for this service's customer
		// - this is the only value that can give a positive evaluation for a new mapping
		double serve_customer_dual = duals->serveCustomerDuals[s->globalServiceIndex];
		// - if not a "negative cost", new mapping can not have a positive evaluation, return false
		if(serve_customer_dual <= 0.0) {
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
		vector<double> arc_costs_primary = _dualPrimaryArcCostsForService(s, duals, data);

		// mark all arcs to be without restriction for primary path
		vector<int> arc_restrictions_primary(data->n_arcs, 0);

		// for each possible placement of this service
		for(unsigned int pp = 0; pp < s->possible_placements.size(); ++pp) {

			placement *p = &s->possible_placements.at(pp);
			int placement_node = data->n_nodes - data->n_providers + p->globalProviderIndex;
			int customer_node = c->globalCustomerIndex;

			// find primary path by shortest path problem
			list<arc*> primary_up_arcs;
			double primary_path_eval  = spprc(customer_node, placement_node, data->n_nodes, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, 0.0, &primary_up_arcs);

			// see if a path was found at all -> if not, go to next iteration
			if(primary_up_arcs.size() == 0) continue;

			// check if found primary path can be basis for a profitable column
			if(serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);

				// IF availability req ok
				if(primary.exp_availability > s->availability_req) {
					// create new mapping
					mapping m;

					// add primary path to paths list, then add its pointer to mapping
					data->paths.push_back(primary);
					m.primary = &data->paths.back();
					m.backup = NULL;

					// add new mapping to model and return
					cout << "---> NEW MAPPING (HA): " << serve_customer_dual - primary_path_eval - p->price << "\n";
					this->model->AddMappingToModel(&m, s);
					return true;
				} else {
					// ELSE: try finding primary/backup combo

					// extract arc costs with dual values for a backup path
					vector<double> arc_costs_backup = _dualBackupArcCostsForService(s, duals, &primary, data, beta);

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
						double backup_path_eval = spprc(customer_node, placement_node, data->n_nodes, &node_arcs,
														 &arc_costs_backup, &arc_restrictions_backup, s->latency_req,
														 max_restricted_arcs_backup, 0.0, &backup_up_arcs);

						if(backup_up_arcs.size() == 0) {
							--max_restricted_arcs_backup;
							continue;
						}

						// IF total evaluation is positive
						if(serve_customer_dual - primary_path_eval - backup_path_eval - p->price >= EPS) {

							// create a return path object and calculate combo availability
							returnPath backup = _returnPathFromArcs(&backup_up_arcs, s, p);
							double p_and_b = entities::prob_paths_a_and_b(&primary, &backup);

							// IF availability req ok
							if(primary.exp_availability + backup.exp_availability - p_and_b >= s->availability_req) {
								// create a new mapping
								mapping m;

								// add found paths to paths list, then add their pointers to the mapping
								data->paths.push_back(primary);
								m.primary = &data->paths.back();
								data->paths.push_back(backup);
								m.backup = &data->paths.back();

								// add mapping to model and return
								cout << "--> NEW MAPPING (HB): " << serve_customer_dual - primary_path_eval - backup_path_eval - p->price << "\n";
								this->model->AddMappingToModel(&m, s);
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


	/************************* HeuristicBColumnGenerator ***************************/
	HeuristicBColumnGenerator::HeuristicBColumnGenerator(CloudBrokerModel * m, entities::dataContent * d)
		: AbstractColumnGenerator(m, d)
	{

	}

	bool HeuristicBColumnGenerator::GenerateColumnsForService(customer *c, service *s, dual_vals *duals) {

		/*********************** SETUP *****************************/

		double beta = this->model->GetBeta();

		// extract serve customer dual for this service's customer
		// - this is the only value that can give a positive evaluation for a new mapping
		double serve_customer_dual = duals->serveCustomerDuals[s->globalServiceIndex];
		// - if not a "negative cost", new mapping can not have a positive evaluation, return false
		if(serve_customer_dual <= 0.0) {
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
		vector<double> arc_costs_primary = _dualPrimaryArcCostsForService(s, duals, data);

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
			double primary_path_eval  = spprc(customer_node, placement_node, data->n_nodes, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, s->availability_req, &primary_up_arcs);
			// see if a path was found at all and is profitable
			if(primary_up_arcs.size() > 0 && serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);
				// create new mapping
				mapping m;

				// add primary path to paths list, then add its pointer to mapping
				data->paths.push_back(primary);
				m.primary = &data->paths.back();
				m.backup = NULL;

				// add new mapping to model and return
				cout << "--> NEW MAPPING (HB): " << serve_customer_dual - primary_path_eval - p->price << "\n";
				this->model->AddMappingToModel(&m, s);
				return true;
			}

			// find primary path to combine with backup by shortest path problem with resource constraints
			double availability_lbd = max((s->availability_req - availability_ubd) / (1 - availability_ubd), 0.0);
			primary_up_arcs.clear();
			primary_path_eval  = spprc(customer_node, placement_node, data->n_nodes, &node_arcs,
									   &arc_costs_primary, &arc_restrictions_primary, s->latency_req,
									   1, availability_lbd, &primary_up_arcs);

			// see if a path was found at all -> if not, go to next iteration
			if(primary_up_arcs.size() == 0) continue;

			// check if found primary path can be basis for a profitable column
			if(serve_customer_dual - primary_path_eval - p->price >= EPS) {
				returnPath primary = _returnPathFromArcs(&primary_up_arcs, s, p);

				// if primary if availability feasible, it does not need a backup and will be generated in above code -> skip
				if(primary.exp_availability >= s->availability_req) {
					continue;
				}

				// try finding primary/backup combo
				double min_availability = (s->availability_req - primary.exp_availability) / (1 - primary.exp_availability);

				// extract arc costs with dual values for a backup path
				vector<double> arc_costs_backup = _dualBackupArcCostsForService(s, duals, &primary, data, beta);

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
					double backup_path_eval = spprc(customer_node, placement_node, data->n_nodes, &node_arcs,
													 &arc_costs_backup, &arc_restrictions_backup, s->latency_req,
													 max_restricted_arcs_backup, min_availability, &backup_up_arcs);

					if(backup_up_arcs.size() == 0) {
						--max_restricted_arcs_backup;
						continue;
					}

					// IF total evaluation is positive
					if(serve_customer_dual - primary_path_eval - backup_path_eval - p->price >= EPS) {

						// create a return path object and calculate combo availability
						returnPath backup = _returnPathFromArcs(&backup_up_arcs, s, p);
						double p_and_b = entities::prob_paths_a_and_b(&primary, &backup);

						// IF availability req ok
						if(primary.exp_availability + backup.exp_availability - p_and_b >= s->availability_req) {
							// create a new mapping
							mapping m;

							// add found paths to paths list, then add their pointers to the mapping
							data->paths.push_back(primary);
							m.primary = &data->paths.back();
							data->paths.push_back(backup);
							m.backup = &data->paths.back();

							// add mapping to model and return
							cout << "--> NEW MAPPING (HB): " << serve_customer_dual - primary_path_eval - backup_path_eval - p->price << "\n";
							this->model->AddMappingToModel(&m, s);
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

}


