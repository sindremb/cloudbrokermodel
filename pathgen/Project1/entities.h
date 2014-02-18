#ifndef ENTITIES_H
#define ENTITIES_H

#include <vector>

namespace entities {

	struct arc;
	struct path;

	typedef struct arc ARC;
	typedef struct path PATH;
	
	struct arc {
		int startNode;
		int endNode;

		int latency;
		int bandwidth_cap;
		int bandwidth_price;
		double exp_availability;

		std::vector<PATH*> paths;
	};

	struct path {
		int bandwidth_usage;
		int latency;

		int startNode;
		int endNode;

		int cost;

		int pathNumber;

		std::vector<bool> visitedNodes;

		std::vector<ARC*> arcs;
	};

	struct network {
		int n_nodes;
		bool symmetric;

		std::vector<arc> arcs;
	};

	struct placement {
		int price;
		int provider_number;
		std::vector<path> paths_up;
		std::vector<path> paths_down;
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

		std::vector<customer> customers;
	};

	dataContent loadFromJSONFile(const char *);

	void toMoselDataFile(const char *, dataContent *);
}

#endif