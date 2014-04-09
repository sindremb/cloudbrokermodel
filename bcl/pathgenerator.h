#ifndef PATHGEN_H
#define PATHGEN_H

#include "entities.h"

namespace pathgen {
	
	void generatePaths(entities::dataContent *, int max_paths_per_placement);
	void addPathComboAvailabilities(entities::dataContent * data);
	void addFeasibleMappings(entities::dataContent *);
}

#endif
