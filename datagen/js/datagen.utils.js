Datagen.utils = {};

/*
	Takes mosel data string as input and parses to dictionary of parameter names
	as keys with their respective data.
	
	NOTE: does not currently support dynamic arrays of dynamic arrays/arrays
*/
Datagen.utils.parseMoselData = function (data) {
	
	// parsing state
	var dataValues = {}; // the parsed data
	var currentParamName; // name of current unfinished data parameter
	var parsingMode = 'paramname'; // current mode of parsing (what is being looked for)
	var currentParamData; // data parsed so far for current data parameter
	var dataStack = [] // used for nested lists (arrays / dynamic arrays)
	var currentParamIndex; // parsed index for next data value (dynamic arrays)
	
	// helper function
	var issueWarning = function (symbol, mode) {
		console.log('WARNING: encountered unexpected word ', symbol,
			' when parsing data in mode ', mode, ' - IGNORING');
	}
	
	// parse line by line
	var lines = data.split('\n');
	for (var lkey in lines) {
		// extract next line
		var line = lines[lkey];
		// all words before comment symbol
		var words = line.split('!')[0].trim().split(/\s+/g); // \s should include all whitespaces as delimiter
		for (var wkey in words) {
			// split words like '[(1' .. into '[', '(', '1', ...
			var subwords = Datagen.utils._extractSubwords(words[wkey]);
			for (var skey in subwords) {
				var subword = subwords[skey];
				
				// continue to next on empty word
				if(subword.length < 1) continue;
				
				// parse subword depending on mode
				switch(parsingMode) {
					case "paramname":
						if(/^[a-zA-Z_]/.test(subword)) {
							// look for new param declaration
							currentParamName = subword;
							parsingMode = 'assignsymbol';
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "assignsymbol":
						if (subword == ':') {
							parsingMode = 'paramdata';
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "paramdata":
						if(subword == '[') {
							// start parsing list (array or dynamic array)
							parsingMode = "listdata";
							currentParamData = [];
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)[.][0-9]+$/.test(subword)) {
							// assign float value to current param
							var val = parseFloat(subword);
							dataValues[currentParamName] = val;
							parsingMode = "paramname";
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)+$/.test(subword)) {
							// assign int value to current param
							var val = parseInt(subword);
							dataValues[currentParamName] = val;
							parsingMode = "paramname";
						} else if (/^(true|false)$/.test(subword)) {
							// assign bool value to current param
							var val = subword == 'true';
							dataValues[currentParamName] = val;
							parsingMode = "paramname";
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "listdata":
						if(subword == '(') {
							// start parsing index -> set to dynamic array
							currentParamIndex = [];
							currentParamData = {};
							parsingMode = 'index';
						} else if(subword == '[') {
							// nested list -> add current to stack and go again
							dataStack.push(currentParamData);
							currentParamData = [];
						} else if(subword == ']') {
							// finish current array 
							//- pop back to parent array or
							if(dataStack.length > 0) {
								var temp = dataStack.pop(currentParamData);
								temp.push(currentParamData);
								currentParamData = temp;
								parsingMode = 'arraydata';
							// or set param in main data structure
							} else {
								dataValues[currentParamName] = currentParamData;
								parsingMode = 'paramname'
							}
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)[.][0-9]+$/.test(subword)) {
							// assign float value to current param
							var val = parseFloat(subword);
							currentParamData = [val];
							parsingMode = "arraydata";
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)+$/.test(subword)) {
							// assign int value to current param
							var val = parseInt(subword);
							currentParamData = [val];
							parsingMode = "arraydata";
						} else if (/^(true|false)$/.test(subword)) {
							// assign bool value to current param
							var val = subword == 'true';
							currentParamData = [val];
							parsingMode = "arraydata";
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "arraydata":
						if(subword == ']') {
							// finish current array 
							//- pop back to parent array or
							if(dataStack.length > 0) {
								var temp = dataStack.pop(currentParamData);
								temp.push(currentParamData);
								currentParamData = temp;
								parsingMode = 'arraydata';
							// or set param in main data structure
							} else {
								dataValues[currentParamName] = currentParamData;
								parsingMode = 'paramname'
							}
						} else if(subword == '[') {
							// start parsing index -> set to dynamic array
							dataStack.push(currentParamData);
							currentParamData = [];
							parsingMode = 'listdata';
						}  else if (/^-{0,1}([0-9]|[1-9][0-9]*)[.][0-9]+$/.test(subword)) {
							// assign float value to current param
							var val = parseFloat(subword);
							currentParamData.push(val);
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)+$/.test(subword)) {
							// assign int value to current param
							var val = parseInt(subword);
							currentParamData.push(val);
						} else if (/^(true|false)$/.test(subword)) {
							// assign bool value to current param
							var val = subword == 'true';
							currentParamData.push(val);
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "index":
						if(subword == ')') {
							// start parsing index -> set to dynamic array
							parsingMode = 'darraydata';
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)+$/.test(subword)) {
							// push int value to current index
							var val = parseInt(subword);
							currentParamIndex.push(val);
						}else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "darraydata":
						if (/^-{0,1}([0-9]|[1-9][0-9]*)[.][0-9]+$/.test(subword)) {
							// assign float value to current param
							var val = parseFloat(subword);
							var dict = currentParamData;
							for(var i = 0; i < currentParamIndex.length-1; i++) {
								if(!((currentParamIndex[i]) in dict)) {
									dict[currentParamIndex[i]] = {};
								} 
								dict = dict[currentParamIndex[i]];
							}
							dict[currentParamIndex[currentParamIndex.length-1]] = val;
							parsingMode = "darrayindex";
						} else if (/^-{0,1}([0-9]|[1-9][0-9]*)+$/.test(subword)) {
							// assign int value to current param
							var val = parseInt(subword);
							var dict = currentParamData;
							for(var i = 0; i < currentParamIndex.length-1; i++) {
								if(!((currentParamIndex[i]) in dict)) {
									dict[currentParamIndex[i]] = {};
								} 
								dict = dict[currentParamIndex[i]];
							}
							dict[currentParamIndex[currentParamIndex.length-1]] = val;
							parsingMode = "darrayindex";
						} else if (/^(true|false)$/.test(subword)) {
							// assign int value to current param
							var val = subword == 'true';
							var dict = currentParamData;
							for(var i = 0; i < currentParamIndex.length-1; i++) {
								if(!((currentParamIndex[i]) in dict)) {
									dict[currentParamIndex[i]] = {};
								} 
								dict = dict[currentParamIndex[i]];
							}
							dict[currentParamIndex[currentParamIndex.length-1]] = val;
							parsingMode = "darrayindex";
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					case "darrayindex":
						if(subword == ']') {
							// finish current array 
							//- pop back to parent array or
							if(dataStack.length > 0) {
								var temp = dataStack.pop(currentParamData);
								temp.push(currentParamData);
								currentParamData = temp;
								parsingMode = 'arraydata';
							// or set param in main data structure
							} else {
								dataValues[currentParamName] = currentParamData;
								parsingMode = 'paramname'
							}
						} else if(subword == '(') {
							// start parsing index -> set to dynamic array
							currentParamIndex = [];
							parsingMode = 'index';
						} else {
							issueWarning(subword, parsingMode)
						}
						break;
					default:
						console.log('WARNING: unexpected parsing mode: ', parsingMode);
				}
			}
		}
	}
	
	return dataValues;
}

