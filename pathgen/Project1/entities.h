#ifndef ENTITIES_H
#define ENTITIES_H

#include <vector>

namespace entities {

	struct arc;
	struct path;

	typedef struct arc ARC;
	typedef struct returnPath RETURN_PATH;
	
	struct arc {
		int startNode;
		int endNode;

		int latency;
		int bandwidth_cap;
		int bandwidth_price;
		double exp_availability;

		arc * return_arc;

		std::vector<RETURN_PATH*> up_paths;
		std::vector<RETURN_PATH*> down_paths;
	};

	struct returnPath {
		int bandwidth_usage_up;
		int bandwidth_usage_down;
		int latency;

		int startNode;
		int endNode;

		int cost;

		double exp_availability;

		int pathNumber;

		std::vector<bool> visitedNodes;

		std::vector<ARC*> arcs_up;
		std::vector<ARC*> arcs_down;
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

	struct network {
		int n_nodes;
		bool symmetric;

		std::vector<arc> arcs;
	};

	struct placement {
		int price;
		int provider_index;
		std::vector<RETURN_PATH> paths;
	};

	struct service {
		int bandwidth_req_up;
		int bandwidth_req_down;
		int latency_req;
		double availability_req;
		std::vector<placement> possible_placements;
	};

	struct customer {
		int revenue;

		std::vector<service> services;
	};

	struct dataContent {
		int n_customers;
		int n_providers;
		int n_services;

		network network;

		std::vector<pathCombo> pathCombos;
		std::vector<pathOverlap> pathOverlaps;

		std::vector<customer> customers;
	};

	void loadFromJSONFile(const char *, dataContent *);

	void toMoselDataFile(const char *, dataContent *);
}

#endif