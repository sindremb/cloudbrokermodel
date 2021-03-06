#include <vector>
#include <list>
#include <iostream>

#include "entities.h"

namespace pathgen {
	
	using namespace entities;
	using namespace std;

	void _expandPathWithArc(returnPath * path, arc * a) {
		path->endNode = a->endNode;
		path->cost += a->bandwidth_price * path->bandwidth_usage_up 
			+ a->return_arc->bandwidth_price * path->bandwidth_usage_down;
		path->exp_availability *= a->exp_availability;
		path->latency += a->latency + a->return_arc->latency;
		path->arcs_up.push_back(a);
		path->arcs_down.push_back(a->return_arc);
		path->visitedNodes[a->endNode] = true;
	}

	vector<returnPath> _generateReturnPathsForNodePair(int start_node, int end_node, double max_latency, double bandwidth_up,
		double bandwidth_down, vector<vector<arc*>> * node_arcs, double start_cost, pathgenConfig config)
	{
		// initial incomplete path to spawn all other paths from
		returnPath initial;
		initial.startNode = start_node;
		initial.endNode = start_node;
		initial.latency = 0;
		initial.exp_availability = 1;
		initial.bandwidth_usage_up = bandwidth_up;
		initial.bandwidth_usage_down = bandwidth_down;
		initial.cost = start_cost;
		initial.visitedNodes.resize(node_arcs->size(), false);
		initial.visitedNodes[start_node] = true;

		// lists of incomplete and complete paths during path generation
		list<returnPath> incomplete;
		list<returnPath> complete;

		// add initial path to list of incomplete paths
		incomplete.push_back(initial);

		while(incomplete.size() > 0) {
			// take first incomplete path from list
			returnPath current = incomplete.front();
			incomplete.pop_front();

			// current path has reached destination -> add to complete paths list, skip to next
			if(current.endNode == end_node) {
				complete.push_back(current);
				// check if max number of paths generated -> end path generation
				if (config.maxPathsPerPlacement > 0 && complete.size() >= config.maxPathsPerPlacement) break;
				// otherwise -> skip to next iteration
				continue;
			}

			// arcs starting from last node of current path
			vector<arc*> * arcs = &node_arcs->at(current.endNode);

			// for each arc, expand current path or spawn new paths (if more than one possibility for expansion)
			for(unsigned int j = 0; j < arcs->size(); ++j) {
				// shorthands for current arc and its return arc
				arc * a_up = arcs->at(j);
				arc * a_down = a_up->return_arc;

				// non-feasibility check for expansion of arc
				if( current.visitedNodes.at(a_up->endNode) ||								// arc leads to already visited node
					(a_up->latency + a_down->latency > max_latency - current.latency) ||	// including path incurs to much latency
					(a_up->bandwidth_cap <= current.bandwidth_usage_up) ||					// current arc does not have enough capacity for upstream
					(a_down->bandwidth_cap <= current.bandwidth_usage_down))				// return arc does not have enough capacity for downstream
					continue;
				
				// if reached, arc qualifies -> spawn new incomplete path by adding arc
				returnPath newPath = current;		// copy of original (need to keep original for any additional arcs)
				_expandPathWithArc(&newPath, a_up);	// expand new copy with arc
				incomplete.push_back(newPath);		// add new path to incomplete list
			}
		}
		cout << "\n  - # generated paths: " << complete.size();
		
		// convert complete paths list to vector for return
		vector<returnPath> result;
		result.reserve(complete.size());
		result.insert(result.end(), complete.begin(), complete.end());
		return result;
	}

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

	bool _checkPathOverlap(returnPath * a, returnPath * b) {

		// loop through all arc used by path a and b, check if any arc is the same
		// customer -> provider
		for (list<arc*>::const_iterator i = b->arcs_up.begin(), end = b->arcs_up.end(); i != end; ++i) {
			for (list<arc*>::const_iterator j = a->arcs_up.begin(), end = a->arcs_up.end(); j != end; ++j) {
				if(*i == *j) {
					return true;
				}
			}
		}
		// provider -> customer
		for (list<arc*>::const_iterator i = b->arcs_down.begin(), end = b->arcs_down.end(); i != end; ++i) {
			for (list<arc*>::const_iterator j = a->arcs_down.begin(), end = a->arcs_down.end(); j != end; ++j) {
				if(*i == *j) {
					return true;
				}
			}
		}

		return false;
	}

	void generatePaths(dataContent * data, pathgenConfig config) {

		// create arcs from node pointers for each node
		vector<vector<arc*>> nodeArcs(data->network.n_nodes);
		for (unsigned int i = 0; i < data->network.arcs.size(); i++) {
			arc * a = &data->network.arcs[i];
			nodeArcs.at(a->startNode).push_back(a);
		}

		// loop through customers
		int serviceNumber = 0; // tracker for service number in total
		for (unsigned int c = 0; c < data->customers.size(); ++c)
		{
			// - loop through customer's services
			customer * cu = &data->customers[c];
			cout << "\n\nCustomer #" << c+1;
			for (unsigned int s = 0; s < cu->services.size(); ++s) 
			{
				service * se = &cu->services[s];
				// -- for each service's placement		
				for (unsigned int p = 0; p < se->possible_placements.size(); ++p)
				{
					placement * pl = &se->possible_placements[p];
					cout << "\n- Service #" << serviceNumber+1 << ", Provider #" << pl->globalProviderIndex+1;
					int providerNode = data->network.n_nodes - data->n_providers + pl->globalProviderIndex;
					
					// CUSTOMER -> PLACEMENT
					// generate paths
					pl->paths = _generateReturnPathsForNodePair(c, // customer index == customer node index
						providerNode, se->latency_req, 
						se->bandwidth_req_up, se->bandwidth_req_down,
						&nodeArcs, pl->price, config
					);

					// Register paths at their used arcs
					for (unsigned int ipath = 0; ipath < pl->paths.size(); ++ipath) {
						// all arcs used on the way up
						for (list<arc*>::const_iterator j = pl->paths[ipath].arcs_up.begin(), end = pl->paths[ipath].arcs_up.end(); j != end; ++j) {
							(*j)->up_paths.push_back(&pl->paths[ipath]);
						}
						// all arcs used on the way down
						for (list<arc*>::const_iterator j = pl->paths[ipath].arcs_down.begin(), end = pl->paths[ipath].arcs_down.end(); j != end; ++j) {
							(*j)->down_paths.push_back(&pl->paths[ipath]);
						}
					}
				}
				++serviceNumber;
			}
		}
		return;
	}

