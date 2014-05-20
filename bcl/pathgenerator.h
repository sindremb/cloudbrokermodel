#ifndef PATHGEN_H
#define PATHGEN_H

#include "entities.h"

namespace pathgen {
	
	/*
	 * Generates possible paths for each service-placement pair in data content provided, up to
	 * maximum number of paths per service-placement pair as given
	 */
	void generatePaths(entities::dataContent *, int max_paths_per_placement);

	/*
	 * Adds precalculated P(A)P(B|A) values for all valid combinations of two paths for all services
	 */
	void addPathComboAvailabilities(entities::dataContent * data);

	/*
	 * Adds all availability feasible mappings for all services from already generated paths
	 *
	 * Note: Will not add mappings with a primary and backup for cases where the primary paths
	 * 		 alone is sufficient
	 */
	void addFeasibleMappings(entities::dataContent *);
}

#endif
