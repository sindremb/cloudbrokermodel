#include <iostream>
#include <string>
#include <algorithm>

#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>

#include "moseldataparser.h"
#include "entities.h"
#include "pathgenerator.h"


using namespace std;
using namespace pathgen;
using namespace moseldata;
using namespace entities;

struct pathgenConfig {
	bool calcOverlaps;
};

int main()
{
	pathgenConfig config;
	config.calcOverlaps = false;

	cout << "Welcome to network path generation bot v0.1";
	while(true) {
		int rootSelection;
		cout << "\n\n Menu:\n (1) Load (JSON) file and generate paths\n (2) Open editor \n (3) config \n (0) exit\n\nSelection: ";
		cin >> rootSelection;

		if(rootSelection == 1) {
			string inputfilename;
			cout << "\n\nType filename for data to import and press \'enter\' to start: ";
			cin >> inputfilename;

			// STEP 1: import data
			dataContent data;
			loadFromJSONFile(inputfilename.c_str(), &data);

			string outputfilename;
			cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
			cin >> outputfilename;

			// STEP 2: generate paths
			pathgen::generatePaths(&data, config.calcOverlaps);

			// STEP 3: store to generated file
			entities::toMoselDataFile(outputfilename.c_str(), &data);

			cout << "\n\nPaths generated and data stored to " << outputfilename;

		} else if(rootSelection == 2) {
			if ((int)ShellExecute(NULL, "open", "..\\datagen\\index.htm", NULL, NULL, SW_SHOWNORMAL) < 32)
			{
				cout << "\n editor opened";
			}
			else {
				cout << "\n could not open editor";
			}
		} else if(rootSelection == 3) {
			while(true) {
				int configSelection;
				cout << "Pathgen Config\n - Calculate Path Overlaps: " << (config.calcOverlaps ? "true" : "false");
				cout << "\n\n Options:\n (1) Toggle Calculate Path Overlaps\n (0) back\n\nSelection: ";
				cin >> rootSelection;

				if(rootSelection == 1) {
					config.calcOverlaps = !config.calcOverlaps;
				} else if(rootSelection == 0) {
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
