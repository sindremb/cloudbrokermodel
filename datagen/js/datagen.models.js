// Main viewmodel class
function DataModel(datavm) {

	this.numCustomers = datavm.numCustomers();
    this.numServices = datavm.numServices();
    this.numProviders = datavm.numProviders();
	
    this.network = new NetworkModel(datavm.network());
	
    this.customers = [];
    
	for(var i in datavm.customers()) {
		this.customers.push(new CustomerModel(datavm.customers()[i]))
	}
}

// network viewmodel class
function NetworkModel(networkvm) {

	this.numberOfNodes = networkvm.numberOfNodes();
	
	this.isSymmetric = networkvm.isSymmetric();

    this.arcs = []
	this.nodes = []
    this.leasableArcs = []
	
	for(var i in networkvm.arcs()) {
		this.arcs.push(new ArcModel(networkvm.arcs()[i]));
	}
	
	for(var i in networkvm.nodes()) {
		this.nodes.push(new NodeModel(networkvm.nodes()[i]));
	}
	
	for(var i in networkvm.leasableArcs()) {
		this.leasableArcs.push(new ArcModel(networkvm.leasableArcs()[i]));
	}
}

// Network arc model class
function ArcModel(arcvm) {

    this.nodeTo = arcvm.nodeTo().nodeNumber()-1;
    this.nodeFrom = arcvm.nodeFrom().nodeNumber()-1
    this.latency = arcvm.latency();
    this.bandwidthCap = arcvm.bandwidthCap();
    this.bandwidthPrice = arcvm.bandwidthPrice();
    this.expectedAvailability = arcvm.expectedAvailability();
}

// Node model
function NodeModel(nodevm) {
	this.x = nodevm.x();
	this.y = nodevm.y();
}

// Customer model class
function CustomerModel(customervm) {
    this.revenue = customervm.revenue();
    this.services = [];
	
	for(var i in customervm.services()) {
		this.services.push(new ServiceModel(customervm.services()[i]));
	}
}

// Service model class
function ServiceModel(servicevm) {

    this.bandwidthRequirementUp = servicevm.bandwidthRequirementUp();
    this.bandwidthRequirementDown = servicevm.bandwidthRequirementDown();
    this.latencyRequirement = servicevm.latencyRequirement();
	
	this.availabilityRequirement = servicevm.availabilityRequirement();

    this.placements = [];
	
	for(var i in servicevm.placements()) {
		this.placements.push(new ServicePlacementModel(servicevm.placements()[i]));
	}
}

function ServicePlacementModel(servicePlacementVM) {
    this.provider = servicePlacementVM.provider().providerNumber() -1;
    this.price = servicePlacementVM.price();
}