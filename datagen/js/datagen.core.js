var Datagen = {};

function MainViewModel() {
	self = this;
    this.dataVM = ko.observable(new DataViewModel());
	
	this.selectedObjects = [];

    this.generationConfig = ko.observable(new GenerationConfigViewModel());

    this.outputToMoselData = function (containerId) {
        $('#' + containerId).val(Datagen.utils.toMoselData(this.dataVM()));
    };

    this.generateRandomNetwork = function (containerId) {
        this.dataVM(Datagen.utils.generateData(this.generationConfig()));
        this.outputToMoselData(containerId);
    };
	
	this.handleKeypress = function(keycode) {
		//alert(keycode);
		if(keycode == 46) { // delete key
			var nodes = this.dataVM().network().nodes()
			var tobeDeleted = []
			for(var i in nodes) {
				if(nodes[i].selected()) {
					tobeDeleted.push(nodes[i]);
				}
			}
			for(var i in tobeDeleted) {
				this.dataVM().network().nodes.remove(tobeDeleted[i]);
			}
			var arcs = this.dataVM().network().arcs()
			tobeDeleted = []
			for(var i in arcs) {
				if(arcs[i].selected()) {
					tobeDeleted.push(arcs[i]);
				}
			}
			for(var i in tobeDeleted) {
				this.dataVM().network().arcs.remove(tobeDeleted[i]);
			}
			arcs = this.dataVM().network().leasableArcs()
			tobeDeleted = []
			for(var i in arcs) {
				if(arcs[i].selected()) {
					tobeDeleted.push(arcs[i]);
				}
			}
			for(var i in tobeDeleted) {
				this.dataVM().network().leasableArcs.remove(tobeDeleted[i]);
			}
		}
	}
	
	window.onkeyup = function(e) {
	   var key = e.keyCode ? e.keyCode : e.which;

	   self.handleKeypress(key);
	}
}

// Data generation config viewmodel
function GenerationConfigViewModel() {
	this._numNodeLevels = ko.observable(2)
	this._numNodesPerCluster = ko.observable(5)
    this._numLeasableArcs = ko.observable(10);
    this._numberOfCustomers = ko.observable(10);
    this._maxNumberOfServicesPerCustomer = ko.observable(3);
    this._numberOfProviders = ko.observable(10);
    this._proportionEligibleProviders = ko.observable(0.5);
	
	this.numNodeLevels = ko.computed({
		read : function() { return this._numNodeLevels(); },
		write : function(value) {
				var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._numNodeLevels(numval)
				}
				else {
					console.log('failed to set "numNodeLevels" to', value);
				}
			},
		owner : this
	});
	
	this.numNodesPerCluster = ko.computed({
		read : function() { return this._numNodesPerCluster(); },
		write : function(value) {
				var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._numNodesPerCluster(numval)
				}
				else {
					console.log('failed to set "numNodesPerCluster" to', value);
				}
			},
		owner : this
	});
	
	this.numLeasableArcs = ko.computed({
		read : function() { return this._numLeasableArcs(); },
		write : function(value) {
				var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._numLeasableArcs(numval)
				}
				else {
					console.log('failed to set "numLeasableArcs" to', value);
				}
			},
		owner : this
	});
	
	this.numberOfCustomers = ko.computed({
		read : function() { return this._numberOfCustomers(); },
		write : function(value) {
				var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._numberOfCustomers(numval)
				}
				else {
					console.log('failed to set "numberOfCustomers" to', value);
				}
			},
		owner : this
	});
	
	this.maxNumberOfServicesPerCustomer = ko.computed({
		read : function() { return this._maxNumberOfServicesPerCustomer(); },
		write : function(value) {
				var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._maxNumberOfServicesPerCustomer(numval)
				}
				else {
					console.log('failed to set "maxNumberOfServicesPerCustomer" to', value);
				}
			},
		owner : this
	});
	
	this.numberOfProviders = ko.computed({
		read : function() { return this._numberOfProviders(); },
		write : function(value) { var numval = parseInt(value);
				if(!isNaN(numval)) {
					this._numberOfProviders(numval)
				}
				else {
					console.log('failed to set "numberOfProviders" to', value);
				}
			},
		owner : this
	});
	
	this.proportionEligibleProviders = ko.computed({
		read : function() { return this._proportionEligibleProviders(); },
		write : function(value) { var numval = parseFloat(value);
				if(!isNaN(numval)) {
					this._proportionEligibleProviders(numval)
				}
				else {
					console.log('failed to set "proportionEligibleProviders" to', value);
				}
			},
		owner : this
	});
}

