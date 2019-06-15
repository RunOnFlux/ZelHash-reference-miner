// ZelHash OpenCL Miner
// Stratum interface class
// Copyright 2019 Wilke Trei


#include "zelStratum.h"
#include "crypto/sha256.c"
#include "crypto/blake2b.h"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe32(x) OSSwapHostToBigInt32(x)
#endif

namespace zelMiner {

// This one ensures that the calling thread can work on immediately
void zelStratum::queueDataSend(string data) {
	io_service.post(boost::bind(&zelStratum::syncSend,this, data)); 
}

// Function to add a string into the socket write queue
void zelStratum::syncSend(string data) {
	writeRequests.push_back(data);
	activateWrite();
}


// Got granted we can write to our connection, lets do so	
void zelStratum::activateWrite() {
	if (!activeWrite && writeRequests.size() > 0) {
		activeWrite = true;

		string json = writeRequests.front();
		writeRequests.pop_front();

		std::ostream os(&requestBuffer);
		os << json;
		if (debug) cout << "Write to connection: " << json;

		boost::asio::async_write(*socket, requestBuffer, boost::bind(&zelStratum::writeHandler,this, boost::asio::placeholders::error)); 		
	}
}
	

// Once written check if there is more to write
void zelStratum::writeHandler(const boost::system::error_code& err) {
	activeWrite = false;
	activateWrite(); 
	if (err) {
		if (debug) cout << "Write to stratum failed: " << err.message() << endl;
	} 
}


// Called by main() function, starts the stratum client thread
void zelStratum::startWorking(){
	t_start = time(NULL);
	std::thread (&zelStratum::connect,this).detach();
}

// This function will be used to establish a connection to the API server
void zelStratum::connect() {	
	while (true) {
		tcp::resolver::query q(host, port); 

		cout << "Connecting to " << host << ":" << port << endl;
		try {
	    		tcp::resolver::iterator endpoint_iterator = res.resolve(q);
			tcp::endpoint endpoint = *endpoint_iterator;
			socket.reset(new tcp::socket(io_service));

			socket->async_connect(endpoint,
			boost::bind(&zelStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));	

			io_service.run();
		} catch (std::exception const& _e) {
			 cout << "Stratum error: " <<  _e.what() << endl;
		}

		workId = "-1";
		io_service.reset();
		socket->close();

		cout << "Lost connection to ZEL stratum server" << endl;
		cout << "Trying to connect in 5 seconds"<< endl;

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}		
}


// Once the physical connection is there start a TLS handshake
void zelStratum::handleConnect(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
	if (!err) {
	cout << "Connected to pool." << endl;

      	// The connection was successful. Listen to incomming messages
	boost::asio::async_read_until(*socket, responseBuffer, "\n",
	boost::bind(&zelStratum::readStratum, this, boost::asio::placeholders::error));

	// Send auth here !!!!

	// The connection was successful. Send the subscribe request
	std::stringstream json;
	json << "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\""
 	 << "zelHash reference 1.0.0" << "\",\"" << user << "\",\"" << port << "\", null]}\n";
	queueDataSend(json.str());	
	
	
    	} else if (err != boost::asio::error::operation_aborted) {
		if (endpoint_iterator != tcp::resolver::iterator()) {
			// The endpoint did not work, but we can try the next one
			tcp::endpoint endpoint = *endpoint_iterator;

			socket->async_connect(endpoint,
			boost::bind(&zelStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));
		} 
	} 	
}



// Simple helper function that casts a hex string into byte array
vector<uint8_t> parseHex (string input) {
	vector<uint8_t> result ;
	result.reserve(input.length() / 2);
	for (uint32_t i = 0; i < input.length(); i += 2){
		uint32_t byte;
		std::istringstream hex_byte(input.substr(i, 2));
		hex_byte >> std::hex >> byte;
		result.push_back(static_cast<unsigned char>(byte));
	}
	return result;
}

// An other helper function that computes the blake2b output of the first 128 byte of the block header
void zelStratum::preComputeBlake() {
	blake2bInstance blakeInst;
	blakeInst.init(64,125,4, "ZelProof");

	vector<uint8_t> tmpHeader;
	tmpHeader.insert(tmpHeader.end(), blockHeader.begin(), blockHeader.end());
	tmpHeader.insert(tmpHeader.end(), poolNonce.begin(), poolNonce.end());

	while (tmpHeader.size() < 128) tmpHeader.push_back((uint8_t) 0);

	uint8_t* msg = tmpHeader.data();
	blakeInst.update(msg,128,0);
	serverWork.assign(64,0);
	blakeInst.ret_state((uint64_t*) serverWork.data());	
}


// Main stratum read function, will be called on every received data
void zelStratum::readStratum(const boost::system::error_code& err) {
	if (!err) {
		// We just read something without problem.
		std::istream is(&responseBuffer);
		std::string response;
		getline(is, response);

		if (debug) cout << "Incomming Stratum: " << response << endl;

		// Parse the input to a property tree
		pt::iptree jsonTree;
		try {
			istringstream jsonStream(response);
			pt::read_json(jsonStream,jsonTree);

			int id = jsonTree.get<int>("id", -1);
			// If id >= 0 it is a reply to a send message

			// Reply to subscribe
			if (id == 1) {
				int exists = jsonTree.count("result");

				if (exists > 0) {
					// Read the pool part of nonce
					string nonceStr = element_at<string>(jsonTree, "result", 1);
					poolNonce = parseHex(nonceStr);

					cout << "Miner subscribed to pool, sending authorization request" << endl;

					string json = "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\""
		       			+ user + "\",\"" + pass + "\"]}\n";
					queueDataSend(json);	
				}	
			}


			// Reply to authorize
			if (id == 2) {
				int exists = jsonTree.count("result");
				bool isAuthorized = false;
				if (exists > 0) {
					isAuthorized = jsonTree.get<bool>("result");
				}

			       	if (!isAuthorized) {
					cout << "Fatal Error: Miner not authorized, Closing Miner" << endl;
					exit(0);
				} else {
					cout << "Worker authorized: " << user << endl;
				}
			}

			// Reply to a submitted share
			if (id == 4) {
				int exists = jsonTree.count("result");
				if (exists > 0) {
					bool accepted_share = jsonTree.get<bool>("result", false);
					
					if (accepted_share) {
						cout << "Share accepted" << endl;
						sharesAcc++;
					} else {
						cout << "Share rejected" << endl;
						sharesRej++;
					}
				}
			}

			
			string method = jsonTree.get<string>("method", "null");
			// If method is set, it is a push method from the stratum server

			// We got a new target
			if (method.compare("mining.set_target") == 0) {
				updateMutex.lock();
				target = parseHex(element_at<string>(jsonTree, "params", 0));
				updateMutex.unlock();

				cout << "Target received: " << element_at<string>(jsonTree, "params", 0).substr(0,16) << endl; 
			}

			// We got a new block header / work
			if (method.compare("mining.notify") == 0) {
				
				std::stringstream ssHeader;
				ssHeader << element_at<string>(jsonTree, "params", 1) << element_at<string>(jsonTree, "params", 2) << element_at<string>(jsonTree, "params", 3)
		         		 << element_at<string>(jsonTree, "params", 4) << element_at<string>(jsonTree, "params", 5) << element_at<string>(jsonTree, "params", 6);

				blockHeader = parseHex(ssHeader.str());
				
				updateMutex.lock();
				workId = element_at<string>(jsonTree, "params", 0);
				timeStr = element_at<string>(jsonTree, "params", 5); 
				preComputeBlake();
				updateMutex.unlock();

				cout << "New job received with id " << element_at<string>(jsonTree, "params", 0) << endl;

				t_current = time(NULL);
				cout << "Solutions (A/R): " << sharesAcc << " / " << sharesRej << " Uptime: " << (int)(t_current-t_start) << " sec" << endl; 
			}

			

		} catch(const pt::ptree_error &e) {
			cout << "Json parse error: " << e.what() << endl; 
		}

		// Prepare to continue reading
		boost::asio::async_read_until(*socket, responseBuffer, "\n",
        	boost::bind(&zelStratum::readStratum, this, boost::asio::placeholders::error));
	}
}


// Checking if we have valid work, else the GPUs will pause
bool zelStratum::hasWork() {
	return (workId.compare("-1") != 0);
}


// function the clHost class uses to fetch new work
void zelStratum::getWork(WorkDescription& wd, uint8_t* dataOut) {

	// nonce is atomic, so every time we call this will get a nonce increased by one
	uint32_t cliNonce = nonce.fetch_add(1);
	wd.nonce = cliNonce;  
	
	updateMutex.lock();

	wd.workId = workId;
	memcpy(dataOut, serverWork.data(), 64);

	updateMutex.unlock();
}


void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad) {
	assert(bit_len >= 8);
	assert(8*sizeof(uint32_t) >= bit_len);

	size_t in_width { (bit_len+7)/8 + byte_pad };
	assert(out_len == (bit_len*in_len/in_width + 7)/8);

	uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

	// The acc_bits least-significant bits of acc_value represent a bit sequence
	// in big-endian order.
	size_t acc_bits = 0;
	uint32_t acc_value = 0;

	size_t j = 0;
	for (size_t i = 0; i < out_len; i++) {
		// When we have fewer than 8 bits left in the accumulator, read the next
		// input element.
		if (acc_bits < 8) {
			if (j < in_len) {
				acc_value = acc_value << bit_len;
				for (size_t x = byte_pad; x < in_width; x++) {
					acc_value = acc_value | (
					(
					// Apply bit_len_mask across byte boundaries
					in[j + x] & ((bit_len_mask >> (8 * (in_width - x - 1))) & 0xFF)
					) << (8 * (in_width - x - 1))); // Big-endian
				}
				j += in_width;
				acc_bits += bit_len;
			}
			else {
				acc_value <<= 8 - acc_bits;
				acc_bits += 8 - acc_bits;;
			}
		}

		acc_bits -= 8;
		out[i] = (acc_value >> acc_bits) & 0xFF;
	}
}

