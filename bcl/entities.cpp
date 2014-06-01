#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "entities.h"
#include "libjson/libjson.h"

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
					placement pl;

					JSONNODE_ITERATOR p_itr = json_begin(j);
					while(p_itr != json_end(j)) {
						JSONNODE *p = *p_itr;
						std::string node_name_p = json_name(p);
						// find out where to store the values
						if (node_name_p == "provider"){
							pl.globalProviderIndex = json_as_int(p);
						}
						else if (node_name_p == "price") {
							pl.price = json_as_float(p);
						}

						++p_itr;
					}
					
					s.possible_placements.push_back(pl);

					//increment the iterator
					++j_itr;
				}
			}
			//increment the iterator
			++i_itr;
		}
		return s;
	}


	customer _parseJsonCustomerObj(JSONNODE *n, dataContent * data) {
		customer c;

		JSONNODE_ITERATOR i_itr = json_begin(n);

		while (i_itr != json_end(n)){
			JSONNODE *i = *i_itr;

			// get the node name
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "revenue"){
				c.revenue = json_as_int(i);
			}
			else if (node_name == "services"){
				JSONNODE_ITERATOR j_itr = json_begin(i);
				while (j_itr != json_end(i)){
					data->services.push_back(_parseJsonServiceObj(*j_itr));
					service *s = &data->services.back();
					s->globalServiceIndex = data->services.size() -1;
					c.services.push_back(s);
					++j_itr;
				}
			}

			//increment the iterator
			++i_itr;
		}

		return c;
	}

	void _parseJsonNetworkObj(JSONNODE *n,  dataContent * data) {

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
				data->n_nodes = json_as_int(i);
			}
			else if (node_name == "isSymmetric") {
				symmetric = json_as_bool(i);
			}
			else if (node_name == "arcs") {
				JSONNODE_ITERATOR j_itr = json_begin(i);
				// iterate over all arc json nodes
				while (j_itr != json_end(i)){
					JSONNODE *j = *j_itr;

					// create arc object
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

					arc.globalArcIndex = data->arcs.size();

					data->arcs.push_back(arc);

					//increment the iterator
					++j_itr;
				}
			}

			//increment the iterator
			++i_itr;
		}

		if(symmetric) {
			// duplicate arc in each direction
			int originalSize = data->arcs.size();
			data->arcs.reserve(originalSize);
			for(int a = 0; a < originalSize; ++a) {
				arc sym = data->arcs[a];
				sym.startNode = data->arcs[a].endNode;
				sym.endNode = data->arcs[a].startNode;
				sym.globalArcIndex = originalSize + a;
				data->arcs.push_back(sym);
			}
			for(int a = 0; a < originalSize; ++a) {
				data->arcs[a].return_arc = &data->arcs[a+originalSize];
				data->arcs[a+originalSize].return_arc = &data->arcs[a];
			}
		} else {
			// find the return arc for each arc
			int originalSize = data->arcs.size();
			for(int i = 0; i < originalSize; ++i) {
				for(int j = 0; j < originalSize; ++j) {
					if(data->arcs[i].startNode == data->arcs[j].endNode &&
						data->arcs[j].startNode == data->arcs[i].endNode)
						data->arcs[i].return_arc = &data->arcs[j];
						data->arcs[j].return_arc = &data->arcs[i];
				}
			}
		}
	}

	void _parseJsonObject(JSONNODE *n, dataContent *data) {

		JSONNODE_ITERATOR i_itr = json_begin(n);

		while (i_itr != json_end(n)){
			JSONNODE *i = *i_itr;
			// get the node name
			std::string node_name = json_name(i);

			// find out where to store the values
			if (node_name == "numCustomers"){
				data->n_customers = json_as_int(i);
				data->customers.reserve(data->n_customers);
			}
			else if (node_name == "numServices"){
				data->n_services = json_as_int(i);
				data->services.reserve(data->n_services);
			}
			else if (node_name == "numProviders") {
				data->n_providers = json_as_int(i);
			}
			else if (node_name == "network") {
				 _parseJsonNetworkObj(i, data);
			}
			else if (node_name == "customers") {

				JSONNODE_ITERATOR j_itr = json_begin(i);
				while(j_itr != json_end(i)) {
					customer c = _parseJsonCustomerObj(*j_itr, data);
					c.globalCustomerIndex = data->customers.size();
					data->customers.push_back(c);
					j_itr++;
				}
			}

			//increment the iterator
			++i_itr;
		}

		data->n_arcs = data->arcs.size();
		data->n_mappings = 0;
		data->n_paths = 0;
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
		list<returnPath>::iterator k_itr = data->paths.begin();
		for (unsigned int pp = 0; pp < data->paths.size(); ++pp) {
			returnPath *k = &(*k_itr);
			k->globalPathIndex = pp;
			++k_itr;
		}
		return data->paths.size();
	}

	void _addCommonMoselData(dataContent * data, ofstream * file) {

		*file << "!!!!!!!!!!!!!!! COMMON DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";

		*file << "\nn_Customers: " << data->n_customers;
		*file << "\nn_Services: " << data->n_services;
		*file << "\nn_Providers: " << data->n_providers;
		*file << "\nn_Nodes: " << data->n_nodes;
		*file << "\n\nSymmetric: false";

		*file << "\n\nR_Revenue: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			*file << data->customers[i].revenue << " ";
		}
		*file << "]";

		*file << "\n\nS_ServicesForCustomer: [";
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
				*file << c->services[j]->availability_req << " ";
			}
		}
		*file << "\n]";


		*file << "\n\nG_LatencyReq: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				*file << data->customers[i].services[j]->latency_req << " ";
			}
		}
		*file << "]";

		*file << "\n\nF_BandwidthCap: [";
		for (unsigned int i = 0; i < data->arcs.size(); ++i) {
			arc * a = &data->arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") " << a->bandwidth_cap;
		}
		*file << "]";

		*file << "\n\nC_BackupCost: [";
		for (unsigned int i = 0; i < data->arcs.size(); ++i) {
			arc* a = &data->arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") " << a->bandwidth_price;
		}
		*file << "]";
	}

	void _addPathMoselData(dataContent * data, ofstream * file) {
		
		*file << "\n\n!!!!!!!!!!!!!!! GENERATED PATHS DATA !!!!!!!!!!!!!!!!!!!!!!!!!!";
;
		*file << "\n\nn_Paths: " << data->n_paths;

		*file << "\n\nK_PathsServiceProvider: [";
		int globalServiceNumber = 0;
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			customer * c = &data->customers[i];
			for (unsigned int j = 0; j < c->services.size(); ++j) {
				service * s = c->services[j];
				++globalServiceNumber;
				for (unsigned int k = 0; k < s->possible_placements.size(); ++k) {
					placement * p = &s->possible_placements[k];
					*file << "\n (" << globalServiceNumber << " " << p->globalProviderIndex + 1 << ") [";
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l]->globalPathIndex+1 << " ";
					}
					*file << "]";
				}
				
			}
		}
		*file << "\n]";

		*file << "\n\nL_PathsUsingArc: [\n";
		for (unsigned int i = 0; i < data->arcs.size(); ++i) {
			arc* a = &data->arcs[i];
			*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << ") [";
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				*file << (*j)->globalPathIndex+1 << " ";
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				*file << (*j)->globalPathIndex+1 << " ";
			}
			*file << "]\n";
		}
		*file << "]";

		*file << "\n\nU_PathBandwidthUsage: [\n";
		for (unsigned int i = 0; i < data->arcs.size(); ++i) {
			arc* a = &data->arcs[i];
			for (list<returnPath*>::const_iterator j = a->up_paths.begin(), end = a->up_paths.end(); j != end; ++j) {
				*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
					(*j)->globalPathIndex+1 << ") " << " " << (*j)->bandwidth_usage_up;
			}
			for (list<returnPath*>::const_iterator j = a->down_paths.begin(), end = a->down_paths.end(); j != end; ++j) {
				*file << " (" << a->startNode + 1 << " " << a->endNode + 1 << " " <<
					(*j)->globalPathIndex+1 << ") " << " " << (*j)->bandwidth_usage_down;
			}
			if (a->up_paths.size() > 0)
				*file << "\n";
		}
		*file << "]";

		*file << "\n\nD_PathAvailability: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j]->possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j]->possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l]->exp_availability << " ";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nC_PathCost: [";
		for (unsigned int i = 0; i < data->customers.size(); ++i) {
			for (unsigned int j = 0; j < data->customers[i].services.size(); ++j) {
				for (unsigned int k = 0; k < data->customers[i].services[j]->possible_placements.size(); ++k) {
					placement * p = &data->customers[i].services[j]->possible_placements[k];
					for (unsigned int l = 0; l < p->paths.size(); ++l) {
						*file << p->paths[l]->cost << " ";
					}
				}
			}
		}
		*file << "]";

		*file << "\n\nD_CombinationAvailability: [\n";
		for (list<pathCombo>::const_iterator i = data->pathCombos.begin(), end = data->pathCombos.end(); i != end; ++i) {
			*file << " (" << i->a->globalPathIndex+1 << " " << i->b->globalPathIndex+1<< ") " << i->prob_a_and_b;
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
				service * s = c->services[j];
				*file << "\n (" << serviceNumber << ")";
				*file << " [";
				for (list<mapping*>::iterator m_itr = s->mappings.begin(), m_end = s->mappings.end(); m_itr != m_end; ++m_itr) {
					*file << (*m_itr)->globalMappingIndex+1 << " ";
				}
				*file << "]";
				++serviceNumber;
			}
		}
		*file << "\n]";

		*file << "\n\nM_PrimaryMappingsPerPath: [\n";
		for (list<returnPath>::const_iterator k_itr = data->paths.begin(), k_end = data->paths.end(); k_itr != k_end; ++k_itr) {
			*file << "(" << k_itr->globalPathIndex+1 << ") [";
			for (list<mapping*>::const_iterator m = k_itr->primary_mappings.begin(), m_end = k_itr->primary_mappings.end(); m != m_end; ++m) {
				*file << (*m)->globalMappingIndex +1 << " ";
			}
			*file << "]\n";
		}
		*file << "]";

		*file << "\n\nM_BackupMappingsPerPath: [\n";
		for (list<returnPath>::const_iterator k_itr = data->paths.begin(), k_end = data->paths.end(); k_itr != k_end; ++k_itr) {
			*file << "(" << k_itr->globalPathIndex+1 << ") [";
			for (list<mapping*>::const_iterator m = k_itr->backup_mappings.begin(), mend = k_itr->backup_mappings.end(); m != mend; ++m) {
				*file << (*m)->globalMappingIndex +1 << " ";
			}
			*file << "]\n";
		}
		*file << "]";
	}

	void toMoselDataFile(const char * filename, dataContent * data) {

		ofstream myfile;
		myfile.open(filename);

		if(!myfile) {
			cerr << "could not open file <" << filename << "> for writing\n";
			return;
		}

		_addCommonMoselData(data, &myfile);
		_addPathMoselData(data, &myfile);
		_addMappingMoselData(data, &myfile);

		myfile.close();

		return;
	}

	/*
	 * Calculates the P(A)P(B|A) availability term only for paths *a and *b (ie. probability of a and b simultaneously)
	 */
	double prob_paths_a_and_b(returnPath * a, returnPath * b) {

		// P(A)
		double a_and_b = a->exp_availability;

		// find unique links in B for calculating P(B|A)
		vector<arc*> unique;

		// for all links in path b (represented by arcs going up)
		for (list<arc*>::const_iterator i = b->arcs_up.begin(), end = b->arcs_up.end(); i != end; ++i) {
			bool found = false;
			// for all links in path a (represented by arcs going up)
			for (list<arc*>::const_iterator j = a->arcs_up.begin(), end = a->arcs_up.end(); j != end; ++j) {
				// path b's up-arc matches a's up-arc or its down-arc (-> link is shared)
				if(*i == *j || *i == (*j)->return_arc) {
					found = true;
					break;
				}
			}
			if(!found) {
				unique.push_back(*i);
			}
		}

		// P(B|A)
		for(unsigned int i = 0; i < unique.size(); ++i) {
			a_and_b *= unique[i]->exp_availability;
		}

		return a_and_b;
	}
}