// Main viewmodel class
function DataViewModel() {
    this.customers = ko.observableArray();
    this.services = ko.observableArray();
    this.providers = ko.observableArray();
    this.numCustomers = ko.computed(function () { return this.customers().length }, this);
    this.numServices = ko.computed(function () { return this.services().length }, this);
    this.numProviders = ko.computed(function () { return this.providers().length }, this);
    this.network = ko.observable(new NetworkViewModel());
}

// network viewmodel class
function NetworkViewModel() {
    this.nodes = ko.observableArray();
    this.arcs = ko.observableArray();
    this.leasableArcs = ko.observableArray();
    this.isSymmetric = ko.observable(true);

    this.numberOfNodes = ko.computed(function () {
            return this.nodes().length;
        }, this
    );

    this.numberOfArcs = ko.computed(function () {
            return this.arcs().length;
        }, this
    );
}

function Selectable() {
	this.selected = ko.observable(false);
	
	this.toggleSelect = function() {
		this.selected(!this.selected());
		return false;
	}
}

// Network node viewmodel class
function NodeViewModel(network, x, y, level, type) {
	Selectable.call(this);
	
    this.network = network;

    this.x = ko.observable(x);
    this.y = ko.observable(y);
	this.level = ko.observable(level);
	this.nodeType = ko.observable(type);

    this.nodeNumber = ko.computed(function () {
        return this.network.nodes.indexOf(this) + 1;
    }, this);
	
	this.colour = ko.computed(function () {
		if(this.nodeType()=='provider'){
			return 'green';
		} else if (this.nodeType()=='customer') {
			return 'red';
		} else {
			return 'gray';
		}
    }, this);
}
NodeViewModel.prototype = new Selectable();

// Network arc viewmodel class
function ArcViewModel(nodeTo, nodeFrom, latency, bandwidthCap, bandwidthPrice, availability) {
	Selectable.call(this);
	
    this.nodeTo = ko.observable(nodeTo);
    this.nodeFrom = ko.observable(nodeFrom);
    this.latency = ko.observable(latency);
    this.bandwidthCap = ko.observable(bandwidthCap);
    this.bandwidthPrice = ko.observable(bandwidthPrice);
    this.expectedAvailability = ko.observable(availability);
}
ArcViewModel.prototype = new Selectable( );

// Provider viewmodel class
function ProviderViewModel(main) {
    this.main = main

    this.providerNumber = ko.computed(function () {
        return this.main.providers.indexOf(this) + 1;
    }, this);
}

// Customer viewmodel class
function CustomerViewModel(main, revenue) {
    this.main = main;
    this.revenue = ko.observable(revenue)
    this.services = ko.observableArray();

    this.customerNumber = ko.computed(function () {
        return this.main.customers.indexOf(this) + 1;
    }, this);
}

// Service viewmodel class
function ServiceViewModel(main, bandwidthReq, latencyReq, bandwidthReqDown, latencyReqDown, availabilityReq) {
    this.main = main;
    this.main.services.push(this);

    this.bandwidthRequirementUp = ko.observable(bandwidthReq);
    this.latencyRequirementUp = ko.observable(latencyReq);
    this.bandwidthRequirementDown = ko.observable(bandwidthReqDown);
    this.latencyRequirementDown = ko.observable(latencyReqDown);
	this.availabilityRequirement = ko.observable(availabilityReq)

    this.placements = ko.observableArray();

    this.serviceNumber = ko.computed(function () {
        return this.main.services.indexOf(this) + 1;
    }, this);
}

function ServicePlacementViewModel(service, provider, price) {
    this.service = ko.observable(service);
    this.provider = ko.observable(provider);
    this.price = ko.observable(price);
}