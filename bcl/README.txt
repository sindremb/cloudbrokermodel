###########################################################################################
#
#	README: Cloud Broker Optimisation v1.0
#	author: Sindre Møgster Braaten
#
###########################################################################################

Cloud Broker optimisation program can be invoked in two different ways, either with 
no arguments, thus launching a simple text based UI, or with arguments, specifying what the
program should do.

1. Invoking with no arguments:

	example: ./cloudbroker-bcl
	
	A simple text based UI will be presented where the options are to select a task to run, enter 
	configuration for the possible tasks, or terminate the program. The data files to import or store data
	to will be asked for after selecting a task to perform. 
	
2. Invoking with arguments

	format: <program> <action> <options>

	example: ./cloudbroker-bcl solve -i data.json -o results.txt

	The first argument given is the action to be performed, and can be one of the following

		solve / s : solves the normal bcl model with needed mappings and paths pregenerated

		cgsolve / c : solves the bcl model, using column generation to add mappings

		moseldata / m : stores the imported data, including any generated paths / mappings to
				a mosel data file.
	
	The program is given arguments for deciding what task to perform and with what configuration.
	Arguments can be given independently in the invocation as shown above with a preceeding switch
	of the form "-x" where x informs the program what the following argument (like "data.json") is
	to be interpreted as. The order of the arguments is not fixed. If an argument is specified 
	multiple times, the latter will overwrite any previous specifications.
	
	supported option arguments are:
	
	-i <filename>			| no default value, required
	specifies the filename (including relative path) for importing data to the model
	
	-o <filename>			| no default value, optional for solve/cgsolve, required for moseldata
	specifies the filename (including relative path) for storing any results from the task to perform
	
	-cgalg <integer>		| default: 3, optional
	specifies which column generation algorithm to use if solving the bcl model with column generation
		1 : brute force
		2 : heuristic A
		3 : heuristic B 
	
	-beta <float>			| defualt: 0.25, optional
	sets the beta value to use in the backup reservation constraint of the model
	
	-maxtime <integer>		| default: 0, optional
	sets the maximum number of seconds allowed for solving the mip model, ignores limit on 0
	Notes: different interpretation depending on sign:
		- n < 0: stops after n seconds
		- n > 0: stops after n seconds if integer solution is found, or at first integer solution found after n seconds.
	
	-cgmaxiters <integer>	| default: -1, optional
	sets the maximum number of column generation iterations allowed, ignores limit on -1
	
	-cgmaxcount <integer>	| default: -1, optional
	sets the maximum number of columns to add by column generation, ignores limit on -1
	
	-cgmaxtime <integer>	| default: -1, optional
	sets the maximum time consumption (in seconds) for column generation, ignores limit on -1
	
	-maxpaths <integer>		| default: -1, optional
	sets the maximum number of paths to generate FOR EACH (service, provider) PAIR, ignores limit on -1
	
	-nomappings				| default: not set
	if given, tells pregeneration to skip pregeneration of mappings where otherwise done as default
	
	-alg <string>			| default: " ", optional
	Choice of the solution algorithm for MIP/LP optimisation, which should be one (or more when applicable) of:
		LP/MIP
		" " solve the problem using the default LP/QP/MIP/MIQP algorithm;
		"d" solve the problem using the dual simplex algorithm;
		"p" solve the problem using the primal simplex algorithm;
		"b" solve the problem using the Newton barrier algorithm;
		"n" use the network solver (LP / for the initial LP (for MIP) );
		"c" continue a previously interrupted optimization run.
		MIP ONLY (will work for solve, but not cgsolve)
		"l" stop after solving the initial continuous relaxation (using MIP information in presolve);
		
	-dedicated 				| default: not set
	if given, all constraint related to not allowing overlap between both backup paths and primary paths of two services
	are skipped, as these are only useful when using a shared protection scheme.
	(this flag also makes the model ignore any beta values given and sets beta to 1.0)
	
	
	
	
	
