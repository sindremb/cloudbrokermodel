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

	vector<returnPath> _generateReturnPathsForNodePair(int start_node, int end_node, int max_latency, int bandwidth_up, int bandwidth_down, vector<vector<arc*>> * node_arcs, int start_cost = 0)
	{
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

		vector<returnPath> incomplete;
		vector<returnPath> complete;

		incomplete.push_back(initial);

		int i = 0;
		while(incomplete.size() > i) {
			// current: copy of first path in list
			returnPath current = incomplete[i];

			// current path has reached destination
			if(current.endNode == end_node) {
				complete.push_back(current);
				++i;
				continue;
			}

			// arcs starting from last node of current path
			vector<arc*> * arcs = &node_arcs->at(current.endNode);

			// for each arc, expand current path or spawn new paths (if more than one possibility for expansion)
			bool path_expanded = false;
			for(unsigned int j = 0; j < arcs->size(); ++j) {
				// shorthands for current arc and its return arc
				arc * a_up = arcs->at(j);
				arc * a_down = a_up->return_arc;

				// expansion non-feasibility check for arc
				if( current.visitedNodes.at(a_up->endNode) ||								// arc leads to already visited node
					(a_up->latency + a_down->latency > max_latency - current.latency) ||	// including path incurs to much latency
					(a_up->bandwidth_cap <= current.bandwidth_usage_up) ||					// current arc does not have enough capacity for upstream
					(a_down->bandwidth_cap <= current.bandwidth_usage_down))				// return arc does not have enough capacity for downstream
					continue;
				
				// if current already expanded once -> spawn new incomplete path from current copy
				if(path_expanded) {
					returnPath newPath = current;	// copy of original
					_expandPathWithArc(&newPath, a_up);	// expand new copy with arc
					incomplete.push_back(newPath);	// add new path to incomplete list
				}
				// expand current in place
				else {
					_expandPathWithArc(&incomplete[i], a_up);
					path_expanded = true;
				}
			}
			// if current path could not be extended -> skip to next path
			if(!path_expanded) {
				++i;
			}
		}
		cout << "\n - # generated (incomplete paths): " << complete.size() << " (" << incomplete.size() - complete.size() << ")";
		return complete;
	}

	pathCombo _pathComboForPaths(returnPath * a, returnPath * b) {
		pathCombo combo;
		combo.a = a;
		combo.b = b;
		combo.exp_b_given_a = a->exp_availability;

		// calculate prop b up given a up
		vector<arc*> unique;

		for(unsigned int i = 0; i < b->arcs_up.size(); ++i) {
			arc * anarc = b->arcs_up[i];
			bool found = false;
			for(unsigned int j = 0; j < a->arcs_up.size(); ++j) {
				if(anarc == a->arcs_up[j]) {
					found = true;
					break;
				}
			}
			if(!found) {
				unique.push_back(anarc);
			}
		}

		for(unsigned int i = 0; i < unique.size(); ++i) {
			combo.exp_b_given_a *= unique[i]->exp_availability;
		}

		return combo;
	}

	void generatePaths(dataContent * data) {

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
					placement * placement = &se->possible_placements[p];
					cout << "\n- Service #" << serviceNumber << ", Provider #" << placement->provider_number;
					int providerNode = data->network.n_nodes - data->n_providers + placement->provider_number;
					
					// CUSTOMER -> PLACEMENT
					// generate paths
					placement->paths = _generateReturnPathsForNodePair(c, // customer index == customer node index
						providerNode, se->latency_req, 
						se->bandwidth_req_up, se->bandwidth_req_down,
						&nodeArcs, placement->price
					);

					// Register paths at their used arcs
					for (unsigned int ipath = 0; ipath < placement->paths.size(); ++ipath) {
						// all arcs used on the way up
						for (unsigned int iarc = 0; iarc < placement->paths[ipath].arcs_up.size(); ++iarc) {
							placement->paths[ipath].arcs_up[iarc]->up_paths.push_back(&placement->paths[ipath]);
						}
						// all arcs used on the way down
						for (unsigned int iarc = 0; iarc < placement->paths[ipath].arcs_down.size(); ++iarc) {
							placement->paths[ipath].arcs_down[iarc]->down_paths.push_back(&placement->paths[ipath]);
						}
					}

					// calculate combo availability [ P(A)*P(B|A) ]
					for (unsigned int apath = 0; apath < placement->paths.size(); ++apath) {
						for (unsigned int bpath = 0; bpath < placement->paths.size(); ++bpath) {
							data->pathCombos.push_back(_pathComboForPaths(&placement->paths[apath], &placement->paths[bpath]));
						}
					}
				}
				++serviceNumber;
			}
		}
		// find paths with overlap

		return;
	}

	
}