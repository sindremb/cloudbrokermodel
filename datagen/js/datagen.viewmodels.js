function MainViewModel() {
	self = this;
	
	// view model for all core data
    this.dataVM = ko.observable(new DataViewModel(this));

	// view model for random data generation configuration
    this.generationConfig = ko.observable(new GenerationConfigViewModel());
	
	// current mode for editor view of data
	this.mode = ko.observable('view');
	
	// current selected objects in editor view
	this.selectedObjects = ko.observableArray();

	// outputs the dataVM content as text to container of provided id
    this.outputToMoselData = function (containerId) {
        $('#' + containerId).val(Datagen.utils.toMoselData(this.dataVM()));
    };
	
	// creates a new dataVM from the provided mosel data string
	this.parseMoselData = function (data) {
		var dataObj = Datagen.utils.parseMoselData(data);
		this.dataVM(Datagen.utils.networkFromDataObject(dataObj, this));
	}

	// generates a new random dataVM according to current configuration
    this.generateRandomNetwork = function (containerId) {
        this.dataVM(Datagen.utils.generateData(this.generationConfig(), this));
        this.outputToMoselData(containerId);
    };
	
	this.setMode = function(mode) {
		if(mode != this.mode()) {
			this.mode(mode);
			this.selectedObjects.removeAll();
		}
	}
	
	// keypress handler
	this.handleKeypress = function(keycode) {
		if(this.mode() == 'edit' || this.mode() == 'view'){
			if(keycode == 46) { // delete key
				this.deleteSelection();
			}
		}
	}
	
	this.deleteSelection = function() {
		// delete all objects
		console.log('! DELETING OBJECTS!')
		for (var i in this.selectedObjects()) {
			var obj = this.selectedObjects()[i];
			console.log('! - DELETING:', obj)
			if(obj instanceof NodeViewModel) {
				this.dataVM().network().nodes.remove(obj);
			} else if(obj instanceof ArcViewModel) {
				this.dataVM().network().arcs.remove(obj);
				this.dataVM().network().leasableArcs.remove(obj);
			}
		}
		
		console.log('! - STARTING CLEANUP:')
		// cleanup (remove arc not attached to nodes in both ends
		var arcsToCleanup = [];
		for (var i in this.dataVM().network().arcs()) {
			var arc = this.dataVM().network().arcs()[i];
			if(this.dataVM().network().nodes.indexOf(arc.nodeTo()) < 0 ||
				this.dataVM().network().nodes.indexOf(arc.nodeFrom()) < 0) {
				arcsToCleanup.push(arc);
			}
		}
		for(var i in arcsToCleanup) {
			this.dataVM().network().arcs.remove(arcsToCleanup[i])
		}
		
		arcsToCleanup = [];
		for (var i in this.dataVM().network().leasableArcs()) {
			var arc = this.dataVM().network().leasableArcs()[i];
			if(this.dataVM().network().nodes.indexOf(arc.nodeTo()) < 0 ||
				this.dataVM().network().nodes.indexOf(arc.nodeFrom()) < 0) {
				arcsToCleanup.push(arc);
			}
		}
		for(var i in arcsToCleanup) {
			this.dataVM().network().leasableArcs.remove(arcsToCleanup[i])
		}
		
		// clear selected objects list
		this.selectedObjects.removeAll();
	}
	
	this.mapclick = function(e) {
		if(this.mode() == 'view' || this.mode() == 'edit' || this.mode() == 'addarc') {
			this.selectedObjects.removeAll();
		} else if(this.mode() == 'addnode') {
			this.dataVM().network().addInternalNodeForLocation(e.offsetX, e.offsetY);
		}
		e.cancelBubble = true;
			if (e.stopPropagation) e.stopPropagation();
	}
	
	this.objectSelected = function(obj) {
		switch(this.mode()) {
			case "addnode":
				break;
			case "addarc":
				if(obj instanceof NodeViewModel) {
					this.selectedObjects.push(obj);
					if(this.selectedObjects().length >= 2) {
						this.dataVM().network().addArcForNodes(
							this.selectedObjects()[0],
							this.selectedObjects()[1]
						);
						this.selectedObjects.removeAll();
					}
				}
				break;
			case "view":
				this.selectedObjects.removeAll();
			case "edit":
				this.selectedObjects.push(obj);
				break;
		}
	}
	
	this.objectDeselected = function(obj) {
		this.selectedObjects.remove(obj);
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
function DataViewModel(main) {
	this.main = main;
	
    this.customers = ko.observableArray();
    this.services = ko.observableArray();
    this.providers = ko.observableArray();
    this.numCustomers = ko.computed(function () { return this.customers().length }, this);
    this.numServices = ko.computed(function () { return this.services().length }, this);
    this.numProviders = ko.computed(function () { return this.providers().length }, this);
    this.network = ko.observable(new NetworkViewModel(this));
	
	this.removePlacementsForProvider = function(provider) {
		for(var i in this.customers()) {
			var customer = this.customers()[i];
			for(var j in customer.services()) {
				var service = customer.services()[j];
				var toRemove = [];
				for(var k in service.placements()){
					var placement = service.placements()[k];
					if(placement.provider() == provider) {
						toRemove.push(placement);
					}
				}
				for(var k in toRemove) {
					service.placements.remove(toRemove[k]);
				}
			}
		}
	}
}

// network viewmodel class
function NetworkViewModel(dataVM) {
	this.dataVM = dataVM;

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
	
	this.addInternalNodeForLocation = function(x, y) {
		this.nodes.splice(this.nodes().length - this.dataVM.providers().length,
			0, new NodeViewModel(this, x, y, 2));
	}
	
	this.addArcForNodes = function(nodeFrom, nodeTo) {
		this.arcs.push(Datagen.utils._arcForNodes(nodeFrom, nodeTo));
	}
	
	this.insertNodeAsInternal = function (node) {
		this.nodes.splice(this.nodes().length - this.dataVM.providers().length,
			0, node);
	}
	
	this.insertNodeAsFirstInternal = function (node) {
		this.nodes.splice(this.dataVM.customers().length,
			0, node);
	}
}

function Selectable(tracker) {
	this.tracker = ko.observable(tracker);

	this.selected = ko.computed(function () {
			if(this.tracker() != null) {
				return this.tracker().selectedObjects.indexOf(this) > -1;
			}
		}, this
	);
	
	this.toggleSelect = function(e) {
		if(this.tracker() != null) {
			if(this.selected()) {
				this.tracker().objectDeselected(this);
			}
			else {
				this.tracker().objectSelected(this);
			}
		}
		if(e != null) {
			e.cancelBubble = true;
			if (e.stopPropagation) e.stopPropagation();
		}
	}
	
	this.getTemplate = function() {
		return 'UnknownTemplate';
	}
}

// Network node viewmodel class
function NodeViewModel(network, x, y, level) {
	Selectable.call(this, network.dataVM.main);
	// alt: Selectable.apply(this, [network.dataVM.main]);
	
    this.network = network;

    this.x = ko.observable(x);
    this.y = ko.observable(y);
	this.level = ko.observable(level);

    this.nodeNumber = ko.computed(function () {
        return this.network.nodes.indexOf(this) + 1;
    }, this);
	
	this.getTemplate = function() {
		return 'NodeViewModelTemplate';
	}
	
	this.nodeType = ko.computed(function () {
		var index = this.network.nodes.indexOf(this);
		if(index < this.network.dataVM.customers().length) {
			return 'customer';
		} else if (index >= (this.network.nodes().length - this.network.dataVM.providers().length)){
			return 'provider';
		}
		return 'internal';
	}, this);
	
	this.content = ko.computed(function() {
		var index = this.network.nodes.indexOf(this);
		if(index < this.network.dataVM.customers().length) {
			return this.network.dataVM.customers()[index];
		} else if (index >= (this.network.nodes().length - this.network.dataVM.providers().length)){
			return this.network.dataVM.providers()[index - (this.network.nodes().length- this.network.dataVM.providers().length)];
		}
		return null;
	}, this);
	
	
	this.removeCustomer = function(customer) {
		var ownerNode = this.network.nodes()[customer.customerNumber()-1]
		if(ownerNode != null) {
			// remove node, customer and customer's services
			this.network.nodes.remove(ownerNode);
			this.network.dataVM.customers.remove(customer);
			for(var i in customer.services()) {
				this.network.dataVM.services.remove(customer.services()[i]);
			}
			// reinsert node as internal node
			this.network.insertNodeAsInternal(ownerNode);
		}
	}
	
	this.removeProvider = function(provider) {
		var ownerNode = this.network.nodes()[this.network.nodes().length - this.network.dataVM.providers().length + provider.providerNumber()-1]
		if(ownerNode != null) {
			// remove node and customer
			this.network.nodes.remove(ownerNode);
			this.network.dataVM.providers.remove(provider);
			// reinsert node as internal node
			this.network.insertNodeAsInternal(ownerNode);
			this.network.dataVM.removePlacementsForProvider(provider);
		}
	}
	
	this.addCustomer = function() {
		if(this.nodeType == 'customer' || this.nodeType == 'provider') {
			console.log('tried adding customer to node already containing customer/provider - aborting..');
			return;
		}
		this.network.nodes.remove(this);
		this.network.insertNodeAsFirstInternal(this);
		this.network.dataVM.customers.push(new CustomerViewModel(this.network.dataVM, 1000));
	}
	
	this.addProvider = function() {
		if(this.nodeType == 'customer' || this.nodeType == 'provider') {
			console.log('tried adding provider to node already containing customer/provider - aborting..');
			return;
		}
		this.network.nodes.remove(this);
		this.network.nodes.push(this);
		this.network.dataVM.providers.push(new ProviderViewModel(this.network.dataVM));
	}
}
NodeViewModel.prototype = new Selectable();

// Network arc viewmodel class
function ArcViewModel(nodeTo, nodeFrom, latency, bandwidthCap, bandwidthPrice, availability) {
	this.network = nodeTo.network;
	Selectable.call(this, nodeTo.network.dataVM.main);
	
    this.nodeTo = ko.observable(nodeTo);
    this.nodeFrom = ko.observable(nodeFrom);
    this.latency = ko.observable(latency);
    this.bandwidthCap = ko.observable(bandwidthCap);
    this.bandwidthPrice = ko.observable(bandwidthPrice);
    this.expectedAvailability = ko.observable(availability);
	
	this.getTemplate = function() {
		return 'ArcViewModelTemplate';
	}
}
ArcViewModel.prototype = new Selectable( );

// Provider viewmodel class
function ProviderViewModel(dataVM) {
    this.dataVM = dataVM

    this.providerNumber = ko.computed(function () {
        return this.dataVM.providers.indexOf(this) + 1;
    }, this);
	
	this.getTemplate = function() {
		return 'ProviderViewModelTemplate';
	}
	
	this.owner = function() {
		return this.dataVM.network().nodes()[this.dataVM.network().nodes().length - this.dataVM.providers().length + this.providerNumber()-1]
	}
}

// Customer viewmodel class
function CustomerViewModel(dataVM, revenue) {
    this.dataVM = dataVM;
    this.revenue = ko.observable(revenue)
    this.services = ko.observableArray();

    this.customerNumber = ko.computed(function () {
        return this.dataVM.customers.indexOf(this) + 1;
    }, this);
	
	this.getTemplate = function() {
		return 'CustomerViewModelTemplate';
	}
	
	this.removeService = function(service) {
		this.services.remove(service);
		this.dataVM.services.remove(service);
	}
	
	this.owner = function() {
		return this.dataVM.network().nodes()[this.customerNumber()-1]
	}
	
	this.addService = function() {
		this.services.push(new ServiceViewModel(
			this.dataVM, 20, 100, 20, 100, 0.95
		));
	}
}

// Service viewmodel class
function ServiceViewModel(dataVM, bandwidthReq, latencyReq, bandwidthReqDown, latencyReqDown, availabilityReq) {
    this.dataVM = dataVM;
    this.dataVM.services.push(this);

    this.bandwidthRequirementUp = ko.observable(bandwidthReq);
    this.latencyRequirementUp = ko.observable(latencyReq);
    this.bandwidthRequirementDown = ko.observable(bandwidthReqDown);
    this.latencyRequirementDown = ko.observable(latencyReqDown);
	this.availabilityRequirement = ko.observable(availabilityReq)

    this.placements = ko.observableArray();

    this.serviceNumber = ko.computed(function () {
        return this.dataVM.services.indexOf(this) + 1;
    }, this);
	
	this.removePlacement = function(placement) {
		this.placements.remove(placement);
	}
	
	this.owner = function() {
		for(var i in this.dataVM.customers()) {
			if(this.dataVM.customers()[i].services.indexOf(this) >= 0) {
				return this.dataVM.customers()[i];
			}
		}
	}
	
	this.addPlacement = function(provider, price) {
		this.placements.push(new ServicePlacementViewModel(this, provider, price));
	}
	
	this.nonPlaceableProviders = ko.computed(function() {
		var nonPlaceable = [];
		for(var i in this.dataVM.providers()) {
			var provider = this.dataVM.providers()[i];
			var placeable = false;
			for(var j in this.placements()) {
				if(provider == this.placements()[j].provider()) {
					placeable = true;
				}
			}
			if(!placeable) {
				nonPlaceable.push(provider);
			}
		}
		return nonPlaceable;
	}, this);
}

function ServicePlacementViewModel(service, provider, price) {
    this.service = ko.observable(service);
    this.provider = ko.observable(provider);
    this.price = ko.observable(price);
}