#ifdef WIN32

inline uint32_t htobe32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >> 8) |
        ((x & 0x0000ff00U) << 8) | ((x & 0x000000ffU) << 24));
}


#endif // WIN32

// Big-endian so that lexicographic array comparison is equivalent to integer comparison
void EhIndexToArray(const uint32_t i, unsigned char* array) {
	static_assert(sizeof(uint32_t) == 4, "");
	uint32_t bei = htobe32(i);
	memcpy(array, &bei, sizeof(uint32_t));
}


// Helper function that compresses the solution from 32 unsigned integers (128 bytes) to 104 bytes
std::vector<unsigned char> GetMinimalFromIndices(std::vector<uint32_t> indices, size_t cBitLen) {
	assert(((cBitLen+1)+7)/8 <= sizeof(uint32_t));
	size_t lenIndices { indices.size()*sizeof(uint32_t) };
	size_t minLen { (cBitLen+1)*lenIndices/(8*sizeof(uint32_t)) };
	size_t bytePad { sizeof(uint32_t) - ((cBitLen+1)+7)/8 };
	std::vector<unsigned char> array(lenIndices);
	for (size_t i = 0; i < indices.size(); i++) {
		EhIndexToArray(indices[i], array.data()+(i*sizeof(uint32_t)));
	}
	std::vector<unsigned char> ret(minLen);
	CompressArray(array.data(), lenIndices, ret.data(), minLen, cBitLen+1, bytePad);
	return ret;
}

