Datagen.utils = {};

Datagen.utils.toMoselData = function (dataVM) {
    data = '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n' +
            '! Data set: #' + Math.floor((Math.random() * 100)) + '\n' +
            '! - Auto generated dataset by Cloud Broker Datagen v0.1 \n' +
            '!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n';
    data = data + '\nn_Customers: ' + dataVM.numCustomers() + ' ! number of customers\n';
    data = data + 'n_Services: ' + dataVM.numServices() + ' ! number of services in total\n';
    data = data + 'n_Providers: ' + dataVM.numProviders() + ' ! number of providers\n';
    data = data + 'n_Nodes: ' + dataVM.network().numberOfNodes() + ' ! number of nodes in total (customer, internal and provider nodes)\n';

    data = data + '\nSymmetric: ' + (dataVM.network().isSymmetric() ? 'true' : 'false') + '! indicates wether or not arc provided in dataset goes both ways\n';

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
    data = data + 'S_ServiceForCustomer:	[\n'
    for (var i in dataVM.customers()) {
        var customer = dataVM.customers()[i]
        data = data + ' ['
        for (var j in customer.services()) {
            data = data + customer.services()[j].serviceNumber() + ' ';
        }
        data = data + ']\n'
    }
    data = data + ']\n'

    data = data + '\n! (s) Required maximum latency for service s from customer to provider (UPLINK)\n'
    data = data + 'G_LatencyReq: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].latencyRequirementUp() + ' ';
    }
    data = data + ']\n'

    data = data + '\n! (s) Required maximum latency for service s from provider to customer (DOWNLINK)\n'
    data = data + 'G_LatencyReqD: ['
    for (var i in dataVM.services()) {
        data = data + dataVM.services()[i].latencyRequirementDown() + ' ';
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

    return data
};

Datagen.utils._arcForNodes = function(nodeA, nodeB, config) {
	return new ArcViewModel(
					nodeA,
					nodeB,
					Math.floor((Math.random() * 40)) + 10,
					Math.floor((Math.random() * 180)) + 20,
					Math.floor((Math.random() * 4)) + 1,
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
		var node = new NodeViewModel(network, cx + (100/levelCount)*Math.sin(step*i), cy + (100/levelCount)*Math.cos(step*i), levelCount, 'internal');
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

Datagen.utils.generateData = function (genConfig) {
    var dataVM = new DataViewModel()

    // STEP 1: Generate network
    var network = new NetworkViewModel()
	Datagen.utils._generateNodeLevelRecursive(400, 250, null, network, genConfig, 1)
    
	/*
    // generate random leasable arcs
    for (var i = 0; i < genConfig.numLeasableArcs(); i++) {
        network.leasableArcs.push(new ArcViewModel(
                network.nodes()[(Math.floor((Math.random() * network.nodes().length)))],
                network.nodes()[(Math.floor((Math.random() * network.nodes().length)))],
                Math.floor((Math.random() * 50)) + 20,
                Math.floor((Math.random() * 1000)) + 200,
                Math.floor((Math.random() * 6)) + 2,
				Math.random()
            ));
    }*/
    
    // STEP 2: Generate customers and services
	
	var numLeafNodes = Math.pow(genConfig.numNodesPerCluster(), genConfig.numNodeLevels());
	var firstIndex = network.nodes().length - numLeafNodes;
    for (var i = 0; i < genConfig.numberOfCustomers(); i++) {
		var neighbour = network.nodes()[firstIndex+i+(Math.floor((Math.random() * numLeafNodes)))];
		var node = new NodeViewModel(network, neighbour.x() -25 + Math.floor(Math.random()*50), neighbour.y() -25 + Math.floor(Math.random()*50), genConfig.numNodeLevels()+1, 'customer');
		network.nodes.unshift(node);
		network.arcs.push(Datagen.utils._arcForNodes(neighbour, node, genConfig));
        var customer = new CustomerViewModel(dataVM, Math.floor((Math.random() * 1000)) + 500)
        numServices = Math.floor((Math.random() * genConfig.maxNumberOfServicesPerCustomer())) + 1
        for (var j = 0; j < numServices; j++) {
            customer.services.push(new ServiceViewModel(
                    dataVM, Math.floor((Math.random() * 15)) + 5, Math.floor((Math.random() * 250)) + 50,
                    Math.floor((Math.random() * 15)) + 5, Math.floor((Math.random() * 250)) + 50,
					(Math.random() * 0.025) + 0.97
                ));
        }
        dataVM.customers.push(customer);
    }

    // STEP 3: Generate providers
    for (var i = 0; i < genConfig.numberOfProviders(); i++) {
        dataVM.providers.push(new ProviderViewModel(dataVM));
		var neighbour = network.nodes()[genConfig.numberOfCustomers() + Math.floor(Math.random() * genConfig.numNodesPerCluster())];
		var node = new NodeViewModel(network, neighbour.x() -25 + Math.floor(Math.random()*50), neighbour.y() -25 + Math.floor(Math.random()*50), genConfig.numNodeLevels()+1, 'provider');
		network.nodes.push(node);
		network.leasableArcs.push(Datagen.utils._arcForNodes(neighbour, node, genConfig));
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