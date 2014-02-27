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

		int discardedPathCount = 0;
		while(incomplete.size() > 0) {
			// take first incomplete path from list
			returnPath current = incomplete.front();
			incomplete.pop_front();

			// current path has reached destination -> add to complete paths list, skip to next
			if(current.endNode == end_node) {
				complete.push_back(current);
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

				// non-feasibility check for expansion of arc
				if( current.visitedNodes.at(a_up->endNode) ||								// arc leads to already visited node
					(a_up->latency + a_down->latency > max_latency - current.latency) ||	// including path incurs to much latency
					(a_up->bandwidth_cap <= current.bandwidth_usage_up) ||					// current arc does not have enough capacity for upstream
					(a_down->bandwidth_cap <= current.bandwidth_usage_down))				// return arc does not have enough capacity for downstream
					continue;
				
				// if reached, arc qualifies -> spawn new incomplete path by adding arc
				returnPath newPath = current;		// copy of original (need to keep original for additional arcs)
				_expandPathWithArc(&newPath, a_up);	// expand new copy with arc
				incomplete.push_back(newPath);		// add new path to incomplete list
				path_expanded = true;
			}
			// if path could not be expanded -> update discarded paths counter
			if(!path_expanded) {
				discardedPathCount++;
			}
		}
		cout << "\n - # generated paths (discarded paths): " << complete.size() << " (" << discardedPathCount << ")";
		
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

	bool _checkPathOverlap(returnPath * a, returnPath * b) {

		// loop through all arc used by path a and b, check if any arc is the same
		for(unsigned int i = 0; i < b->arcs_up.size(); ++i) {
			for(unsigned int j = 0; j < a->arcs_up.size(); ++j) {
				if(b->arcs_up[i] == a->arcs_up[j]) {
					return true;
				}
			}
		}

		for(unsigned int i = 0; i < b->arcs_down.size(); ++i) {
			for(unsigned int j = 0; j < a->arcs_down.size(); ++j) {
				if(b->arcs_down[i] == a->arcs_down[j]) {
					return true;
				}
			}
		}

		return false;
	}

	void generatePaths(dataContent * data, bool calcOverlaps = false) {

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
					cout << "\n- Service #" << serviceNumber+1 << ", Provider #" << placement->provider_index+1;
					int providerNode = data->network.n_nodes - data->n_providers + placement->provider_index;
					
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
					cout << "\n - calculating combo availability (" << placement->paths.size() << "x" << placement->paths.size() << ")";
					// calculate combo availability [ P(A)*P(B|A) ]
					for (unsigned int apath = 0; apath < placement->paths.size(); ++apath) {
						for (unsigned int bpath = 0; bpath < placement->paths.size(); ++bpath) {
							data->pathCombos.push_back(_pathComboForPaths(&placement->paths[apath], &placement->paths[bpath]));
						}
					}
					cout << "\n - done";
				}
				++serviceNumber;
			}
		}
		if(calcOverlaps) {
			// find paths with overlap
			// NOTE: current implementation inefficient
			// loop through all paths
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
		} else {
			cout << "\n\n- skipping calculating overlaps..";
		}

		return;
	}

	
}