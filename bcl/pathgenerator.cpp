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
		double bandwidth_down, vector<vector<arc*> > * node_arcs, double start_cost, pathgenConfig config)
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
				if (config.maxPathsPerPlacement > 0 && (int)complete.size() >= config.maxPathsPerPlacement) break;
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
		cout << "  - # generated paths: " << complete.size() << "\n";
		
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

	void generatePaths(dataContent * data, pathgenConfig config) {

		// create arcs from node pointers for each node
		vector<vector<arc*> > nodeArcs(data->network.n_nodes);
		for (unsigned int i = 0; i < data->network.arcs.size(); i++) {
			arc * a = &data->network.arcs[i];
			nodeArcs.at(a->startNode).push_back(a);
		}

		int pathNumber = 0;

		// loop through customers
		int serviceNumber = 0; // tracker for service number in total
		for (unsigned int c = 0; c < data->customers.size(); ++c)
		{
			// - loop through customer's services
			customer * cu = &data->customers[c];
			cout << "\nCustomer #" << c+1 << "\n";
			for (unsigned int s = 0; s < cu->services.size(); ++s) 
			{
				service * se = &cu->services[s];
				// -- for each service's placement		
				for (unsigned int p = 0; p < se->possible_placements.size(); ++p)
				{
					placement * pl = &se->possible_placements[p];
					cout << "- Service #" << serviceNumber+1 << ", Provider #" << pl->globalProviderIndex+1 << "\n";
					int providerNode = data->network.n_nodes - data->n_providers + pl->globalProviderIndex;
					
					// CUSTOMER -> PLACEMENT
					// generate paths
					pl->paths = _generateReturnPathsForNodePair(c, // customer index == customer node index
						providerNode, se->latency_req, 
						se->bandwidth_req_up, se->bandwidth_req_down,
						&nodeArcs, pl->price, config
					);

					// Register paths at their used arcs (and assign global path numbers)
					for (unsigned int ipath = 0; ipath < pl->paths.size(); ++ipath) {
						++pathNumber;
						returnPath *p = &pl->paths[ipath];
						p->pathNumber = pathNumber;
						// all arcs used on the way up
						for (list<arc*>::const_iterator j = p->arcs_up.begin(), end = p->arcs_up.end(); j != end; ++j) {
							(*j)->up_paths.push_back(p);
						}
						// all arcs used on the way down
						for (list<arc*>::const_iterator j = p->arcs_down.begin(), end = p->arcs_down.end(); j != end; ++j) {
							(*j)->down_paths.push_back(p);
						}
					}
				}
				++serviceNumber;
			}
		}
		return;
	}

	void addPathComboAvailabilities(dataContent * data) {
		cout << " - calculating combo availability within each placement\n";
		
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
		cout << " - done (number of combos: " << data->pathCombos.size() << ")\n";
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
				se->possible_mappings.clear();
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
							++mappingNumber;
							mapping m;
							m.globalMappingNumber = mappingNumber;
							m.primary = apath;
							m.backup = NULL;
							se->possible_mappings.push_back(m);

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
									++mappingNumber;
									mapping m;
									m.globalMappingNumber = mappingNumber;
									m.primary = apath;
									m.backup = bpath;
									se->possible_mappings.push_back(m);
								}
							}
						}
					}
				}
				// register mappings for service at used paths
				for (unsigned int i = 0; i < se->possible_mappings.size(); ++i) {
					mapping * m = &se->possible_mappings[i];
					m->primary->primary_mappings.push_back(m);
					if(m->backup != NULL) {
						m->backup->backup_mappings.push_back(m);
					}
				}
				cout << " - # total availability feasible routings (primary[+backup]): " << se->possible_mappings.size() << "\n";
			}
		}

		data->n_mappings = mappingNumber;

		return;
	}
}
