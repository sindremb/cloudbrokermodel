// Main viewmodel class
function DataModel(datavm) {

	this.numCustomers = datavm.numCustomers();
    this.numServices = datavm.numCustomers();
    this.numProviders = datavm.numCustomers();
	
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
    this.leasableArcs = []
	
	for(var i in networkvm.arcs()) {
		this.arcs.push(new ArcModel(networkvm.arcs()[i]));
	}
	
	for(var i in networkvm.leasableArcs()) {
		this.leasableArcs.push(new ArcModel(networkvm.leasableArcs()[i]));
	}
}

// Network arc viewmodel class
function ArcModel(arcvm) {

    this.nodeTo = arcvm.nodeTo().nodeNumber()-1;
    this.nodeFrom = arcvm.nodeFrom().nodeNumber()-1
    this.latency = arcvm.latency();
    this.bandwidthCap = arcvm.bandwidthCap();
    this.bandwidthPrice = arcvm.bandwidthPrice();
    this.expectedAvailability = arcvm.expectedAvailability();
}

// Customer viewmodel class
function CustomerModel(customervm) {
    this.revenue = customervm.revenue();
    this.services = [];
	
	for(var i in customervm.services()) {
		this.services.push(new ServiceModel(customervm.services()[i]));
	}
	
}

// Service viewmodel class
function ServiceModel(servicevm) {

    this.bandwidthRequirementUp = servicevm.bandwidthRequirementUp();
    this.latencyRequirementUp = servicevm.latencyRequirementUp();
    this.bandwidthRequirementDown = servicevm.bandwidthRequirementDown();
    this.latencyRequirementDown = servicevm.latencyRequirementDown();
	
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