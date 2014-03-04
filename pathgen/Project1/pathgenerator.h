#ifndef PATHGEN_H
#define PATHGEN_H

#include "entities.h"

namespace pathgen {
	void generatePaths(entities::dataContent *, entities::pathgenConfig);
	void generateRoutings(entities::dataContent *, entities::pathgenConfig);
}

#endif