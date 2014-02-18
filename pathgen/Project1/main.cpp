#include <iostream>
#include <string>

#include "moseldataparser.h"
#include "entities.h"
#include "pathgenerator.h"


using namespace std;
using namespace pathgen;
using namespace moseldata;
using namespace entities;

int main()
{
	cout << "Welcome to network path generation bot v0.1";
	while(true) {
		int rootSelection;
		cout << "\n\n Menu:\n (1) Load (JSON) file and generate paths\n (0) exit\n\nSelection: ";
		cin >> rootSelection;

		if(rootSelection == 1) {
			string inputfilename;
			cout << "\n\nType filename for data to import and press \'enter\' to start: ";
			cin >> inputfilename;

			// STEP 1: import data
			dataContent data = loadFromJSONFile(inputfilename.c_str());

			string outputfilename;
			cout << "\nJSON data loaded..\n\nEnter filename for storing generated data and press enter to start path generation: ";
			cin >> outputfilename;

			// STEP 2: generate paths
			pathgen::generatePaths(&data);

			// STEP 3: store to generated file
			entities::toMoselDataFile(outputfilename.c_str(), &data);

			cout << "\n\nPaths generated and data stored to " << outputfilename;

		} else if(rootSelection == 0) {
			break;
		} else {
			cout << "\n Unknown choice";
		}
	}
	return 0;
}