// Helper function that does target comparison
inline int32_t cmp_target_256(vector<uint8_t> a, vector<uint8_t> b) {
	
    	for (int i = a.size() - 1; i >= 0; i--)
		if (a[i] != b[b.size() - i - 1])
	    	return (int32_t)a[i] - b[b.size() - i -1 ];
   	return 0;
}

bool zelStratum::testSolution(const vector<uint32_t>& indices, WorkDescription& wd, vector<uint8_t> &outNonce, vector<uint8_t> &outSolution) {

	// Check if it is a solution to the current job
	if (workId.compare(wd.workId) != 0) return false;

	// get the compressed representation of the solution and check against target
	std::vector<uint8_t> compressed;
	compressed = GetMinimalFromIndices(indices,25);

	vector<uint8_t> fullHeader;

	// Insert block header
	fullHeader.assign(140,0);
	for (int c=0; c<blockHeader.size(); c++) {
		fullHeader[c] = blockHeader[c];
	}

	// Pool Nonce
	for (int c=0; c<poolNonce.size(); c++) {
		fullHeader[blockHeader.size()+c] = poolNonce[c];
	}

	// Solution Nonce
	uint32_t* noncePoint = (uint32_t*) &fullHeader.data()[fullHeader.size()-4];
	*noncePoint = wd.nonce;

	// This indicates that the following solution should have 52 bytes
	fullHeader.push_back((uint8_t) 52);

	// Add the solution
	fullHeader.insert(fullHeader.end(), compressed.begin(), compressed.end());		

	// Double sha-256
	vector<uint8_t> hash0;
	vector<uint8_t> hash1;

	hash0.assign(32,0);
	hash1.assign(32,0);

	Sha256_Onestep(fullHeader.data(), fullHeader.size(), hash0.data());
	Sha256_Onestep(hash0.data(), hash0.size(), hash1.data());

	if (cmp_target_256(hash1, target) < 0) {
		// The solution is below target
		
		// Copy out the length encoding and the solution
		outSolution.push_back((uint8_t) 52);
		outSolution.insert(outSolution.end(), compressed.begin(), compressed.end());	

		// Copy out the client nonce
		int freeNonceSz = 32 - poolNonce.size();
		outNonce.assign(freeNonceSz, 0);
		for (int c=0; c<freeNonceSz; c++) {
			outNonce[c] = fullHeader[blockHeader.size() + poolNonce.size() + c];
		}

		return true;
	} 

	return false;
}