Datagen.utils._extractSubwords = function(word) {
	subwords = [];
	startIndex = 0;
	for(var i = 0; i < word.length; i++) {
		var c = word[i];
		if(/^[()\[\]:]/.test(c)) {
			var prevWord = word.substring(startIndex, i);
			if(prevWord.length > 0) subwords.push(prevWord);
			
			subwords.push(c);
			
			startIndex = i+1;
		}
	}
	if(startIndex < word.length) {
		subwords.push(word.substring(startIndex));
	}
	return subwords;
}

Datagen.utils.dataModelToDataVM = function(model, main) {
	var dataVM = new DataViewModel(main)
	
    // STEP 1: Generate network
    var network = new NetworkViewModel(dataVM)
	dataVM.network(network);
	
	// generate nodes from data obj (using given number and locations)
	for(var i in model.network.nodes) {
		network.nodes.push(new NodeViewModel(network,
			model.network.nodes[i].x,
			model.network.nodes[i].y,
			2
		));
	}
	
	// add owned arcs according to data object
	for(var i in model.network.arcs) {
		network.arcs.push(new ArcViewModel(
			network.nodes()[model.network.arcs[i].nodeTo],
			network.nodes()[model.network.arcs[i].nodeFrom],
			model.network.arcs[i].latency, model.network.arcs[i].bandwidthCap,
			model.network.arcs[i].bandwidthPrice, model.network.arcs[i].expectedAvailability
		));
	}
	
    // STEP 2: Generate providers
    for (var i = 0; i < model.numProviders; i++) {
        dataVM.providers.push(new ProviderViewModel(dataVM));
    }
    
    // STEP 3: Generate customers and services
    for (var i in model.customers) {
        var customer = new CustomerViewModel(dataVM, model.customers[i].revenue);
		for (var j in model.customers[i].services) {
			var sm = model.customers[i].services[j];
            var svm = new ServiceViewModel(
				dataVM, sm.bandwidthRequirementUp,
				sm.bandwidthRequirementDown, 
				sm.latencyRequirement,
				sm.availabilityRequirement
			);
			for (var k in sm.placements) {
				svm.addPlacement(
					dataVM.providers()[sm.placements[k].provider],
					sm.placements[k].price
				);
			}
			customer.addService(svm);
        }
        dataVM.customers.push(customer);
    }
	
	return dataVM
}

