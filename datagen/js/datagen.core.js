var Datagen = {};

function MainViewModel() {
    this.dataVM = ko.observable(new DataViewModel());

    this.generationConfig = ko.observable(new GenerationConfigViewModel());

    this.outputToMoselData = function (containerId) {
        $('#' + containerId).val(Datagen.utils.toMoselData(this.dataVM()));
    };

    this.generateRandomNetwork = function (containerId) {
        this.dataVM(Datagen.utils.generateData(this.generationConfig()));
        this.outputToMoselData(containerId);
    };
}

// Data generation config viewmodel
function GenerationConfigViewModel() {
    this.numberOfLeasableArcs = ko.observable(10);
    this.numberOfCustomers = ko.observable(10);
    this.maxNumberOfServicesPerCustomer = ko.observable(3);
    this.numberOfProviders = ko.observable(10);
    this.proportionEligibleProviders = ko.observable(0.5);
    this.numberOfNodes = ko.observable(this.numberOfProviders() + this.numberOfCustomers() + 10);
    this.numberOfArcs = ko.observable(this.numberOfNodes()*3);
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

// Network node viewmodel class
function NodeViewModel(network, x, y) {
    this.network = network;

    this.x = ko.observable(x)
    this.y = ko.observable(y)

    this.nodeNumber = ko.computed(function () {
        return this.network.nodes.indexOf(this) + 1;
    }, this);
}

// Network arc viewmodel class
function ArcViewModel(nodeTo, nodeFrom, latency, bandwidthCap, bandwidthPrice, availability) {
    this.nodeTo = ko.observable(nodeTo);
    this.nodeFrom = ko.observable(nodeFrom);
    this.latency = ko.observable(latency);
    this.bandwidthCap = ko.observable(bandwidthCap);
    this.bandwidthPrice = ko.observable(bandwidthPrice);
    this.expectedAvailability = ko.observable(availability);
}

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