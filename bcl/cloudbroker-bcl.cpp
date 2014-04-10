//============================================================================
// Name        : cloudbroker-bcl.cpp
// Author      : Sindre Møgster Braaten
// Version     :
// Copyright   : Your copyright notice
// Description : CloudBroker-model top level app logic in C++, Ansi-style
//============================================================================

//#define EXCLUDE_BCL /* Uncomment to exclude any BCL dependent code when compiling */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <fstream>

#include "entities.h"
#include "pathgenerator.h"

#ifndef EXCLUDE_BCL
#include "CloudBrokerModel.h"
#endif

using namespace std;

struct cloudBrokerConfig {
	// pregeneration configuartion
	int pregen_paths_limit;

	// perform tasks toggles
	// - pregeneration tasks
	bool pregen_paths;
	bool pregen_mappings;
	bool pregen_path_combos;
	// - main tasks
	bool generate_mosel_data;
	bool complete_bcl_solve;
	bool columngen_bcl_solve;

	// optimiser config
	double mip_time_limit;
	double model_beta;

	// column generation config
	int column_generation_method;
	int column_generation_count_limit;
	int column_generation_iter_limit;

	// input / output filenames
	string input_file;
	string output_file;
};

struct configModel {
	bool	calc_combo_availabilities;
	int 	max_paths_per_placement;
	int 	column_gen_alg;
};

cloudBrokerConfig defaultConfig() {
	cloudBrokerConfig config;
	config.pregen_paths = false;
	config.pregen_paths_limit = -1;
	config.pregen_path_combos = false;
	config.pregen_mappings = false;

	config.generate_mosel_data = false;
	config.complete_bcl_solve = false;
	config.columngen_bcl_solve = true;

	config.model_beta = 0.3;
	config.mip_time_limit = -1.0;
	config.column_generation_method = 2;
	config.column_generation_count_limit = -1;
	config.column_generation_iter_limit = -1;

	return config;
}

/* Takes a stream and a configuration as input, pushes information about the given config to the
 * given stream.
 */
void outputMetaDataToStream(std::ostream& stream, cloudBrokerConfig* config) {
	stream << "################## RUN META DATA ###################\n";

	// add timestamp for data
	stream << "\nTime = ";
	time_t t = time(0);   // get time now
	struct tm * now = localtime( & t );
	stream 	<< (now->tm_year + 1900) << '-'
			<< (now->tm_mon + 1) << '-'
			<< now->tm_mday << ' '
			<< now->tm_hour << ':' << now->tm_min << '\n';

	// add configuration data
	stream << "\nInput = " << config->input_file << "\n";
	if(config->complete_bcl_solve) {
		stream << "\nTask = solve BCL model including ALL PREGENERATED MAPPINGS\n";
	} else if (config->columngen_bcl_solve) {
		stream << "\nTask = solve BCL model adding mappings by COLUMN GENERATION\n";
	}

	if(config->complete_bcl_solve || config->columngen_bcl_solve) {
		stream << "\nMIP-solve time limit = " << config->mip_time_limit << "\n";
		stream << "model beta = " << config->model_beta << "\n";
	}

	if(config->pregen_paths) {
		stream << "\n# pregeneration configuration\n"
				<< "pregen path limit (service, provider) = " << config->pregen_paths_limit << "\n"
				<< "pregen path combo availability = " << (config->pregen_paths_limit ? "true" : "false") << "\n"
				<< "pregen mappings = " << (config->pregen_mappings ? "true" : "false") << "\n";
	}

	if(config->columngen_bcl_solve) {
		stream << "\n# column generation configuration\n"
				<< "column generation method = " << config->column_generation_method << "\n"
				<< "column generation max iterations = " << config->column_generation_iter_limit << "\n"
				<< "column generation count limit = " << config->column_generation_count_limit << "\n";
	}

}

void runConfiguration(cloudBrokerConfig config) {
	// load input data
	entities::dataContent data;
	if(entities::loadFromJSONFile(config.input_file.c_str(), &data)) {

		// run any pregeneration
		if(config.pregen_paths) pathgen::generatePaths(&data, config.pregen_paths_limit);
		if(config.pregen_path_combos) pathgen::addPathComboAvailabilities(&data);
		if(config.pregen_mappings) pathgen::addFeasibleMappings(&data);

		// run task
		if(config.generate_mosel_data) entities::toMoselDataFile(config.output_file.c_str(), &data);
		if(config.complete_bcl_solve || config.columngen_bcl_solve) {
			cloudbrokermodels::CloudBrokerModel model;
			cout << "Building CloudBroker-model..";
			model.BuildModel(&data, config.model_beta);
			cout << "COMPLETE!\n";

			if(config.columngen_bcl_solve) {
				cout << "Solving CloudBroker-model by column generation..\n";
				model.RunModelColumnGeneration(
					config.column_generation_method,
					config.column_generation_iter_limit,
					config.column_generation_count_limit,
					config.mip_time_limit
				);
			} else {
				cout << "Solving CloudBroker-model using pregenerated mappings..\n";
				model.RunModel(true);
			}
			// print solution to console
			model.OutputResultsToStream(cout);
			if(!config.output_file.empty()) {
				// try storing results to file
				ofstream myfile;
				myfile.open(config.output_file.c_str());

				if(myfile) {
					// include information from configuration used
					outputMetaDataToStream(myfile, &config);
					// include solution to optimisation problem
					model.OutputResultsToStream(myfile);

					myfile.close();
					cout << "results were stored to file " << config.output_file;
				} else {
					cerr << "could not open file <" << config.output_file << "> for writing results\n";
				}
			}
		}
	} else {
		cerr << "Unable to load input data from <" << config.input_file << ">\n";
	}
}