Datagen.utils.networkFromDataObject = function(dataObj, main) {

	var dataVM = new DataViewModel(main)

    // STEP 1: Generate network
    var network = new NetworkViewModel(dataVM)
	dataVM.network(network);
	
	// generate nodes from data obj (using given number and locations)
	var step = (Math.PI * 2) / dataObj.n_Nodes; // used if no x- and y-coords are defined for nodes
	for(var i = 0; i < dataObj.n_Nodes; i++) {
		network.nodes.push(new NodeViewModel(network,
			dataObj.datagen_x_coords != null ? dataObj.datagen_x_coords[i] : 200*Math.sin(step*i)+400,
			dataObj.datagen_y_coords != null ? dataObj.datagen_y_coords[i] : 200*Math.cos(step*i)+250,
			2
		));
	}
	
	// add owned arcs according to data object
	for(var i in dataObj.F_BandwidthCap) {
		for(var j in dataObj.F_BandwidthCap[i]) {
			network.arcs.push(new ArcViewModel(
				network.nodes()[i-1], network.nodes()[j-1], // -1 because 1-indexed -> 0-indexed
				dataObj.T_LinkLatency[i][j], dataObj.F_BandwidthCap[i][j],
				dataObj.K_CapPrice[i][j], dataObj.D_AvailabilityExp[i][j]
			));
		}
	}
	
	// add leasable arcs according to data object
	for(var i in dataObj.O_LeasedBandwidthCap) {
		for(var j in dataObj.O_LeasedBandwidthCap[i]) {
			network.leasableArcs.push(new ArcViewModel(
				network.nodes()[i-1], network.nodes()[j-1], // -1 because 1-indexed -> 0-indexed
				dataObj.V_LeaseLatency[i][j], dataObj.O_LeasedBandwidthCap[i][j],
				dataObj.L_LeasedPrice[i][j], 1.0
			));
		}
	}

    // STEP 2: Generate providers
    for (var i = 0; i < dataObj.n_Providers; i++) {
        dataVM.providers.push(new ProviderViewModel(dataVM));
    }
    
    // STEP 3: Generate customers and services
	var sNumber = 0;
    for (var i = 0; i < dataObj.n_Customers; i++) {
        var customer = new CustomerViewModel(dataVM, dataObj.R_Revenue[i])
        for (var j in dataObj.S_ServicesForCustomer[i]) {
			var sIndex = sNumber;
			var s = new ServiceViewModel(
				dataVM, dataObj.B_BandwidthReq[sIndex],
				dataObj.B_BandwidthReqD[sIndex],
				dataObj.G_LatencyReq[sIndex],
				dataObj.Y_AvailabilityReq[sIndex]
			);
			sNumber++
			for (var k in dataObj.H_PlacePrice[sNumber]) {
				s.addPlacement(
					dataVM.providers()[k-1],
					dataObj.H_PlacePrice[sNumber][k]
				);
			}
            customer.addService(s);
        }
        dataVM.customers.push(customer);
    }
	
    return dataVM
} 

