#ifndef ENTITIES_H
#define ENTITIES_H

#include <vector>
#include <list>

namespace entities {

	// typedefs to allow bidirectional pointers
	typedef struct arc ARC;
	typedef struct returnPath RETURN_PATH;
	typedef struct mapping MAPPING;

	struct pathgenConfig {
		bool calcComboAvailabilities;
		int maxPathsPerPlacement;
	};
	
	struct arc {
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

		// Up paths using this arc
		std::list<RETURN_PATH*> up_paths;
		// Down paths using this arc
		std::list<RETURN_PATH*> down_paths;
	};

	struct returnPath {
		// path requirements
		double bandwidth_usage_up;
		double bandwidth_usage_down;
		double latency;

		// path start and end point
		int startNode;
		int endNode;

		// path cost
		double cost;

		// accumulative expected path availability
		double exp_availability;

		// this path's assigned number
		int pathNumber;

		// list of visited state for nodes for this path
		std::vector<bool> visitedNodes;

		// arcs used for up-path
		std::list<ARC*> arcs_up;
		// arcs used for paths return
		std::list<ARC*> arcs_down;

		// mappings using this path as primary
		std::list<MAPPING*> primary_mappings;
		// mappings using this path as backup
		std::list<MAPPING*> backup_mappings;
	};

	struct pathCombo {
		// the paths in the combination
		returnPath * a;
		returnPath * b;

		// P(A)P(B|A) for this path combo
		double exp_b_given_a;
	};

	struct pathOverlap {
		// the two paths overlapping
		returnPath * a;
		returnPath * b;
	};

	struct mapping {
		// assigned mapping number
		int globalMappingNumber;

		// mappings primary path
		returnPath * primary;
		// mappings backup path (if given)
		returnPath * backup;
	};

	struct networkStruct {
		// number of nodes in network
		int n_nodes;

		// indicates if arcs are equal in both direction (thus only defined for one direction)
		bool symmetric;

		// list of arcs in network
		std::vector<arc> arcs;
	};

	struct placement {
		// price of placement
		double price;

		// global provider index for this placement
		int globalProviderIndex;

		// possible paths for this placement
		std::vector<RETURN_PATH> paths;
	};

	struct service {
		// service requirements
		double bandwidth_req_up;
		double bandwidth_req_down;
		double latency_req;
		double availability_req;

		// possible service placements
		std::vector<placement> possible_placements;

		// possible service mappings
		std::vector<mapping> possible_mappings;
	};

	struct customer {
		// revenue from serving customer
		double revenue;

		// services requiered by customer
		std::vector<service> services;
	};

	struct dataContent {
		// number of customers / providers / services in problem
		int n_customers;
		int n_providers;
		int n_services;
		int n_mappings;
		int n_paths;
		
		// list of customers in network
		std::vector<customer> customers;

		// network in problem instance
		networkStruct network;

		// list of precalculated path availability combinations ( P(A)P(B|A) )
		std::list<pathCombo> pathCombos;
		// list of precalculated path overlaps
		std::list<pathOverlap> pathOverlaps;

	};

	void loadFromJSONFile(const char *, dataContent *);

	void toMoselDataFile(const char *, dataContent *);

}

#endif