void zelStratum::submitSolution(vector<uint8_t> cliNonce, vector<uint8_t> comprSol) {

	
	stringstream solutionStr;
	for (int c=0; c<comprSol.size(); c++) {
		solutionStr << std::setfill('0') << std::setw(2) << std::hex << (unsigned) comprSol[c];
	}

	stringstream nonceStr;
	for (int c=0; c<cliNonce.size(); c++) {
		nonceStr << std::setfill('0') << std::setw(2) << std::hex << (unsigned) cliNonce[c];
	}

	string json = "{\"id\":4 ,\"method\":\"mining.submit\",\"params\":[\"" + user + "\",\"" + workId + "\",\"" 
			+ timeStr + "\",\"" + nonceStr.str() + "\",\"" + solutionStr.str() + "\"]}\n";

	queueDataSend(json);	

	cout << "Submitting solution to job " << workId << endl;
}


// Will be called by clHost class for check & submit
void zelStratum::handleSolution(WorkDescription& wd, vector<uint32_t> &indices) {

	vector<uint8_t> solution;
	vector<uint8_t> clientNonce;

	if (testSolution(indices, wd, clientNonce,solution)) {
		submitSolution(clientNonce, solution);
	}
}


zelStratum::zelStratum(string hostIn, string portIn, string userIn, string passIn, bool debugIn) : res(io_service) {

	host = hostIn;
	port = portIn;
	user = userIn;
	pass = passIn;
	debug = debugIn;

	// Assign the work field and nonce
	serverWork.assign(64,(uint8_t) 0);
	target.assign(32,(uint8_t) 0);

	random_device rd;
	default_random_engine generator(rd());
	uniform_int_distribution<uint32_t> distribution(0,0xFFFFFFFF);

	// We pick a random start nonce
	nonce = distribution(generator);

	// No work in the beginning
	workId = "-1";
}

} // End namespace