Datagen.utils.toMoselData = function (dataVM) {
    data = '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n' +
            '! Data set: #' + Math.floor((Math.random() * 100)) + '\n' +
            '! - Auto generated dataset by Cloud Broker Datagen v0.1 \n' +
            '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';
    data = data + '\nn_Customers: ' + dataVM.numCustomers() + ' ! number of customers\n';
    data = data + 'n_Services: ' + dataVM.numServices() + ' ! number of services in total\n';
    data = data + 'n_Providers: ' + dataVM.numProviders() + ' ! number of providers\n';
    data = data + 'n_Nodes: ' + dataVM.network().numberOfNodes() + ' ! number of nodes in total (customer, internal and provider nodes)\n';

    data = data + '\nSymmetric: ' + (dataVM.network().isSymmetric() ? 'true' : 'false') + '! indicates whether or not arc provided in dataset goes both ways\n';

    data = data + '\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';
    data = data + '! Customers, Services and Providers data\n';
    data = data + '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';

    data = data + '\n! (c) Revenue from serving customer c\n';
    data = data + 'R_Revenue: [';
    for (var i in dataVM.customers()) {
        data = data + dataVM.customers()[i].revenue() + ' ';
    }
    data = data + ']\n';

    data = data + '\n! (c) Set of services for customers c\n'
    data = data + 'S_ServicesForCustomer:	[\n'
    for (var i in dataVM.customers()) {
        var customer = dataVM.customers()[i]
        data = data + ' ['
        for (var j in customer.services()) {
            data = data + customer.services()[j].serviceNumber() + ' ';
        }
        data = data + ']\n'
    }
    data = data + ']\n'

    data = data + '\n! (s) Required maximum latency for service s [old: from customer to provider (UPLINK)]\n'
    data = data + 'G_LatencyReq: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].latencyRequirement() + ' ';
    }
    data = data + ']\n'

	data = data + '\n! NOTE: DEPRECATED - no longer used, but kept for now for compatability\n'
    data = data + '! (s) Required maximum latency for service s from provider to customer (DOWNLINK)\n'
    data = data + 'G_LatencyReqD: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].latencyRequirement() + ' ';
    }
    data = data + ']\n'

    data = data + '\n! (s) Required bandwidth for service s from customer to provider (UPLINK)\n'
    data = data + 'B_BandwidthReq: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].bandwidthRequirementUp() + ' ';
    }
    data = data + ']\n'

    data = data + '\n! (s) Required bandwith for service s from provider to customer (DOWNLINK)\n'
    data = data + 'B_BandwidthReqD: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].bandwidthRequirementDown() + ' ';
    }
    data = data + ']\n';
	
	data = data + '\nY_AvailabilityReq: [';
	for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].availabilityRequirement() + ' ';
    }
	data = data + ']\n';

    data = data + '\n! (s, p) Price of placing service s at provider p\n';
    data = data + 'H_PlacePrice: [\n';
    for (var i in dataVM.services()) {
        var service = dataVM.services()[i];
        for (var j in service.placements()) {
            var placement = service.placements()[j];
            data = data + ' (' + service.serviceNumber() + ' ' + placement.provider().providerNumber() + ') ' + placement.price();
        }
        data = data + '\n';
    }
    data = data + ']\n';

    data = data + '\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';
    data = data + '! Network data\n';
    data = data + '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';

    data = data + '\n!!! Own network\n';

    data = data + '\n! (i,j) Latency for arc between nodes i and j\n';
    data = data + 'T_LinkLatency: [\n';
    for (var i in dataVM.network().arcs()) {
        var arc = dataVM.network().arcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.latency();
    }
    data = data + ']\n';

    data = data + '\n! (i,j) Bandwidth capacity between nodes i and j\n';
    data = data + 'F_BandwidthCap: [\n';
    for (var i in dataVM.network().arcs()) {
        var arc = dataVM.network().arcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.bandwidthCap();
    }
    data = data + ']\n';

    data = data + '\n! (i,j) Price per unit of used own capacity between node i and j\n'
    data = data + 'K_CapPrice: [\n'
    for (var i in dataVM.network().arcs()) {
        var arc = dataVM.network().arcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.bandwidthPrice();
    }
    data = data + ']\n'
	
	data = data + '\nD_AvailabilityExp: [\n'
	for (var i in dataVM.network().arcs()) {
        var arc = dataVM.network().arcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.expectedAvailability();
    }
	data = data + ']\n'

    data = data + '\n!!! Leasable network\n';

    data = data + '\n! (i,j) Price per unit leased capacity between node i and j\n';
    data = data + 'L_LeasedPrice: [\n';
    for (var i in dataVM.network().leasableArcs()) {
        var arc = dataVM.network().leasableArcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.bandwidthPrice();
    }
    data = data + ']\n';

    data = data + '\n! (i,j) Latency for leased link between nodes i and j\n';
    data = data + 'V_LeaseLatency: [\n';
    for (var i in dataVM.network().leasableArcs()) {
        var arc = dataVM.network().leasableArcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.latency();
    }
    data = data + ']\n';

    data = data + '\n! (i,j) Capacity for leased link between nodes i and j\n';
    data = data + 'O_LeasedBandwidthCap:[\n';
    for (var i in dataVM.network().leasableArcs()) {
        var arc = dataVM.network().leasableArcs()[i];
        data = data + ' (' + arc.nodeTo().nodeNumber() + ' ' + arc.nodeFrom().nodeNumber() + ') ' + arc.bandwidthCap();
    }
    data = data + ']\n';
	
	data = data + '\n!!! Following data used purely by \'datagen\': keep in file to allow easy editing\n';
	data = data + '\n! X-coordinates of nodes\n';
	data = data + 'datagen_x_coords: [';
	for (var i in dataVM.network().nodes()) {
		var node = dataVM.network().nodes()[i];
		data = data + node.x() + ' ';
	}
	data = data + ']\n';
	data = data + '\n! Y-coordinates of nodes\n';
	data = data + 'datagen_y_coords: [';
	for (var i in dataVM.network().nodes()) {
		var node = dataVM.network().nodes()[i];
		data = data + node.y() + ' ';
	}
	data = data + ']\n';
    return data
};

