/*
 * ColumnGeneration.hpp
 *
 *  Created on: May 18, 2014
 *      Author: sindremo
 */

#ifndef COLUMNGENERATION_HPP_
#define COLUMNGENERATION_HPP_

#include "entities.h"
#include "CloudBrokerModel.hpp"

namespace cloudbrokeroptimisation {

	class AbstractColumnGenerator {
	protected:
		CloudBrokerModel * model;
		entities::dataContent * data;
	public:
		AbstractColumnGenerator(
				CloudBrokerModel * model, 		// the model to generating columns for
				entities::dataContent * data	// problem data of model
		);
		virtual bool GenerateColumnsForService(
				entities::customer * owner,		// customer owning service
				entities::service * service,	// the service generating columns for
				dual_vals * duals				// dual variables of model
		) = 0;
		virtual ~AbstractColumnGenerator() {}
	};

	class BruteForceColumnGenerator : public AbstractColumnGenerator {
	private:
		double _evalMapping(entities::mapping *m, entities::service *s, dual_vals *duals);
	public:
		BruteForceColumnGenerator(
				CloudBrokerModel * model, 		// the model to generating columns for
				entities::dataContent * data	// problem data of model
		);
		bool GenerateColumnsForService(
				entities::customer * owner,		// customer owning service
				entities::service * service,	// the service generating columns for
				dual_vals * duals				// dual variables of model
		);
	};

	class HeuristicAColumnGenerator : public AbstractColumnGenerator {
	public:
		HeuristicAColumnGenerator(
				CloudBrokerModel * model, 		// the model to generating columns for
				entities::dataContent * data	// problem data of model
		);
		bool GenerateColumnsForService(
				entities::customer * owner,		// customer owning service
				entities::service * service,	// the service generating columns for
				dual_vals * duals				// dual variables of model
		);
	};

	class HeuristicBColumnGenerator : public AbstractColumnGenerator {
	public:
		HeuristicBColumnGenerator(
				CloudBrokerModel * model, 		// the model to generating columns for
				entities::dataContent * data	// problem data of model
		);
		bool GenerateColumnsForService(
				entities::customer * owner,		// customer owning service
				entities::service * service,	// the service generating columns for
				dual_vals * duals				// dual variables of model
		);
	};
}

#endif /* COLUMNGENERATION_HPP_ */
