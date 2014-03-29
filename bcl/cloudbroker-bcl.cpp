//============================================================================
// Name        : cloudbroker-bcl.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================


//#define EXCLUDE_BCL /* Uncomment to ignore any BCL specific code from project */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "entities.h"
#include "pathgenerator.h"

#ifndef EXCLUDE_BCL
#include "bcltest.h"
#include "CloudBrokerModel.h"
#endif

using namespace std;

int main(void) {
	entities::pathgenConfig config;
		config.calcComboAvailabilities = true;
		config.maxPathsPerPlacement = 50;

		cout << "Welcome to network path generation bot v0.4";
		while(true) {
			int rootSelection;
			cout << "\n\n Menu:\n"
					" (1) Load (JSON) file and generate common path and mapping mosel data file\n";
#ifndef EXCLUDE_BCL
			cout << " (2) Generate mappings and run cloudbroker opt bcl version\n"
					" (3) Generate paths and run cloudbroker opt mapping column generation bcl version\n"
					" (4) BCL-test\n";
#endif
			cout << " (5) Config\n"
					" (0) exit\n\nSelection: ";
			cin >> rootSelection;

			if(rootSelection == 1) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				if(entities::loadFromJSONFile(inputfilename.c_str(), &data)) {

					string outputfilename;
					cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
					cin >> outputfilename;

					// STEP 2: generate paths and additional data as configured
					pathgen::generatePaths(&data, config);
					if (config.calcComboAvailabilities) pathgen::addPathComboAvailabilities(&data);

					// STEP 3: find and add feasible mappings from generated paths
					pathgen::addFeasibleMappings(&data);

					// STEP 4: store to generated file
					cout << "\n\nWriting to mosel data file..";
					entities::toMoselDataFile(outputfilename.c_str(), &data);

					cout << "\n\nPaths generated and data stored to " << outputfilename;
				}
#ifndef EXCLUDE_BCL
			} else if(rootSelection == 2) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				if(entities::loadFromJSONFile(inputfilename.c_str(), &data)) {

					// STEP 2: generate paths+mappings and additional data as configured
					pathgen::generatePaths(&data, config);
					if (config.calcComboAvailabilities) pathgen::addPathComboAvailabilities(&data);
					pathgen::addFeasibleMappings(&data);

					// STEP 3: build model from data
					cout << "\nSTEP3: build model!";
					cloudbrokermodels::CloudBrokerModel model;
					cout << "\n Starting to build model..\n";
					model.BuildModel(&data, 0.3);
					cout << "\n Finished building model!\n";

					// STEP 4: run optimisation
					model.RunModel(true);

					model.OutputResults();
				}
			} else if(rootSelection == 3) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				if(entities::loadFromJSONFile(inputfilename.c_str(), &data)) {

					// STEP 2: generate paths+mappings and additional data as configured
					pathgen::generatePaths(&data, config);
					if (config.calcComboAvailabilities) pathgen::addPathComboAvailabilities(&data);

					// STEP 3: build model from data
					cout << "\nSTEP3: build model!";
					cloudbrokermodels::CloudBrokerModel model;
					cout << "\n Starting to build model..\n";
					model.BuildModel(&data, 0.3);
					cout << "\n Finished building model!\n";

					// STEP 4: run optimisation
					model.RunModelColumnGeneration();

					model.OutputResults();
				}
			} else if(rootSelection == 4) {
				bcltest::runBclTest();
#endif
			} else if(rootSelection == 5) {
				while(true) {
					int configSelection;
					cout << "Pathgen Config\n - Max number of paths per placement: " << config.maxPathsPerPlacement;
					cout << "\n - Calculate Path Combo Availabilities: " << (config.calcComboAvailabilities ? "true" : "false");
					cout << "\n\n Options:\n (1) Set max number of paths per placement\n (2) Toggle Calculate Path Combo availabilities (0) back\n\nSelection: ";
					cin >> configSelection;

					if(configSelection == 1) {
						cout << "\n\nEnter max number of paths per placement: ";
						int maxnum;
						cin >> maxnum;
						config.maxPathsPerPlacement = maxnum;
					} else if (configSelection == 2) {
						config.calcComboAvailabilities = !config.calcComboAvailabilities;
					}
					else if (configSelection == 0) {
						break;
					} else {
						cout << "\n Unknown choice";
					}
				}
			} else if(rootSelection == 0) {
				break;
			} else {
				cout << "\n Unknown choice";
			}
		}
		return 0;
}