Datagen.utils._arcForNodes = function(nodeA, nodeB, config) {
	return new ArcViewModel(
					nodeA,
					nodeB,
					Math.floor((Math.random() * config.avgArcLatency()) + (0.5 * config.avgArcLatency())),
					Math.floor(Math.random() * 100) + 50,
					Math.floor(Math.random() * 4) + 1,
					Math.random() * 0.01 + 0.99
				);;
}

Datagen.utils._generateNodeLevelRecursive = function (cx, cy, parent, network, config, levelCount) {
	if(levelCount > config.numNodeLevels()) {
		return;
	}
	
	var step = (Math.PI * 2) / config.numNodesPerCluster();
	// create nodes in cluster (and arc to parent if present)
	for (var i = 0; i < config.numNodesPerCluster(); i++){
		var node = new NodeViewModel(network, cx + (100/levelCount)*Math.sin(step*i), cy + (100/levelCount)*Math.cos(step*i), levelCount);
		network.nodes.push(node);
		if(parent != null) {
			network.arcs.push(Datagen.utils._arcForNodes(parent, node, config));
		}
	}
	// add cluster arcs
	var firstIndex = network.nodes().length - config.numNodesPerCluster();
	for (var i = 0; i < config.numNodesPerCluster(); i++){
		var a = firstIndex+i;
		var b = firstIndex + ((i+1) % (config.numNodesPerCluster()));
		network.arcs.push(Datagen.utils._arcForNodes(network.nodes()[a], network.nodes()[b], config));
	}
	// recurse for each node in cluster
	for (var i = 0; i < config.numNodesPerCluster(); i++){
		var p = network.nodes()[firstIndex+i];
		Datagen.utils._generateNodeLevelRecursive(p.x()+(100/levelCount)*Math.sin(step*i), p.y()+(100/levelCount)*Math.cos(step*i), p, network, config, levelCount +1);
	}
}

