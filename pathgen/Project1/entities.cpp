#include <iostream>
#include <fstream>
#include <string>

#include "entities.h"
#include "libjson.h"


namespace entities {

	using namespace std;

	string _fileAsString(const char * filename) {
		string line;
		string content;
		ifstream myfile(filename);
		if (myfile.is_open())
		{
			while (myfile.good())
			{
				getline(myfile, line);
				content = content + line;
			}
			myfile.close();
		}

		return content;
	}

	service _parseJsonServiceObj(const JSONNode & n) {
		service service;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){

			// get the node name and value as a string
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "bandwidthRequirementUp"){
				service.bandwidth_req_up = i->as_int();
			}
			else if (node_name == "bandwidthRequirementDown") {
				service.bandwidth_req_down = i->as_int();
			}
			else if (node_name == "latencyRequirement") {
				service.latency_req = i->as_int();
			}
			else if (node_name == "availabilityRequirement") {
				service.availability_req = i->as_float();
			}
			else if (node_name == "placements") {
				JSONNode::const_iterator j = i->begin();
				while (j != i->end()){
					placement placement;

					JSONNode::const_iterator p = j->begin();
					while(p != j->end()) {
						std::string node_name_p = p->name();
						// find out where to store the values
						if (node_name_p == "provider"){
							placement.provider_number = p->as_int();
						}
						else if (node_name_p == "price") {
							placement.price = p->as_int();
						}

						++p;
					}
					
					service.possible_placements.push_back(placement);

					//increment the iterator
					++j;
				}
			}
			//increment the iterator
			++i;
		}
		return service;
	}


	customer _parseJsonCustomerObj(const JSONNode & n) {
		customer customer;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){
			// get the node name
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "revenue"){
				customer.revenue = i->as_int();
			}
			else if (node_name == "services"){
				JSONNode::const_iterator j = i->begin();
				while (j != i->end()){
					customer.services.push_back(_parseJsonServiceObj(*j));
					++j;
				}
			}

			//increment the iterator
			++i;
		}

		return customer;
	
	}

	network _parseJsonNetworkObj(const JSONNode & n) {
		network network;

		bool symmetric;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){
			// recursively call ourselves to dig deeper into the tree
			//if (i->type() == JSON_ARRAY || i->type() == JSON_NODE){
				//ParseJSON(*i);
			//}

			// get the node name and value as a string
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "numberOfNodes"){
				network.n_nodes = i->as_int();
			}
			else if (node_name == "isSymmetric") {
				symmetric = i->as_bool();
			}
			else if (node_name == "arcs") {
				JSONNode::const_iterator j = i->begin();
				// iterate over all arc json nodes
				while (j != i->end()){
					// create arc object
					arc arc;

					// iterate over all arc attributes
					JSONNode::const_iterator k = j->begin();
					while(k != j->end()) {
						// get the node name and value as a string
						std::string node_name_k = k->name();

						// find out where to store the values
						if (node_name_k == "nodeTo"){
							arc.endNode = k->as_int();
						}
						else if (node_name_k == "nodeFrom") {
							arc.startNode = k->as_int();
						}
						else if (node_name_k == "latency") {
							arc.latency = k->as_int();
						}
						else if (node_name_k == "bandwidthCap") {
							arc.bandwidth_cap = k->as_int();
						}
						else if (node_name_k == "bandwidthPrice") {
							arc.bandwidth_price = k->as_int();
						}
						else if (node_name_k == "expectedAvailability") {
							arc.exp_availability = k->as_float();
						}

						k++;
					}

					network.arcs.push_back(arc);

					//increment the iterator
					++j;
				}
			}

			//increment the iterator
			++i;
		}

		if(symmetric) {
			for(int a = 0, originalSize = network.arcs.size(); a < originalSize; ++a) {
				arc sym = network.arcs[a];
				sym.startNode = network.arcs[a].endNode;
				sym.endNode = network.arcs[a].startNode;
				network.arcs.push_back(sym);
			}
		}

		return network;
	}

	dataContent _parseJsonObject(const JSONNode & n) {

		dataContent data;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){
			// get the node name
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "numCustomers"){
				data.n_customers = i->as_int();
			}
			else if (node_name == "numServices"){
				data.n_services = i->as_int();
			}
			else if (node_name == "numProviders") {
				data.n_providers = i->as_int();
			}
			else if (node_name == "network") {
				data.network = _parseJsonNetworkObj(*i);
			}
			else if (node_name == "customers") {
				JSONNode::const_iterator j = i->begin();
				while(j != i->end()) {
					data.customers.push_back(_parseJsonCustomerObj(*j));
					j++;
				}
			}

			//increment the iterator
			++i;
		}
		return data;
	}

	dataContent loadFromJSONFile(const char * filename) {

		string json = _fileAsString(filename);

		JSONNode n = libjson::parse(json);

		return _parseJsonObject(n);
	}

	void toMoselDataFile(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);
		myfile << "n_Customers: " << data->n_customers;
		myfile << "\nn_Services: " << data->n_services;
		myfile << "\nn_Providers: " << data->n_providers;
		myfile << "\nn_Nodes: " << data->network.n_nodes;
		myfile << "\n!n_Paths: ! included below (not counted yet..)";
		myfile << "\n\nSymmetric: false";

		myfile << "\n\nR_Revenue: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			myfile << data->customers[i].revenue << " ";
		}
		myfile << "]";

		myfile << "\n\nS_ServiceForCustomer: [";
		int serviceNumber = 1;
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			myfile << "\n [ ";
			for (int j = 0; j < c->services.size(); ++j) {
				myfile << serviceNumber << " ";
				++serviceNumber;
			}
			myfile << "]";
		}
		myfile << "\n]";

		myfile << "\n\nK_Paths: [";
		int pathNumber = 1;
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				myfile << "\n";
				for (int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					myfile << " [";
					for (int l = 0; l < p->paths_up.size(); ++l) {
						myfile << pathNumber << " ";
						p->paths_up[l].pathNumber = pathNumber;
						++pathNumber;
					}
					myfile << "]";
				}
			}
		}
		myfile << "\n]";

		myfile << "\n\nK_PathsDown: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				myfile << "\n";
				for (int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					myfile << " [";
					for (int l = 0; l < p->paths_down.size(); ++l) {
						myfile << pathNumber << " ";
						p->paths_down[l].pathNumber = pathNumber;
						++pathNumber;
					}
					myfile << "]";
				}
			}
		}
		myfile << "\n]";

		myfile << "\n\nC_PathCost: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_up.size(); ++l) {
						myfile << p->paths_up[l].cost << " ";
					}
				}
			}
		}
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_down.size(); ++l) {
						myfile << p->paths_down[l].cost << " ";
					}
				}
			}
		}
		myfile << "]";

		myfile << "\n\nL_PathsUsingLink: [";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << "\n(" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (int j = 0; j < a->paths.size(); ++j) {
				myfile << a->paths[j]->pathNumber << " ";
			}
			myfile << "]";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsage: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_up.size(); ++l) {
						myfile << p->paths_up[l].bandwidth_usage << " ";
					}
				}
			}
		}
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_down.size(); ++l) {
						myfile << p->paths_down[l].bandwidth_usage << " ";
					}
				}
			}
		}
		myfile << "]";

		myfile << "\n\nJ_PathLatency: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_up.size(); ++l) {
						myfile << p->paths_up[l].latency << " ";
					}
				}
			}
		}
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths_down.size(); ++l) {
						myfile << p->paths_down[l].latency << " ";
					}
				}
			}
		}
		myfile << "]";

		myfile << "\n\nG_LatencyReq: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				myfile << data->customers[i].services[j].latency_req << " ";
			}
		}
		myfile << "]";

		myfile << "\n\nF_BandwidthCap: [";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode + 1 << ") " << a->bandwidth_cap;
		}
		myfile << "]";

		myfile << "\n\nn_Paths: " << pathNumber-1;

		myfile.close();
		
		return;
	}
}