string getConsoleString(string description) {
	string s;
	cout << description;
	getline(cin, s);
	return s;
}

int getConsoleInt(string description) {
	int number;
	string input;
	while(true) {
		cout << description;

		// collect input
		getline(cin, input);

		// This code converts from string to number safely.
		stringstream myStream(input);
		if (myStream >> number)
			break;
		cerr << "Invalid integer input <" << input << ">\n";
	}
	// should never be reached
	return number;
}

double getConsoleDouble(string description) {
	double number;
	string input;
	while(true) {
		cout << description;

		// collect input
		getline(cin, input);

		// This code converts from string to number safely.
		stringstream myStream(input);
		if (myStream >> number)
			break;
		cerr << "Invalid number input <" << input << ">\n";
	}
	// should never be reached
	return number;
}

void textUI() {
	cloudBrokerConfig config = defaultConfig();

	cout << "Welcome to network path generation bot v0.5";
	while(true) {
		int rootSelection = getConsoleInt(
			"\n\n Menu:\n"
			" (1) Load (JSON) file and generate common path and mapping mosel data file\n"
#ifndef EXCLUDE_BCL
			" (2) Pregenerate all mappings and solve CloudBroker-model\n"
			" (3) Use column generation to generate mappings and solve CloudBroker-Model\n"
#endif
			" (4) Configuration\n"
			" (0) exit\n\nSelection: "
		);

		if(rootSelection == 1) {
			// configure to generate mosel data with generated paths and mappings
			config.generate_mosel_data = true;
			config.pregen_paths = true;
			config.pregen_mappings = true;
			config.complete_bcl_solve = false;
			config.columngen_bcl_solve = false;

			// get filename to import
			config.input_file = getConsoleString("\nEnter filename for data to import: ");

			// get filename to store mosel data to
			config.output_file = getConsoleString("\nEnter filename for storing generated mosel data: ");

			runConfiguration(config);
#ifndef EXCLUDE_BCL
		} else if(rootSelection == 2) {
			// configure to run complete bcl model using pregenerated paths and mappings
			config.complete_bcl_solve = true;
			config.pregen_paths = true;
			config.pregen_mappings = true;
			config.columngen_bcl_solve = false;
			config.generate_mosel_data = false;

			// get filename to import
			config.input_file = getConsoleString("\nEnter filename for data to import: ");

			// get filename to store results to
			config.output_file = getConsoleString("\nEnter filename for storing results: ");

			runConfiguration(config);
		} else if(rootSelection == 3) {
			// configure to solve bcl model using column generation to add mappings
			config.columngen_bcl_solve = true;
			config.pregen_paths = config.column_generation_method == 1; // only pregenerate paths for brute force mapping generation (method 1)
			config.complete_bcl_solve = false;
			config.pregen_mappings = false;
			config.generate_mosel_data = false;

			// get filename to import
			config.input_file = getConsoleString("\nEnter filename for data to import: ");

			// get filename to store results to
			config.output_file = getConsoleString("\nEnter filename for storing results: ");

			runConfiguration(config);
#endif
		} else if(rootSelection == 4) {
			while(true) {
				stringstream sstm;
				sstm << "\nCloudBroker app config:"
						"\n - Max number of paths per placement: " 		<< config.pregen_paths_limit <<
						"\n - Model backup reservation beta: " 			<< config.model_beta <<
						"\n - Column Generation Method: " 				<< config.column_generation_method <<
						"\n\n Options:\n"
						"(1) Set max number of paths per placement\n"
						"(2) Set model backup reservation beta\n"
						"(3) Change Column Generation Algorithm\n"
						"(0) back\n\nSelection: ";
				int configSelection = getConsoleInt(sstm.str());

				if(configSelection == 1) {
					config.pregen_paths_limit = getConsoleInt("\nEnter max number of paths per placement: ");
				} else if (configSelection == 2) {
					config.model_beta = getConsoleDouble("\nEnter new model backup reservation beta: ");
				} else if (configSelection == 3){
					int colgenchoice = getConsoleInt(
						"\nColumn Generation Algorithms:\n"
						" (1) Brute Force from pregenerated paths\n"
						" (2) Heuristic A\n"
						" (3) Heuristic B\n"
						"\nEnter selection: "
					);
					if(colgenchoice > 0 && colgenchoice <= 3) {
						config.column_generation_method = colgenchoice;
					} else {
						cout << "\nUnknown choice\n";
					}
				}
				else if (configSelection == 0) {
					break;
				} else {
					cout << "\nUnknown choice\n";
				}
			}
		} else if(rootSelection == 0) {
			break;
		} else {
			cout << "\nUnknown choice\n";
		}
	}
}

int main(int argc, char *argv[]) {
	// no arguments given - use simple text based ui
	if(argc == 1) {
		textUI();
	} else {
		// parse arguments and try to execute
		cerr << "Executable call argument parsing not yet implemented";
	}
	return 0;
}
