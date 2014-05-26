
#include "CloudBrokerOptimisation.hpp"
#include "ColumnGeneration.hpp"
#include "CloudBrokerModel.hpp"
#include "entities.h"
#include "simpletimer.hpp"

#include <list>
#include <iostream>
#include <algorithm>

using namespace entities;
using namespace ::dashoptimization;
using namespace std;

namespace cloudbrokeroptimisation {

	CloudBrokerOptimiser::CloudBrokerOptimiser(entities::dataContent * d, double beta, bool dedicated)
	: model()
	{
		this->data = d;
		this->model.BuildModel(d, beta, dedicated);
	}

	void CloudBrokerOptimiser::Solve(bool enforce_integer, int time_limit, const char *lp_alg)
	{
		this->model.RunModel(enforce_integer, time_limit = 0, lp_alg);
	}

	void CloudBrokerOptimiser::OutputResultsToStream(std::ostream& stream) {
		this->model.OutputResultsToStream(stream);
	}

	void CloudBrokerOptimiser::RunColumnGeneration(int cg_alg, int cg_maxiters, int cg_maxcount, const char *opt_alg)
	{
		AbstractColumnGenerator * cg;

		cout << "- Column Generation By: ";
		switch(cg_alg) {
			case CG_BRUTEFORCE:
				cout << "Brute Force\n";
				cg = new BruteForceColumnGenerator(&this->model, this->data);
				break;
			case CG_HEURISTIC_A:
				cout << "Heuristic A\n";
				cg = new HeuristicAColumnGenerator(&this->model, this->data);
				break;
			case CG_HEURISTIC_B:
				cout << "Heuristic B\n";
				cg = new HeuristicBColumnGenerator(&this->model, this->data);
				break;
			default:
				cerr << "undefined cg algorithm choice: " << cg_alg << "\n";
				return;
		}
		cout << "-- max iterations: " << cg_maxiters << "\n";
		cout << "-- max mappings: " << cg_maxcount << "\n";

		timer run_cg_total_start;
		double lp_time_total = 0.0;
		double cg_time_total = 0.0;
		double run_cg_total_time = 0.0;

		double lp_opt = 0.0;

		this->model.SetColumnGenerationConfiguration(true);

		int itercount = 0;
		while((cg_maxiters < 0 || itercount < cg_maxiters) && 
			  (cg_maxcount < 0 || data->n_mappings < cg_maxcount)) {

			bool foundColumn = false;

			// solve LP-relaxation
			timer lp_start;
			cout << "\nIteration " << itercount+1 << ":\n-- running lp-relaxation..\n";
			this->model.RunModel(false, 0, opt_alg);
			lp_time_total += lp_start.elapsed();

			lp_opt = this->model.GetObjectiveValue();
			this->model.SaveBasis();
			dual_vals duals = this->model.GetDualVals();
			
			cout << "-- Generating Columns..\n";
			timer cg_start;

			// generate columns for every (service, provider) pair
			for(int cc = 0; cc < this->data->n_customers; ++cc) {
				customer *c = &this->data->customers[cc];
				for(unsigned int ss = 0; ss < c->services.size(); ++ss) {
					service *s = c->services[ss];
					
					if(cg->GenerateColumnsForService(c, s, &duals)) {
						foundColumn = true;
					}
				}
			}

			cg_time_total += cg_start.elapsed();

			if(!foundColumn)  {
				cout << "\n==> No more column found, column generation stopped\n";
				break; /* no new column was found -> end column generation */
			}

			this->model.LoadSavedBasis();
			++itercount;
		}

		this->model.SetColumnGenerationConfiguration(false);

		run_cg_total_time = run_cg_total_start.elapsed();

		cout << "\n############## COLUMN GENERATION COMPLETED ####################\n";
		cout << "- Total time:          " << run_cg_total_time << "\n";
		cout << "- Total LP solve time: " << lp_time_total << "\n";
		cout << "- Total CG time:       " << cg_time_total << "\n";
		cout << "- Overhead time:       " << run_cg_total_time - cg_time_total - lp_time_total << "\n";
		cout << "- # iterations:        " << itercount << "\n";
		cout << "- # of mappings total: " << this->data->n_mappings << "\n";
		cout << "- LP-optimum value:    " << lp_opt << "\n\n";

		delete cg;
	}
}
