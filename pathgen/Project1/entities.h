#ifndef ENTITIES_H
#define ENTITIES_H

#include <vector>
#include <list>

namespace entities {

	struct arc;
	struct path;

	typedef struct arc ARC;
	typedef struct returnPath RETURN_PATH;
	typedef struct routing ROUTING;

	struct pathgenConfig {
		bool calcOverlaps;
		int maxPathsPerPlacement;
	};
	
	struct arc {
		int startNode;
		int endNode;

		double latency;
		double bandwidth_cap;
		double bandwidth_price;
		double exp_availability;

		arc * return_arc;

		std::list<ROUTING*> up_routings_primary;
		std::list<ROUTING*> down_routings_primary;
		std::list<ROUTING*> up_routings_backup;
		std::list<ROUTING*> down_routings_backup;

		std::list<RETURN_PATH*> up_paths;
		std::list<RETURN_PATH*> down_paths;
	};

	struct returnPath {
		double bandwidth_usage_up;
		double bandwidth_usage_down;
		double latency;

		int startNode;
		int endNode;

		double cost;

		double exp_availability;

		int pathNumber;

		std::vector<bool> visitedNodes;

		std::list<ARC*> arcs_up;
		std::list<ARC*> arcs_down;
	};

	struct pathCombo {
		returnPath * a;
		returnPath * b;

		double exp_b_given_a;
	};

	struct pathOverlap {
		returnPath * a;
		returnPath * b;
	};

	struct routing {
		int routingNumber;
		returnPath * primary;
		returnPath * backup;
	};

	struct network {
		int n_nodes;
		bool symmetric;

		std::vector<arc> arcs;
	};

	struct placement {
		double price;
		int provider_index;
		std::vector<RETURN_PATH> paths;
	};

	struct service {
		double bandwidth_req_up;
		double bandwidth_req_down;
		double latency_req;
		double availability_req;
		std::vector<placement> possible_placements;

		std::vector<routing> possible_routings;
	};

	struct customer {
		double revenue;

		std::vector<service> services;
	};

	struct dataContent {
		int n_customers;
		int n_providers;
		int n_services;

		network network;

		std::list<pathCombo> pathCombos;
		std::list<pathOverlap> pathOverlaps;

		std::vector<customer> customers;
	};

	void loadFromJSONFile(const char *, dataContent *);

	void toMoselDataFile(const char *, dataContent *);

	void toMoselDataFileV2(const char *, dataContent *);
}

#endif