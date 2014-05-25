/*
 * spprc.cpp
 *
 *  Created on: May 25, 2014
 *      Author: sindremo
 */

#include "spprc.h"
#include "entities.h"

#include <list>

#ifndef EPS
#define EPS 1e-6
#endif

#ifndef Inf
#define Inf 1e10
#endif

#ifndef NULL
#define NULL 0
#endif

using namespace entities;
using namespace std;

namespace cloudbrokeroptimisation {

	/********** Label struct ************
	 * Data structure for 'labels' used in the labeling algorithm for solving the
	 * SPPRC in heuristic A for column generation
	 */
	struct label {
		double cost;
		double latency;
		double availability;
		int restricted_arcs_count;
		int end_node;
		arc *last_arc;
		label *parent;
		bool isdominated;
	};

	/* spprc: solves an SPPRC problem using a labeling algorithm
	 * assumptions: the expected availability is the probability of an arc and its return arc to go down, thus
	 * the availability of the path returned is only the product sum of its arc in the up-link (down-link using the
	 * equivalent link in the opposite direction)
	 */
	double spprc(
			int start_node, int end_node, int n_nodes,
			vector<vector<arc*> > *node_arcs,
			vector<double> *arc_costs,
			vector<int> *arc_restrictions,
			double max_latency,
			int max_restricted_arcs,
			double min_availability,
			list<arc*>* used_arcs) {

		/******** SETUP ***********/

		list<label> all_labels;
		list<label*> u;
		list<label*> p;

		label initiallabel;
		initiallabel.cost = 0.0;
		initiallabel.latency = 0.0;
		initiallabel.availability = 1.0;
		initiallabel.restricted_arcs_count = 0;
		initiallabel.end_node = start_node;
		initiallabel.last_arc = NULL;
		initiallabel.parent = NULL;
		initiallabel.isdominated = false;

		all_labels.push_back(initiallabel);
		u.push_back(&all_labels.back());

		/************ SPPRC labeling algorithm *************/

		while(u.size() > 0) {
			// PATH EXPANSION
			// - extract label q from U
			label *q = u.front();
			u.pop_front();

			bool found_new = false;

			// need only try expanding if not already dominated or not at end node
			if(!q->isdominated && q->end_node != end_node) {
				// - arcs from this label's (q) end node
				vector<arc*> *arcs = &node_arcs->at(q->end_node);
				// - try expand for each arc from end node
				for(unsigned int aa = 0; aa < arcs->size(); ++aa) {
					arc *a = arcs->at(aa);
					if(q->latency + a->latency + a->return_arc->latency <= max_latency &&
						q->restricted_arcs_count + arc_restrictions->at(a->globalArcIndex) <= max_restricted_arcs &&
						q->availability * a->exp_availability >= min_availability) {
						// resource check ok -> spawn new label
						label child;
						child.parent = q;
						child.last_arc = a;
						child.end_node = a->endNode;
						child.cost = q->cost + arc_costs->at(a->globalArcIndex);
						child.latency = q->latency + a->latency + a->return_arc->latency;
						child.availability = q->availability * a->exp_availability;
						child.restricted_arcs_count = q->restricted_arcs_count + arc_restrictions->at(a->globalArcIndex);
						child.isdominated = false;

						all_labels.push_back(child);
						u.push_back(&all_labels.back());

						found_new = true;
					}
				}
			}
			// - add q to P
			p.push_back(q);

			// need only perform dominations of any new labels were found
			if(found_new) {
				// DOMINATION

				// for every combination of labels (a, b)
				// - every label a not having been dominated
				for(list<label>::iterator l_itr = all_labels.begin(), l_end = all_labels.end(); l_itr != l_end; ++l_itr) {
					label* a = &(*l_itr);
					if(!a->isdominated) {

						// - every label b, succeeding a, that has not been dominated
						list<label>::iterator l_itr2 = l_itr;
						++l_itr2;
						for(list<label>::iterator l_end2 = all_labels.end(); l_itr2 != l_end2; ++l_itr2) {
							label* b = &(*l_itr2);

							// check that labels are not the same, but end at same node
							if(!b->isdominated && a->end_node == b->end_node) {
								// try dominate
								if(b->cost >= a->cost && b->latency >= a->latency && b->availability <= a->availability && b->restricted_arcs_count >= a->restricted_arcs_count) {
									// b is equally bad or worse than a -> dominate b
									b->isdominated = true;
								}
								else if(b->cost <= a->cost && b->latency <= a->latency && b->availability >= a->availability && b->restricted_arcs_count <= a->restricted_arcs_count) {
									// a is worse than b (not equally bad due to above if) -> dominate a
									a->isdominated = true;
								}
								// ELSE: a and b can not dominate / be dominated by each other..
							}

						}
					}
				}
			}

		}

		/********** EXTRACT SOLUTION **********/

		label *bestlabel = NULL;
		double bestcost = Inf;

		for(list<label*>::iterator l_itr = p.begin(), l_end = p.end(); l_itr != l_end; ++l_itr) {
			label* l = *l_itr;
			if(l->end_node == end_node && l->cost <= bestcost) {
				bestlabel = l;
				bestcost = l->cost;
			}
		}

		if(bestlabel != NULL) {

			double costcheck = 0.0;
			label *l = bestlabel;
			while(l->last_arc != NULL) {
				arc *a = l->last_arc;
				used_arcs->push_front(a);
				costcheck += arc_costs->at(a->globalArcIndex);
				l = l->parent;
			}

			return bestlabel->cost;
		}

		// return a very high cost if no path to end node was found
		return Inf;
	}

}


