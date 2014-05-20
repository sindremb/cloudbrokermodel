#ifndef ENTITIES_H
#define ENTITIES_H

#include <vector>
#include <list>

namespace entities {

	// typedefs to allow bidirectional pointers
	typedef struct arc ARC;
	typedef struct returnPath RETURN_PATH;
	typedef struct mapping MAPPING;
	
	struct arc {
		// this arcs assigned index
		int globalArcIndex;

		// arc start and end point
		int startNode;
		int endNode;

		// arc characteristics
		double latency;
		double bandwidth_cap;
		double bandwidth_price;
		double exp_availability;

		// arc in opposite direction to this arc
		arc * return_arc;

		// list of paths using this arc  (pointers)
		// - start -> end
		std::list<RETURN_PATH*> up_paths;
		// - end -> start
		std::list<RETURN_PATH*> down_paths;
	};

	struct returnPath {
		// path characteristics
		double bandwidth_usage_up;
		double bandwidth_usage_down;
		double latency;

		// path start and end point
		int startNode;
		int endNode;

		// path cost (network usage and path end point cost)
		double cost;

		// accumulative expected path availability
		double exp_availability;

		// this path's assigned index
		int globalPathIndex;

		// list of visited state for nodes for this path
		std::vector<bool> visitedNodes;

		// lists of arcs used for this path (pointers)
		// - start -> end
		std::list<ARC*> arcs_up;
		// - end -> start
		std::list<ARC*> arcs_down;

		// lists of mappings using this path (pointers)
		// - as primary path
		std::list<MAPPING*> primary_mappings;
		// - as backup path
		std::list<MAPPING*> backup_mappings;
	};

	struct pathCombo {
		// the paths in the combination
		returnPath * a;
		returnPath * b;

		// P(A)P(B|A) for this path combo
		double prob_a_and_b;
	};

	struct mapping {
		// assigned mapping number
		int globalMappingIndex;

		// mappings primary path
		returnPath * primary;
		// mappings backup path (if defined)
		returnPath * backup;
	};

	struct placement {
		// price of placement
		double price;

		// global provider index for this placement
		int globalProviderIndex;

		// possible paths for this placement (pointers)
		std::vector<RETURN_PATH*> paths;
	};

	struct service {
		// service requirements
		double bandwidth_req_up;
		double bandwidth_req_down;
		double latency_req;
		double availability_req;

		int globalServiceIndex;

		// possible service placements
		std::vector<placement> possible_placements;

		// list of generated mappings for this service (pointers)
		std::list<mapping*> mappings;
	};

	struct customer {
		double revenue;					// revenue from serving customer
		int globalCustomerIndex;		// global customer index
		std::vector<service*> services; // services required by this customer (pointers)
	};

	struct dataContent {
		// problem dimensions
		// - fixed
		int n_customers;
		int n_providers;
		int n_services;
		int n_nodes;
		int n_arcs;
		// - dynamic
		int n_mappings;
		int n_paths;
		
		// given sets/lists in problem
		// - all customers
		std::vector<customer> customers;
		// - all services
		std::vector<service> services;
		// - all arcs
		std::vector<arc> arcs;

		// generated sets/lists in problem
		// - paths
		std::list<returnPath> paths;
		// - mappings
		std::list<mapping> mappings;
		// - precalculated path availability combinations ( P(A)P(B|A) )
		std::list<pathCombo> pathCombos;

	};

	/*
	 * Calculates the P(A)P(B|A) availability term only for paths *a and *b (ie. probability of a and b simultaneously)
	 */
	double prob_paths_a_and_b(returnPath * a, returnPath * b);

	/*
	 * Loads content of a JSON formatted file as dataContent object
	 */
	bool loadFromJSONFile(const char * filename, dataContent * content);

	/*
	 * Outputs data content object to mosel data compatible file
	 */
	void toMoselDataFile(const char * filename, dataContent * content);

}

#endif