Datagen.utils.generateData = function (genConfig, main) {
    var dataVM = new DataViewModel(main)

    // STEP 1: Generate core network
    var network = new NetworkViewModel(dataVM)
	Datagen.utils._generateNodeLevelRecursive(400, 250, null, network, genConfig, 1)
    
    // STEP 2: Generate customers (with customer node and arc) and services
	var numLeafNodes = Math.pow(genConfig.numNodesPerCluster(), genConfig.numNodeLevels());
	var firstIndex = network.nodes().length - numLeafNodes;
    for (var i = 0; i < genConfig.numberOfCustomers(); i++) {
		var neighbour = network.nodes()[firstIndex+i+(Math.floor((Math.random() * numLeafNodes)))];
		var node = new NodeViewModel(network, neighbour.x() -25 + Math.floor(Math.random()*50),
			neighbour.y() -25 + Math.floor(Math.random()*50), genConfig.numNodeLevels()+1);
		network.nodes.unshift(node);
		network.arcs.push(Datagen.utils._arcForNodes(neighbour, node, genConfig));
        var customer = new CustomerViewModel(dataVM, Math.floor((Math.random() * 1000)) + 500)
        numServices = Math.floor((Math.random() * genConfig.maxNumberOfServicesPerCustomer())) + 1
        for (var j = 0; j < numServices; j++) {
            customer.addService(new ServiceViewModel(
                    dataVM, Math.floor((Math.random() * 15)) + 5, Math.floor((Math.random() * 15)) + 5,
                    Math.floor((Math.random() * genConfig.avgServiceLatencyReq()) + 0.5*genConfig.avgServiceLatencyReq()),
					(Math.random() * 0.025) + 0.97
                ));
        }
        dataVM.customers.push(customer);
    }

    // STEP 3: Generate providers (with nodes and arcs)
    for (var i = 0; i < genConfig.numberOfProviders(); i++) {
        dataVM.providers.push(new ProviderViewModel(dataVM));
		var neighbour = network.nodes()[genConfig.numberOfCustomers() + Math.floor(Math.random() * genConfig.numNodesPerCluster())];
		var node = new NodeViewModel(network, neighbour.x() -25 + Math.floor(Math.random()*50),
			neighbour.y() -25 + Math.floor(Math.random()*50), genConfig.numNodeLevels()+1);
		network.nodes.push(node);
		network.arcs.push(Datagen.utils._arcForNodes(neighbour, node, genConfig));
    }

    // STEP 4: Map services to eligible providers
    for (var i in dataVM.services()) {
        var service = dataVM.services()[i];
        for (var j in dataVM.providers()) {
            if (Math.random() <= genConfig.proportionEligibleProviders()) {
                provider = dataVM.providers()[j]
                service.placements.push(new ServicePlacementViewModel(
                    service, provider, Math.floor((Math.random() * 300)) + 50
                ));
            }
        }
    }
	
	// add network to data viewmodel
	dataVM.network(network);
    return dataVM
};