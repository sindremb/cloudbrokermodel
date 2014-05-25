/*
 * spprc.h
 *
 *  Created on: May 25, 2014
 *      Author: sindremo
 */

#ifndef SPPRC_H_
#define SPPRC_H_

#include "entities.h"

#include <list>

namespace cloudbrokeroptimisation {

	double spprc(
		int start_node, int end_node, int n_nodes, 				// start node, end node, total number of nodes
		std::vector<std::vector<entities::arc*> > *node_arcs,	// list of arcs for each node, filtered to have sufficient bandwidth, includes latency and availability of arc
		std::vector<double> *arc_costs,							// cost of using each arc, including dual costs
		std::vector<int> *arc_overlaps,							// for each arc, 1 if causing overlap, 0 otherwise
		double max_latency,										// max round trip latency, required
		int max_restricted_arcs,								// max overlap, optional (set to < 0 to ignore)
		double min_availability,								// min latency, optional (set to < 0.0 to ignore)
		std::list<entities::arc*>* used_arcs					// list of arcs for found path
	);
}

#endif /* SPPRC_H_ */
