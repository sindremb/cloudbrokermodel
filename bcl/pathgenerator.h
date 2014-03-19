#ifndef PATHGEN_H
#define PATHGEN_H

#include "entities.h"

namespace pathgen {
	
	void generatePaths(entities::dataContent *, entities::pathgenConfig);
	void addPathComboAvailabilities(entities::dataContent * data);
	void addPathOverlaps(entities::dataContent * data);
	void addFeasibleMappings(entities::dataContent *);
}

#endif