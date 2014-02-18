#include <vector>
#include <list>
#include <iostream>

#include "entities.h"

namespace pathgen {
	
	using namespace entities;
	using namespace std;

	vector<path> _generatePathsForNodePair(int startNode, int endNode, int maxLatency, int bandwidth, vector<vector<arc*>> * nodeArcs, int start_cost = 0)
	{
		path initial;
		initial.startNode = startNode;
		initial.endNode = startNode;
		initial.latency = 0;
		initial.bandwidth_usage = bandwidth;
		initial.cost = start_cost;
		initial.visitedNodes.resize(nodeArcs->size(), false);
		initial.visitedNodes[startNode] = true;

		vector<path> incomplete;
		vector<path> complete;
		incomplete.push_back(initial);

		int i = 0;
		while(incomplete.size() > i) {
			path * cpath = &incomplete[i];

			// current path has reached destination
			if(cpath->endNode == endNode) {
				complete.push_back(*cpath);
				++i;
				continue;
			}

			// arc data from current node
			vector<arc*> * arcs = &nodeArcs->at(cpath->endNode);

			// for each arc, expand current path or spawn new paths (if more than one possibility for expansion)
			bool path_expanded = false;
			for(int j = 0; j < arcs->size(); ++j) {
				arc * a = arcs->at(j);
				// expansion feasibility check for arc
				if( cpath->visitedNodes.at(a->endNode) || 
					(a->latency > (maxLatency - cpath->latency)) ||
					(a->bandwidth_cap <= cpath->bandwidth_usage))
					continue;
				
				// if current already expanded -> spawn new incomplete path from original
				if(path_expanded) {
					path newPath = *cpath;
					newPath.endNode = a->endNode;
					newPath.cost += a->bandwidth_price * newPath.bandwidth_usage;
					newPath.arcs.push_back(a);
					newPath.visitedNodes[newPath.endNode] = true;
					incomplete.push_back(newPath);
					cpath = &incomplete[i]; // push_back creates new array -> new mem location
				}
				// expand current
				else {
					cpath->latency += a->latency;
					cpath->endNode = a->endNode;
					cpath->cost += a->bandwidth_price * cpath->bandwidth_usage;
					cpath->visitedNodes[cpath->endNode] = true;
					cpath->arcs.push_back(a);
					path_expanded = true;
				}
			}
			// if current path could not be extended -> skip to next path
			if(!path_expanded) {
				++i;
			}
		}
		cout << "\n - # generated paths: " << complete.size();
		cout << "\n - # skipped due to latency/loop req: " << incomplete.size() - complete.size();
		return complete;
	}

	void generatePaths(dataContent * data) {

		vector<vector<arc*>> nodeArcs(data->network.n_nodes);
		for (int i = 0; i < data->network.arcs.size(); i++) {
			arc * a = &data->network.arcs[i];
			nodeArcs.at(a->startNode).push_back(a);
		}

		// loop through customers
		int customerNode = 0;
		int serviceNumber = 0;
		for (int c = 0; c < data->customers.size(); ++c)
		{
			// - loop through customer's services
			customer * cu = &data->customers[c];
			cout << "\n\nCustomer #" << customerNode;
			for (int s = 0; s < cu->services.size(); ++s) 
			{
				service * se = &cu->services[s];
				// -- for each service's placement		
				for (int p = 0; p < se->possible_placements.size(); ++p)
				{
					placement * placement = &se->possible_placements[p];
					cout << "\n Service #" << serviceNumber << ", provider #" << placement->provider_number;
					int providerNode = data->network.n_nodes - data->n_providers + placement->provider_number;
					
					// --- generate all feasible paths
					
					// CUSTOMER -> PLACEMENT
					// generate paths
					cout << "\n -From node " << customerNode << " to " << providerNode;
					placement->paths_up = _generatePathsForNodePair(customerNode, providerNode,
						se->latency_req, se->bandwidth_req_up, &nodeArcs, placement->price);
					// find min latency for up-paths (for max latency down paths)
					int min_up_latency = se->latency_req;
					for (int ipath = 0; ipath < placement->paths_up.size(); ++ipath)
					{
						if(placement->paths_up[ipath].latency < min_up_latency) {
							min_up_latency = placement->paths_up[ipath].latency;
						}
					}

					// PLACMENT -> CUSTOMER
					// generate paths (with stricter latency req from above)
					cout << "\n -From node " << providerNode << " to " << customerNode;
					placement->paths_down = _generatePathsForNodePair(providerNode, customerNode, se->latency_req - min_up_latency,
						se->bandwidth_req_down, &nodeArcs);

					// PURGE: remove any infeasible up paths from new minimum down latency
					// - find min down latency
					int min_down_latency = se->latency_req - min_up_latency;
					for (int ipath = 0; ipath < placement->paths_down.size(); ++ipath)
					{
						if (placement->paths_down[ipath].latency < min_down_latency) {
							min_down_latency = placement->paths_down[ipath].latency;
						}
					}
					// - perform purge
					// TODO: implement infeasible up path purge

					// Register remaining paths at their arcs used
					for (int ipath = 0; ipath < placement->paths_up.size(); ++ipath) {
						for (int iarc = 0; iarc < placement->paths_up[ipath].arcs.size(); ++iarc) {
							placement->paths_up[ipath].arcs[iarc]->paths.push_back(&placement->paths_up[ipath]);
						}
					}
					for (int ipath = 0; ipath < placement->paths_down.size(); ++ipath) {
						for (int iarc = 0; iarc < placement->paths_down[ipath].arcs.size(); ++iarc) {
							placement->paths_down[ipath].arcs[iarc]->paths.push_back(&placement->paths_down[ipath]);
						}
					}
				}
				++serviceNumber;
			}
			++customerNode;
		}
		
		
		return;
	}

	
}