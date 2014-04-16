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

	The first argument given is the kind of action to perform, and can be one of the following

		solve / s : solves the normal bcl model with all needed mappings and paths pregenerated

		cgsolve / c : solves the bcl model, using column generation to add mappings

		moseldata / m : stores the imported data, including any generated paths / mappings to
				a mosel data file.
	
	The program is given arguments for deciding what task to perform and with what configuration.
	Arguments can be given independently in the invocation as shown above with a preceeding switch
	of the form "-x" where x informs the program what the following argument (like "data.json") is
	to be interpreted as. The order of the arguments is not fixed. If an argument is specified 
	multiple times, the latter will overwrite any previous specifications.
	
	supported arguments are:
	
	-i <filename>			| no default value, required
	specifies the filename (including relative path) for importing data to the model
	
	-o <filename>			| no default value, optional
	specifies the filename (including relative path) for storing any results from the task to perform
	
	-cgmethod <integer>		| default: 2, optional
	specifies which column generation method to use if solving the bcl model with column generation
		1 : brute force
		2 : heuristic A
		3 : heuristic B 
	
	-beta <float>			| defualt: 0.3, optional
	sets the beta value to use in the backup reservation constraint of the model
	
	-miplimit <integer>		| default: -1, optional
	sets the maximum number of seconds allowed for solving the mip model, ignores limit on -1
	
	-cgmaxiters <integer>	| default: -1, optional
	sets the maximum number of column generation iterations allowed, ignores limit on -1
	
	-cgmaxcount <integer>	| default: -1, optional
	sets the maximum number of columns to add by column generation, ignores limit on -1
	
	-plimit <integer>		| default: -1, optional
	sets the maximum number of paths to generate for each (service, provider) combination, ignores limit on -1
	
	
	
	
	