	void addPathComboAvailabilities(dataContent * data) {
		cout << "\n - calculating combo availability within each placement";
		
		for (unsigned int c = 0; c < data->customers.size(); ++c)
		{
			// - loop through customer's services
			customer * cu = &data->customers[c];
			for (unsigned int s = 0; s < cu->services.size(); ++s) 
			{
				service * se = &cu->services[s];
				// -- for each service's placement		
				for (unsigned int p = 0; p < se->possible_placements.size(); ++p) {
					placement * pl = &se->possible_placements[p];
					// calculate combo availability [ P(A)*P(B|A) ]
					for (unsigned int apath = 0; apath < pl->paths.size(); ++apath) {
						for (unsigned int bpath = 0; bpath < pl->paths.size(); ++bpath) {
							data->pathCombos.push_back(_pathComboForPaths(&pl->paths[apath], &pl->paths[bpath]));
						}
					}
				}
			}
		}
		cout << "\n - done (number of combos: " << data->pathCombos.size() << ")";
	}

	void addPathOverlaps(dataContent * data) {
		cout << "\n\ncalculating path overlaps..";
		int path1Num = 0;
		
		for (unsigned int c1 = 0; c1 < data->customers.size(); ++c1) {
			for (unsigned int s1 = 0; s1 < data->customers[c1].services.size(); ++s1) {	
				for (unsigned int p1 = 0; p1 < data->customers[c1].services[s1].possible_placements.size(); ++p1) {
					for (unsigned int ipath1 = 0; ipath1 < data->customers[c1].services[s1].possible_placements[p1].paths.size(); ++ipath1) {
						returnPath * path1 = &data->customers[c1].services[s1].possible_placements[p1].paths[ipath1];
						for (unsigned int c2 = 0; c2 < data->customers.size(); ++c2) {
							for (unsigned int s2 = 0; s2 < data->customers[c2].services.size(); ++s2) {	
								for (unsigned int p2 = 0; p2 < data->customers[c2].services[s2].possible_placements.size(); ++p2) {
									for (unsigned int ipath2 = 0; ipath2 < data->customers[c2].services[s2].possible_placements[p2].paths.size(); ++ipath2) {
										returnPath * path2 = &data->customers[c2].services[s2].possible_placements[p2].paths[ipath2];
										if(_checkPathOverlap(path1, path2)) {
											pathOverlap o;
											o.a = path1;
											o.b = path2;
											data->pathOverlaps.push_back(o);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		cout << "\n- # overlapping path pairs (not distinct): " << data->pathOverlaps.size();
	}

	void addFeasibleMappings(dataContent * data) {
		int mappingNumber = 0;
		int pathNumber = 0;
		// loop through customers
		for (unsigned int c = 0; c < data->customers.size(); ++c)
		{
			// - loop through customer's services
			customer * cu = &data->customers[c];
			for (unsigned int s = 0; s < cu->services.size(); ++s) 
			{
				service * se = &cu->services[s];
				// -- for each service's placement		
				for (unsigned int p = 0; p < se->possible_placements.size(); ++p)
				{
					placement * pl = &se->possible_placements[p];
					// --- for each path at current placement
					for (unsigned int a = 0; a < pl->paths.size(); ++a) {
						returnPath * apath = &pl->paths[a];
						++pathNumber;
						// check if feasible mapping alone
						if(apath->exp_availability >= se->availability_req) {
							// path offers sufficient availability alone -> dont add backup path
							mapping m;
							m.primary = apath;
							m.backup = NULL;
							se->possible_mappings.push_back(m);
							++mappingNumber;
						}
						// OR try combining with other path to placement as backup
						else {
							// path does not offer sufficient availability -> look for possible backup paths
							for (unsigned int b = 0; b < pl->paths.size(); ++b) {
								returnPath * bpath = &pl->paths[b];
								// calculate combo availability [ P(A)*P(B|A) ]
								pathCombo combo = _pathComboForPaths(apath, bpath);
								if(apath->exp_availability + bpath->exp_availability - combo.exp_b_given_a >= se->availability_req) {
									// combination of a as primary and b as backup is feasible -> add routing
									mapping m;
									m.primary = apath;
									m.backup = bpath;
									se->possible_mappings.push_back(m);
									++mappingNumber;
								}
							}
						}
					}
				}
				// register routings for service at used arcs
				for (unsigned int i = 0; i < se->possible_mappings.size(); ++i) {
					mapping * m = &se->possible_mappings[i];
					m->primary->primary_mappings.push_back(m);
					if(m->backup != NULL) {
						m->backup->backup_mappings.push_back(m);
					}
				}
				cout << "\n - # total availability feasible routings (primary[+backup]): " << se->possible_mappings.size();
			}
		}

		return;
	}
}