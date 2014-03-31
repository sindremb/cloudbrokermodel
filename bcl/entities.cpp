#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "entities.h"
#include "libjson/libjson.h"

namespace entities {

	using namespace std;

	int serviceCount = 0;
	int customerCount = 0;

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

	service _parseJsonServiceObj(JSONNODE *n) {
		service s;

		JSONNODE_ITERATOR i_itr = json_begin(n);

		while (i_itr != json_end(n)){
			JSONNODE *i = *i_itr;

			// get the node name and value as a string
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "bandwidthRequirementUp"){
				s.bandwidth_req_up = json_as_float(i);
			}
			else if (node_name == "bandwidthRequirementDown") {
				s.bandwidth_req_down = json_as_float(i);
			}
			else if (node_name == "latencyRequirement") {
				s.latency_req = json_as_float(i);
			}
			else if (node_name == "availabilityRequirement") {
				s.availability_req = json_as_float(i);
			}
			else if (node_name == "placements") {
				JSONNODE_ITERATOR j_itr = json_begin(i);
				while (j_itr != json_end(i)){
					JSONNODE *j = *j_itr;
					placement placement;

					JSONNODE_ITERATOR p_itr = json_begin(j);
					while(p_itr != json_end(j)) {
						JSONNODE *p = *p_itr;
						std::string node_name_p = json_name(p);
						// find out where to store the values
						if (node_name_p == "provider"){
							placement.globalProviderIndex = json_as_int(p);
						}
						else if (node_name_p == "price") {
							placement.price = json_as_float(p);
						}

						++p_itr;
					}
					
					s.possible_placements.push_back(placement);

					//increment the iterator
					++j_itr;
				}
			}
			//increment the iterator
			++i_itr;
		}
		return s;
	}


	customer _parseJsonCustomerObj(JSONNODE *n) {
		customer customer;

		JSONNODE_ITERATOR i_itr = json_begin(n);

		while (i_itr != json_end(n)){
			JSONNODE *i = *i_itr;

			// get the node name
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "revenue"){
				customer.revenue = json_as_int(i);
			}
			else if (node_name == "services"){
				JSONNODE_ITERATOR j_itr = json_begin(i);
				while (j_itr != json_end(i)){
					service s = _parseJsonServiceObj(*j_itr);
					s.globalServiceIndex = serviceCount;
					customer.services.push_back(s);
					++serviceCount;
					++j_itr;
				}
			}

			//increment the iterator
			++i_itr;
		}

		return customer;
	}

	void _parseJsonNetworkObj(JSONNODE *n,  networkStruct * net) {

		bool symmetric = false;

		JSONNODE_ITERATOR i_itr = json_begin(n);

		while (i_itr != json_end(n)){
			// recursively call ourselves to dig deeper into the tree
			//if (i->type() == JSON_ARRAY || i->type() == JSON_NODE){
				//ParseJSON(*i);
			//}
			JSONNODE *i = *i_itr;

			// get the node name and value as a string
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "numberOfNodes"){
				net->n_nodes = json_as_int(i);
			}
			else if (node_name == "isSymmetric") {
				symmetric = json_as_int(i);
			}
			else if (node_name == "arcs") {
				JSONNODE_ITERATOR j_itr = json_begin(i);
				// iterate over all arc json nodes
				while (j_itr != json_end(i)){
					// create arc object
					JSONNODE *j = *j_itr;
					arc arc;

					// iterate over all arc attributes
					JSONNODE_ITERATOR k_itr = json_begin(j);
					while(k_itr != json_end(j)) {
						JSONNODE *k = *k_itr;
						// get the node name and value as a string
						std::string node_name_k = json_name(k);

						// find out where to store the values
						if (node_name_k == "nodeTo"){
							arc.endNode = json_as_int(k);
						}
						else if (node_name_k == "nodeFrom") {
							arc.startNode = json_as_int(k);
						}
						else if (node_name_k == "latency") {
							arc.latency = json_as_float(k);
						}
						else if (node_name_k == "bandwidthCap") {
							arc.bandwidth_cap = json_as_float(k);
						}
						else if (node_name_k == "bandwidthPrice") {
							arc.bandwidth_price = json_as_float(k);
						}
						else if (node_name_k == "expectedAvailability") {
							arc.exp_availability = json_as_float(k);
						}

						//increment the iterator
						k_itr++;
					}

					net->arcs.push_back(arc);

					//increment the iterator
					++j_itr;
				}
			}

			//increment the iterator
			++i_itr;
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

	void _parseJsonObject(JSONNODE *n, dataContent *data) {

		JSONNODE_ITERATOR i_itr = json_begin(n);

		customerCount = 0;
		serviceCount = 0;

		while (i_itr != json_end(n)){
			JSONNODE *i = *i_itr;
			// get the node name
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "numCustomers"){
				data->n_customers = json_as_int(i);
			}
			else if (node_name == "numServices"){
				data->n_services = json_as_int(i);
			}
			else if (node_name == "numProviders") {
				data->n_providers = json_as_int(i);
			}
			else if (node_name == "network") {
				 _parseJsonNetworkObj(i, &data->network);
			}
			else if (node_name == "customers") {

				JSONNODE_ITERATOR j_itr = json_begin(i);
				while(j_itr != json_end(i)) {
					customer c = _parseJsonCustomerObj(*j_itr);
					c.node = customerCount;
					data->customers.push_back(c);
					j_itr++;
					++customerCount;
				}
			}

			//increment the iterator
			++i_itr;
		}
	}

	bool loadFromJSONFile(const char * filename, dataContent * data) {

		string json = _fileAsString(filename);

		if(json.length() == 0) {
			cout << "\nError: no content found in file: " << filename << "\n";
			return false;
		}

		try {
			const char* cjson;
			cjson = json.c_str();

			JSONNODE *n = json_parse(cjson);

			if(n == NULL) throw 1;

			_parseJsonObject(n, data);

			json_delete(n);
		} catch(...) {
			cout << "\nError: could not parse json from file: " << filename << "\n";
			return false;
		}
		return true;
	}

	int assignGlobalPathNumbers(dataContent * data) {
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
;
		*file << "\n\nn_Paths: " << data->n_paths;

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

	void _addMappingMoselData(dataContent * data, ofstream * file) {

		*file << "\n\n!!!!!!!!!!!!!!! GENERATED MAPPINGS DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";

		*file << "\n\nn_Mappings: " << data->n_mappings;
		
		*file << "\n\nM_MappingsPerService: [";
		int serviceNumber = 1;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = &c->services[j];
				*file << "\n (" << serviceNumber << ")";
				*file << " [";
				for (list<mapping>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
					*file << m_itr->globalMappingIndex+1 << " ";
				}
				*file << "]";
				++serviceNumber;
			}
		}
		*file << "\n]";

		*file << "\n\nM_PrimaryMappingsPerPath: [\n";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						returnPath * path = &p->paths[l];
						*file << "(" << p->paths[l].pathNumber << ") [";
						for (list<mapping*>::const_iterator m = path->primary_mappings.begin(), mend = path->primary_mappings.end(); m != mend; ++m) {
							*file << (*m)->globalMappingIndex +1 << " ";
						}
						*file << "]\n";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nM_BackupMappingsPerPath: [\n";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j].possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j].possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						returnPath * path = &p->paths[l];
						*file << "(" << p->paths[l].pathNumber << ") [";
						for (list<mapping*>::const_iterator m = path->backup_mappings.begin(), mend = path->backup_mappings.end(); m != mend; ++m) {
							*file << (*m)->globalMappingIndex +1 << " ";
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
		_addMappingMoselData(data, &myfile);

		myfile.close();

		return;
	}
}
