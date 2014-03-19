//============================================================================
// Name        : cloudbroker-bcl.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "entities.h"
#include "pathgenerator.h"

using namespace std;

int main(void) {
	//puts("Hello World!!!");
	//return EXIT_SUCCESS;
	entities::pathgenConfig config;
		config.calcOverlaps = false;
		config.calcComboAvailabilities = true;
		config.maxPathsPerPlacement = 50;

		cout << "Welcome to network path generation bot v0.3";
		while(true) {
			int rootSelection;
			cout << "\n\n Menu:\n (1) Load (JSON) file and generate paths\n (2) Load (JSON) file and generate primary/backup combos\n (3) Load (JSON) file and generate common path and mapping data\n (4) config \n (0) exit\n\nSelection: ";
			cin >> rootSelection;

			if(rootSelection == 1) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				entities::loadFromJSONFile(inputfilename.c_str(), &data);

				string outputfilename;
				cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
				cin >> outputfilename;

				// STEP 2: generate paths and additional data as configured
				pathgen::generatePaths(&data, config);
				if (config.calcComboAvailabilities) pathgen::addPathComboAvailabilities(&data);
				if (config.calcOverlaps) pathgen::addPathOverlaps(&data);

				// STEP 3: store to generated file
				cout << "\n\nWriting to mosel data file..";
				entities::toMoselDataFile(outputfilename.c_str(), &data);

				cout << "\n\nPaths generated and data stored to " << outputfilename;

			} else if(rootSelection == 2) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				entities::loadFromJSONFile(inputfilename.c_str(), &data);

				string outputfilename;
				cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
				cin >> outputfilename;

				// STEP 2: generate paths
				pathgen::generatePaths(&data, config);

				// STEP 3: find and add feasible mappings from generated paths
				pathgen::addFeasibleMappings(&data);

				// STEP 4: store to generated file
				cout << "\n\nWriting to mosel data file..";
				entities::toMoselDataFileV2(outputfilename.c_str(), &data);

				cout << "\n\nPaths generated and data stored to " << outputfilename;
			} else if(rootSelection == 3) {
				string inputfilename;
				cout << "\n\nType filename for data to import and press \'enter\' to start: ";
				cin >> inputfilename;

				// STEP 1: import data
				entities::dataContent data;
				entities::loadFromJSONFile(inputfilename.c_str(), &data);

				string outputfilename;
				cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
				cin >> outputfilename;

				// STEP 2: generate paths and additional data as configured
				pathgen::generatePaths(&data, config);
				if (config.calcComboAvailabilities) pathgen::addPathComboAvailabilities(&data);
				if (config.calcOverlaps) pathgen::addPathOverlaps(&data);

				// STEP 3: find and add feasible mappings from generated paths
				pathgen::addFeasibleMappings(&data);

				// STEP 4: store to generated file
				cout << "\n\nWriting to mosel data file..";
				entities::toMoselDataFileV3(outputfilename.c_str(), &data);

				cout << "\n\nPaths generated and data stored to " << outputfilename;
			} else if(rootSelection == 4) {
				while(true) {
					int configSelection;
					cout << "Pathgen Config\n - Calculate Path Overlaps: " << (config.calcOverlaps ? "true" : "false");
					cout << "\n - Max number of paths per placement: " << config.maxPathsPerPlacement;
					cout << "\n - Calculate Path Combo Availabilities: " << (config.calcComboAvailabilities ? "true" : "false");
					cout << "\n\n Options:\n (1) Toggle Calculate Path Overlaps\n (2) Set max number of paths per placement\n (3) Toggle Calculate Path Combo availabilities (0) back\n\nSelection: ";
					cin >> configSelection;

					if(configSelection == 1) {
						config.calcOverlaps = !config.calcOverlaps;
					} else if(configSelection == 2) {
						cout << "\n\nEnter max number of paths per placement: ";
						int maxnum;
						cin >> maxnum;
						config.maxPathsPerPlacement = maxnum;
					} else if (configSelection == 3) {
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
