// ZelHash OpenCL Miner
// Stratum interface class
// Copyright 2019 Wilke Trei

#include <iostream>
#include <thread>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <random>

#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
namespace pt = boost::property_tree;

namespace zelMiner {

#ifndef zelMiner_H 
#define zelMiner_H 

class zelStratum {
	public:
	zelStratum(string, string, string, string, bool);
	void startWorking();

	struct WorkDescription
	{
		string workId;
		uint32_t nonce;
	};

	bool hasWork();
	void getWork(WorkDescription&, uint8_t*);

	void handleSolution(WorkDescription&, std::vector<uint32_t>&);


	private:

	// Definitions belonging to the physical connection
	boost::asio::io_service io_service;
	boost::scoped_ptr< tcp::socket > socket;
	tcp::resolver res;
	boost::asio::streambuf requestBuffer;
	boost::asio::streambuf responseBuffer;

	// User Data
	string host;
	string port;
	string user;
	string pass;
	bool debug = true;

	// Storage for received work
	string workId;
	string timeStr;
	std::vector<uint8_t> blockHeader;
	std::vector<uint8_t> serverWork;
	std::atomic<uint32_t> nonce;
	vector<uint8_t> target;
	std::vector<uint8_t> poolNonce;

	// Stat
	uint64_t sharesAcc = 0;
	uint64_t sharesRej = 0;
	time_t t_start, t_current;

	//Stratum sending subsystem
	bool activeWrite = false;
	void queueDataSend(string);
	void syncSend(string);
	void activateWrite();
	void writeHandler(const boost::system::error_code&);	
	std::deque<string> writeRequests;

	// Stratum receiving subsystem
	void readStratum(const boost::system::error_code&);
	boost::mutex updateMutex;

	// Connection handling
	void connect();
	void handleConnect(const boost::system::error_code& err,  tcp::resolver::iterator);

	void preComputeBlake();

	// Solution Check & Submit
	bool testSolution(const vector<uint32_t>&, WorkDescription&, vector<uint8_t>&, vector<uint8_t>&);
	void submitSolution(vector<uint8_t>, vector<uint8_t>);	
};


template <typename T = std::string> 
T element_at(pt::iptree const& tree, std::string name, size_t n) {
    auto r = tree.get_child(name).equal_range("");

    for (; r.first != r.second && n; --n) ++r.first;

    if (n || r.first==r.second)
        throw std::range_error("index out of bounds");

    return r.first->second.get_value<T>();
}

#endif 

}

