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

int main()
{
	cout << "Welcome to network path generation bot v0.1";
	while(true) {
		int rootSelection;
		cout << "\n\n Menu:\n (1) Load (JSON) file and generate paths\n (2) Open editor \n (0) exit\n\nSelection: ";
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
			pathgen::generatePaths(&data);

			// STEP 3: store to generated file
			entities::toMoselDataFile(outputfilename.c_str(), &data);

			cout << "\n\nPaths generated and data stored to " << outputfilename;

		} else if(rootSelection == 2) {
			const char* urlA = "file://C:\Users\sindremo\cloudbrokermodel\datagen\index.htm";
			wchar_t urlW[150];
			std::copy( urlA, urlA + lstrlenA( urlA ) + 1, urlW );
			//if ((int)ShellExecuteW( NULL, L"open", urlW, NULL, NULL, SW_SHOW ) < 32)
			if ((int)ShellExecute(NULL, "open", "..\\datagen\\index.htm", NULL, NULL, SW_SHOWNORMAL) < 32)
			{
				cout << "\n editor opened";
			}
			else {
				cout << "\n could not open editor";
			}
		} else if(rootSelection == 0) {
			break;
		} else {
			cout << "\n Unknown choice";
		}
	}
	return 0;
}
