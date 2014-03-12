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
							placement.globalProviderIndex = p->as_int();
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

	int _assignGlobalPathNumbers(dataContent * data) {
		int pathNumber = 0;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				for (unsigned int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						++pathNumber;
						p->paths[l].pathNumber = pathNumber;
					}
				}
			}
		}
		return pathNumber;
	}

	void _addCommonMoselData(dataContent * data, ofstream * file) {

		*file << "!!!!!!!!!!!!!!! COMMON DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";

		*file << "\nn_Customers: " << data->n_customers;
		*file << "\nn_Services: " << data->n_services;
		*file << "\nn_Providers: " << data->n_providers;
		*file << "\nn_Nodes: " << data->network.n_nodes;
		*file << "\n\nSymmetric: false";

		*file << "\n\nR_Revenue: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			*file << data->customers[i].revenue << " ";
		}
		*file << "]";

		*file << "\n\nS_ServiceForCustomer: [";
		int globalServiceNumber = 0;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			*file << "\n [ ";
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				++globalServiceNumber;
				*file << globalServiceNumber << " ";
			}
			*file << "]";
		}
		*file << "\n]";

		*file << "\n\nY_AvailabilityReq: [\n";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				*file << c->services[j].availability_req << " ";
			}
		}
		*file << "\n]";


		*file << "\n\nG_LatencyReq: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				*file << data->customers[i].services[j].latency_req << " ";
			}
		}
		*file << "]";

		*file << "\n\nF_BandwidthCap: [";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc * a = &data->network.arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") " << a->bandwidth_cap;
		}
		*file << "]";

		*file << "\n\nC_BackupCost: [";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") " << a->bandwidth_price;
		}
		*file << "]";
	}

	void _addPathMoselData(dataContent * data, ofstream * file) {
		
		*file << "\n\n!!!!!!!!!!!!!!! GENERATED PATHS DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";

		int n_paths = _assignGlobalPathNumbers(data);
		*file << "\n\nn_Paths: " << n_paths;

		*file << "\n\nK_Paths: [";
		int globalServiceNumber = 0;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				++globalServiceNumber;
				for (unsigned int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					*file << "\n (" << globalServiceNumber << " " << p->globalProviderIndex + 1 << ") [";
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l].pathNumber << " ";
					}
					*file << "]";
				}
				
			}
		}
		*file << "\n]";

		*file << "\n\nL_PathsUsingLink: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") [";
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				*file << (*j)->pathNumber << " ";
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				*file << (*j)->pathNumber << " ";
			}
			*file << "]\n";
		}
		*file << "]";

		*file << "\n\nU_PathBandwidthUsage: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
					(*j)->pathNumber << ") " << " " << (*j)->bandwidth_usage_up;
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
					(*j)->pathNumber << ") " << " " << (*j)->bandwidth_usage_down;
			}
			if (a->up_paths.size() > 0)
				*file << "\n";
		}
		*file << "]";

		*file << "\n\nD_PathAvailability: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l].exp_availability << " ";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nC_PathCost: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l].cost << " ";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nD_CombinationAvailability: [\n";
		for (list<pathCombo>::const_iterator i = data->pathCombos.begin(), end = data->pathCombos.end(); i != end; ++i) {
			*file << " (" << i->a->pathNumber << " " << i->b->pathNumber << ") " << i->exp_b_given_a;
		}
		*file << "\n]";

		*file << "\n\nQ_Overlap: [\n";
		for (list<pathOverlap>::const_iterator i = data->pathOverlaps.begin(), end = data->pathOverlaps.end(); i != end; ++i) {
			*file << " (" << i->a->pathNumber << " " << i->b->pathNumber << ") 1";
		}
		*file << "\n]";

	}

	int _assignGlobalMappingNumbers(dataContent * data) {
		int mappingNumber = 0;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				for (unsigned int k = 0; k < s->possible_mappings.size(); ++k) {
					mapping * m = &s->possible_mappings[k];
					++mappingNumber;
					m->mappingNumber = mappingNumber;
				}
			}
		}
		return mappingNumber;
	}

	void _addMappingMoselData(dataContent * data, ofstream * file) {

		*file << "\n\n!!!!!!!!!!!!!!! GENERATED MAPPINGS DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";

		*file << "\n\nn_Mappings: " << _assignGlobalMappingNumbers(data);
		
		*file << "\n\nK_Mappings: [";
		int serviceNumber = 1;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				*file << "\n (" << serviceNumber << ")";
				*file << " [";
				for (unsigned int l = 0; l < s->possible_mappings.size(); ++l) {
					*file << s->possible_mappings[l].mappingNumber << " ";
				}
				*file << "]";
				++serviceNumber;
			}
		}
		*file << "\n]";

		*file << "\n\nP_MappingsPerPath: [\n";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						returnPath * path = &p->paths[l];
						*file << "(" << p->paths[l].pathNumber << ") [";
						for (list<mapping*>::const_iterator m = path->primary_mappings.begin(), mend = path->primary_mappings.end(); m != mend; ++m) {
							*file << (*m)->mappingNumber << " ";
						}
						*file << "]\n";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nB_MappingsPerBackupPath: [\n";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						returnPath * path = &p->paths[l];
						*file << "(" << p->paths[l].pathNumber << ") [";
						for (list<mapping*>::const_iterator m = path->backup_mappings.begin(), mend = path->backup_mappings.end(); m != mend; ++m) {
							*file << (*m)->mappingNumber << " ";
						}
						*file << "]\n";
					}
				}
			}
		}
		*file << "]";
	}

	void toMoselDataFile(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);

		_addCommonMoselData(data, &myfile);
		_addPathMoselData(data, &myfile);

		myfile.close();
		
		return;
	}

	void toMoselDataFileV2(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);

		_addCommonMoselData(data, &myfile);

		/*
			Following data uses the term path for mappings.. needed to support mosel model v5
		*/
		myfile << "\n\n!!!!!!!!!!!!!!! LEGACY MAPPING DATA (as paths) !!!!!!!!!!!!!!!!!!!!!!!!!!";

		myfile << "\n\nn_Paths: " << _assignGlobalMappingNumbers(data);

		myfile << "\n\nK_Paths: [";
		int serviceNumber = 1;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				myfile << "\n (" << serviceNumber << ")";
				myfile << " [";
				for (unsigned int l = 0; l < s->possible_mappings.size(); ++l) {
					myfile << s->possible_mappings[l].mappingNumber << " ";
				}
				myfile << "]";
				++serviceNumber;
			}
		}
		myfile << "\n]";

		myfile << "\n\nL_PathsUsingLink: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->primary_mappings.begin(), kend = (*j)->primary_mappings.end(); k != kend; ++k) {
					myfile << (*k)->mappingNumber << " ";
				}
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->primary_mappings.begin(), kend = (*j)->primary_mappings.end(); k != kend; ++k) {
					myfile << (*k)->mappingNumber << " ";
				}
			}
			myfile << "]\n";
		}
		myfile << "]";

		myfile << "\n\nL_PathsUsingLinkBackup: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			myfile << " (" << a->startNode +1 << " " << a->endNode +1 << ") [";
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->backup_mappings.begin(), kend = (*j)->backup_mappings.end(); k != kend; ++k) {
					myfile << (*k)->mappingNumber << " ";
				}
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->backup_mappings.begin(), kend = (*j)->backup_mappings.end(); k != kend; ++k) {
					myfile << (*k)->mappingNumber << " ";
				}
			}
			myfile << "]\n";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsage: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->primary_mappings.begin(), kend = (*j)->primary_mappings.end(); k != kend; ++k) {
					myfile << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
						(*k)->mappingNumber << ") " << (*j)->bandwidth_usage_up;
				}
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->primary_mappings.begin(), kend = (*j)->primary_mappings.end(); k != kend; ++k) {
					myfile << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
						(*k)->mappingNumber << ") " << (*j)->bandwidth_usage_down;
				}
			}
			if(a->up_paths.size() > 0)
				myfile << "\n";
		}
		myfile << "]";

		myfile << "\n\nU_PathBandwidthUsageBackup: [\n";
		for (unsigned int i = 0; i < data->network.arcs.size(); ++i) {
			arc* a = &data->network.arcs[i];
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->backup_mappings.begin(), kend = (*j)->backup_mappings.end(); k != kend; ++k) {
					myfile << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
						(*k)->mappingNumber << ") " << (*j)->bandwidth_usage_up;
				}
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				for (list<mapping*>::const_iterator k = (*j)->backup_mappings.begin(), kend = (*j)->backup_mappings.end(); k != kend; ++k) {
					myfile << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
						(*k)->mappingNumber << ") " << (*j)->bandwidth_usage_down;
				}
			}
			if(a->up_paths.size() > 0)
				myfile << "\n";
		}
		myfile << "]";

		myfile << "\n\nC_PathCost: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				for (unsigned int l = 0; l < s->possible_mappings.size(); ++l) {
					mapping * m = &s->possible_mappings[l];
					myfile << m->primary->cost << " ";
				}
			}
		}
		myfile << "]";

		myfile.close();
		
		return;
	}

	
	void toMoselDataFileV3(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);

		_addCommonMoselData(data, &myfile);
		_addPathMoselData(data, &myfile);
		_addMappingMoselData(data, &myfile);

		myfile.close();

		return;
	}
}