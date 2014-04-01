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

	list<returnPath> _generateReturnPathsForNodePair(int start_node, int end_node, double max_latency, double bandwidth_up,
		double bandwidth_down, vector<vector<arc*> > * node_arcs, double start_cost, int max_paths_per_placement)
	{
		// initial incomplete path to spawn all other paths from
		returnPath initial;
		initial.startNode = start_node;
		initial.endNode = start_node;
		initial.latency = 0;
		initial.exp_availability = 1;
		initial.bandwidth_usage_up = bandwidth_up;
		initial.bandwidth_usage_down = bandwidth_down;
		initial.cost = start_cost; // add the cost associated with the end node placement
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
				// check if max number of paths generated (if set > 0) -> end path generation
				if (max_paths_per_placement > 0 && (int)complete.size() >= max_paths_per_placement) break;
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
		
		// return all completed paths (paths reaching end node before latency limit or path restriction limit)
		return complete;
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

	void generatePaths(dataContent * data, int max_paths_per_placement) {

		// create arcs from node pointers for each node
		vector<vector<arc*> > nodeArcs(data->n_nodes);
		for (int i = 0; i < data->n_arcs; i++) {
			arc * a = &data->arcs[i];
			nodeArcs.at(a->startNode).push_back(a);
		}

		// loop through customers
		int serviceNumber = 0; // tracker for service number in total
		for (unsigned int cc = 0; cc < data->customers.size(); ++cc)
		{
			// - loop through customer's services
			customer * c = &data->customers[cc];
			cout << "\nCustomer #" << c->globalCustomerIndex+1 << " (" << c->services.size() <<" services)\n";
			for (unsigned int ss = 0; ss < c->services.size(); ++ss)
			{
				service * s = c->services[ss];
				cout << "\n- Service #" << s->globalServiceIndex+1 << " (" << s->possible_placements.size() <<" placements)\n";
				// -- for each service's placement		
				for (unsigned int pp = 0; pp < s->possible_placements.size(); ++pp)
				{
					placement * p = &s->possible_placements[pp];
					cout << " - Provider #" << p->globalProviderIndex+1 << "\n";
					int providerNode = data->n_nodes - data->n_providers + p->globalProviderIndex;
					
					// CUSTOMER -> PLACEMENT
					// - generate list of paths
					list<returnPath> paths = _generateReturnPathsForNodePair(cc, // customer index == customer node index
							providerNode, s->latency_req,
							s->bandwidth_req_up, s->bandwidth_req_down,
							&nodeArcs, p->price, max_paths_per_placement
						);
					// - add pointer to paths for this placement
					for (list<returnPath>::iterator k_itr = paths.begin(), k_end = paths.end(); k_itr != k_end; ++k_itr) {
						returnPath *k = &(*k_itr);
						p->paths.push_back(k);
					}
					// - add paths to global list of paths
					data->paths.splice(data->paths.end(), paths);
					cout << "  - # of generated paths: " << p->paths.size() << "\n";
				}
				++serviceNumber;
			}
		}
		// Register paths at their used arcs (and assign global path numbers)
		int pathNumber = 0;
		for (list<returnPath>::iterator k_itr = data->paths.begin(), k_end = data->paths.end(); k_itr != k_end; ++k_itr) {
			returnPath *k = &(*k_itr);
			k->globalPathIndex = pathNumber;
			// all arcs used on the way up
			for (list<arc*>::const_iterator j = k->arcs_up.begin(), end = k->arcs_up.end(); j != end; ++j) {
				(*j)->up_paths.push_back(k);
			}
			// all arcs used on the way down
			for (list<arc*>::const_iterator j = k->arcs_down.begin(), end = k->arcs_down.end(); j != end; ++j) {
				(*j)->down_paths.push_back(k);
			}
			++pathNumber;
		}
		data->n_paths = pathNumber;
		return;
	}

	void addPathComboAvailabilities(dataContent * data) {
		cout << " - calculating combo availability within each placement\n";
		
		// for every service placement
		for (unsigned int s = 0; s < data->services.size(); ++s)
		{
			service * se = &data->services[s];
			for (unsigned int p = 0; p < se->possible_placements.size(); ++p) {
				placement * pl = &se->possible_placements[p];

				// for every combination of paths for placement
				for (unsigned int apath = 0; apath < pl->paths.size(); ++apath) {
					for (unsigned int bpath = 0; bpath < pl->paths.size(); ++bpath) {
						// calculate combo availability [ P(A)*P(B|A) ]
						data->pathCombos.push_back(_pathComboForPaths(pl->paths[apath], pl->paths[bpath]));
					}
				}
			}
		}
		cout << " - done (number of combos: " << data->pathCombos.size() << ")\n";
	}

	void addFeasibleMappings(dataContent * data) {
		int mappingCount = 0;

		cout << "\nAdding feasible mappings..\n";

		for (unsigned int s = 0; s < data->services.size(); ++s)
		{
			service * se = &data->services[s];
			se->mappings.clear();
			// -- for each service's placement
			for (unsigned int p = 0; p < se->possible_placements.size(); ++p)
			{
				placement * pl = &se->possible_placements[p];
				// --- for each path at current placement
				for (unsigned int a = 0; a < pl->paths.size(); ++a) {
					returnPath * apath = pl->paths[a];
					// check if feasible mapping alone
					if(apath->exp_availability >= se->availability_req) {
						// path offers sufficient availability alone -> dont add backup path
						mapping m;
						m.globalMappingIndex = mappingCount;
						m.primary = apath;
						m.backup = NULL;
						data->mappings.push_back(m);
						se->mappings.push_back(&data->mappings.back());
						++mappingCount;

					}
					// OR try combining with other path to placement as backup
					else {
						// path does not offer sufficient availability -> look for possible backup paths
						for (unsigned int b = 0; b < pl->paths.size(); ++b) {
							returnPath * bpath = pl->paths[b];
							// calculate combo availability [ P(A)*P(B|A) ]
							pathCombo combo = _pathComboForPaths(apath, bpath);
							if(apath->exp_availability + bpath->exp_availability - combo.exp_b_given_a >= se->availability_req) {
								// combination of a as primary and b as backup is feasible -> add routing
								mapping m;
								m.globalMappingIndex = mappingCount;
								m.primary = apath;
								m.backup = bpath;
								data->mappings.push_back(m);
								se->mappings.push_back(&data->mappings.back());
								++mappingCount;
							}
						}
					}
				}

				// register mappings for service at used paths
				for (list<mapping*>::iterator m_itr = se->mappings.begin(), m_end = se->mappings.end(); m_itr != m_end; ++m_itr) {
					mapping *m = *m_itr;
					m->primary->primary_mappings.push_back(m);
					if(m->backup != NULL) {
						m->backup->backup_mappings.push_back(m);
					}
				}
				cout << " - service" << se->globalServiceIndex+1 << " at provider " << pl->globalProviderIndex+1 << "\n";
				cout << "  -# total availability feasible routings (primary[+backup]): " << se->mappings.size() << "\n";
			}
		}

		data->n_mappings = mappingCount;

		return;
	}
}
