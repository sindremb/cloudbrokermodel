#include <iostream>
#include <fstream>
#include <string>
#include <list>

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
		service s;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){

			// get the node name and value as a string
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "bandwidthRequirementUp"){
				s.bandwidth_req_up = i->as_float();
			}
			else if (node_name == "bandwidthRequirementDown") {
				s.bandwidth_req_down = i->as_float();
			}
			else if (node_name == "latencyRequirement") {
				s.latency_req = i->as_float();
			}
			else if (node_name == "availabilityRequirement") {
				s.availability_req = i->as_float();
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
							placement.provider_index = p->as_int();
						}
						else if (node_name_p == "price") {
							placement.price = p->as_float();
						}

						++p;
					}
					
					s.possible_placements.push_back(placement);

					//increment the iterator
					++j;
				}
			}
			//increment the iterator
			++i;
		}
		return s;
	}


	customer _parseJsonCustomerObj(const JSONNode & n) {
		customer customer;

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){
			// get the node name
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "revenue"){
				customer.revenue = i->as_float();
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

	void _parseJsonNetworkObj(const JSONNode & n,  network * net) {

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
				net->n_nodes = i->as_int();
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
							arc.latency = k->as_float();
						}
						else if (node_name_k == "bandwidthCap") {
							arc.bandwidth_cap = k->as_float();
						}
						else if (node_name_k == "bandwidthPrice") {
							arc.bandwidth_price = k->as_float();
						}
						else if (node_name_k == "expectedAvailability") {
							arc.exp_availability = k->as_float();
						}

						k++;
					}

					net->arcs.push_back(arc);

					//increment the iterator
					++j;
				}
			}

			//increment the iterator
			++i;
		}

		if(symmetric) {
			int originalSize = net->arcs.size();
			for(int a = 0; a < originalSize; ++a) {
				arc sym = net->arcs[a];
				sym.startNode = net->arcs[a].endNode;
				sym.endNode = net->arcs[a].startNode;
				net->arcs.push_back(sym);
			}
			for(int a = 0; a < originalSize; ++a) {
				net->arcs[a].return_arc = &net->arcs[a+originalSize];
				net->arcs[a+originalSize].return_arc = &net->arcs[a];
			}
		} else {
			int originalSize = net->arcs.size();
			for(int i = 0; i < originalSize; ++i) {
				for(int j = 0; j < originalSize; ++j) {
					if(net->arcs[i].startNode == net->arcs[j].endNode &&
						net->arcs[j].startNode == net->arcs[i].endNode)
						net->arcs[i].return_arc = &net->arcs[j];
						net->arcs[j].return_arc = &net->arcs[i];
				}
			}
		}
	}

	void _parseJsonObject(const JSONNode & n, dataContent * data) {

		JSONNode::const_iterator i = n.begin();

		while (i != n.end()){
			// get the node name
			std::string node_name = i->name();

			// find out where to store the values
			if (node_name == "numCustomers"){
				data->n_customers = i->as_int();
			}
			else if (node_name == "numServices"){
				data->n_services = i->as_int();
			}
			else if (node_name == "numProviders") {
				data->n_providers = i->as_int();
			}
			else if (node_name == "network") {
				 _parseJsonNetworkObj(*i, &data->network);
			}
			else if (node_name == "customers") {
				JSONNode::const_iterator j = i->begin();
				while(j != i->end()) {
					data->customers.push_back(_parseJsonCustomerObj(*j));
					j++;
				}
			}

			//increment the iterator
			++i;
		}
	}

	void loadFromJSONFile(const char * filename, dataContent * data) {

		string json = _fileAsString(filename);

		JSONNode n = libjson::parse(json);

		_parseJsonObject(n, data);
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

		myfile << "\n\nY_AvailabilityReq: [\n";
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				myfile << c->services[j].availability_req << " ";
			}
		}
		myfile << "\n]";

		myfile << "\n\nK_Paths: [";
		int pathNumber = 0;
		serviceNumber = 1;
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				for (int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					myfile << "\n (" << serviceNumber << " " << p->provider_index + 1 << ")";
					myfile << " [";
					for (int l = 0; l < p->paths.size(); ++l) {
						++pathNumber;
						myfile << pathNumber << " ";
						p->paths[l].pathNumber = pathNumber;
					}
					myfile << "]";
				}
				++serviceNumber;
			}
		}
		myfile << "\n]";

		myfile << "\n\nL_PathsUsingLink: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				myfile << (*j)->pathNumber << " ";
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				myfile << (*j)->pathNumber << " ";
			}
			myfile << "]\n";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsage: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					  (*j)->pathNumber << ") " << " " << (*j)->bandwidth_usage_up;
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					  (*j)->pathNumber << ") " << " " << (*j)->bandwidth_usage_down;
			}
			if(a->up_paths.size() > 0)
				myfile << "\n";
		}
		myfile << "]";

		
		myfile << "\n\nD_PathAvailability: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths.size(); ++l) {
						myfile << p->paths[l].exp_availability << " ";
					}
				}
			}
		}
		myfile << "]";

		myfile << "\n\nC_PathCost: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			for (int j = 0; j < data->customers[i].services.size(); ++j) {
				for (int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (int l = 0; l < p->paths.size(); ++l) {
						myfile << p->paths[l].cost << " ";
					}
				}
			}
		}
		myfile << "]";

		myfile << "\n\nD_CombinationAvailability: [\n";
		for (list<pathCombo>::const_iterator i = data->pathCombos.begin(), end = data->pathCombos.end(); i != end; ++i) {
			myfile << " ("  << i->a->pathNumber << " " << i->b->pathNumber << ") " << i->exp_b_given_a;
		}
		myfile << "\n]";

		myfile << "\n\nQ_Overlap: [\n";
		for (list<pathOverlap>::const_iterator i = data->pathOverlaps.begin(), end = data->pathOverlaps.end(); i != end; ++i) {
			myfile << " ("  << i->a->pathNumber << " " << i->b->pathNumber << ") 1";
		}
		myfile << "\n]";


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

		myfile << "\n\nC_BackupCost: [";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode + 1 << ") " << a->bandwidth_price;
		}
		myfile << "]";

		myfile << "\n\nn_Paths: " << pathNumber;

		myfile.close();
		
		return;
	}

	void toMoselDataFileV2(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);
		myfile << "n_Customers: " << data->n_customers;
		myfile << "\nn_Services: " << data->n_services;
		myfile << "\nn_Nodes: " << data->network.n_nodes;
		myfile << "\n!n_Routings: ! included below (not counted yet..)";
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
		int routingNumber = 0;
		serviceNumber = 1;
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				myfile << "\n (" << serviceNumber << ")";
				myfile << " [";
				for (int l = 0; l < s->possible_routings.size(); ++l) {
					++routingNumber;
					myfile << routingNumber << " ";
					s->possible_routings[l].routingNumber = routingNumber;
				}
				myfile << "]";
				++serviceNumber;
			}
		}
		myfile << "\n]";

		myfile << "\n\nL_PathsUsingLink: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (list<routing*>::const_iterator j = a->up_routings_primary.begin(), end = a->up_routings_primary.end(); j != end; ++j) {
				myfile << (*j)->routingNumber << " ";
			}
			for (list<routing*>::const_iterator j = a->down_routings_primary.begin(), end = a->down_routings_primary.end(); j != end; ++j) {
				myfile << (*j)->routingNumber << " ";
			}
			myfile << "]\n";
		}
		myfile << "]";

		myfile << "\n\nL_PathsUsingLinkBackup: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (list<routing*>::const_iterator j = a->up_routings_backup.begin(), end = a->up_routings_backup.end(); j != end; ++j) {
				myfile << (*j)->routingNumber << " ";
			}
			for (list<routing*>::const_iterator j = a->down_routings_backup.begin(), end = a->down_routings_backup.end(); j != end; ++j) {
				myfile << (*j)->routingNumber << " ";
			}
			myfile << "]\n";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsage: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<routing*>::const_iterator j = a->up_routings_primary.begin(), end = a->up_routings_primary.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					(*j)->routingNumber << ") " << (*j)->primary->bandwidth_usage_up;
			}
			for (list<routing*>::const_iterator j = a->down_routings_primary.begin(), end = a->down_routings_primary.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					(*j)->routingNumber << ") " << (*j)->primary->bandwidth_usage_down;
			}
			if(a->up_paths.size() > 0)
				myfile << "\n";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsageBackup: [\n";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<routing*>::const_iterator j = a->up_routings_backup.begin(), end = a->up_routings_backup.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					(*j)->routingNumber << ") " << (*j)->backup->bandwidth_usage_up;
			}
			for (list<routing*>::const_iterator j = a->down_routings_backup.begin(), end = a->down_routings_backup.end(); j != end; ++j) {
				myfile << " (" << a->startNode +1 << " " << a->endNode +1 << " " <<
					(*j)->routingNumber << ") " << (*j)->backup->bandwidth_usage_down;
			}
			if(a->up_paths.size() > 0)
				myfile << "\n";
		}
		myfile << "]";

		myfile << "\n\nC_PathCost: [";
		for (int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				for (int l = 0; l < s->possible_routings.size(); ++l) {
					routing * r = &s->possible_routings[l];
					myfile << r->primary->cost << " ";
				}
			}
		}
		myfile << "]";

		myfile << "\n\nF_BandwidthCap: [";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode + 1 << ") " << a->bandwidth_cap;
		}
		myfile << "]";

		myfile << "\n\nC_BackupCost: [";
		for (int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode + 1 << ") " << a->bandwidth_price;
		}
		myfile << "]";

		myfile << "\n\nn_Paths: " << routingNumber;

		myfile.close();
		
		return;
	}
}