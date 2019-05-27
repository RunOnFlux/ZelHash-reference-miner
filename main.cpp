// ZelHash OpenCL Miner
// Main Function
// Copyright 2019 Wilke Trei

#include "zelStratum.h"
#include "clHost.h"

inline vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while(getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    return split(s, delim, elems);
}

uint32_t cmdParser(vector<string> args, string &host, string &port, string &user, string &pass, bool &debug, bool &cpuMine, vector<int32_t> &devices) {
	bool hostSet = false;
	bool apiSet = false;

	pass = "x";
	
	for (uint32_t i=0; i<args.size(); i++) {
		if (args[i][0] == '-') {
			if ((args[i].compare("-h")  == 0) || (args[i].compare("--help")  == 0)) {
				return 0x4;
			}

			if (args[i].compare("--server")  == 0) {
				if (i+1 < args.size()) {
					vector<string> tmp = split(args[i+1], ':');
					if (tmp.size() == 2) {
						host = tmp[0];
						port = tmp[1];
						hostSet = true;	
						i++;
						continue;
					}
				}
			}

			if (args[i].compare("--user")  == 0) {
				if (i+1 < args.size()) {
					user = args[i+1];
					apiSet = true;
					i++;
					continue;
				}
			}

			if (args[i].compare("--pass")  == 0) {
				if (i+1 < args.size()) {
					pass = args[i+1];
					i++;
					continue;
				}
			}

			if (args[i].compare("--devices")  == 0) {
				if (i+1 < args.size()) {
					vector<string> tmp = split(args[i+1], ',');
					for (int j=0; j<tmp.size(); j++) {
						devices.push_back(stoi(tmp[j]));
					}
					continue;
				}
			}

			if (args[i].compare("--enable-cpu")  == 0) {
				cpuMine = true;
			}

			if (args[i].compare("--debug")  == 0) {
				debug = true;
			}
	
			if (args[i].compare("--version")  == 0) {
				cout << "1.0.0 for ZelCash (Jun 15th 2019)" << endl;
				exit(0);
			}
		}
	}

	uint32_t result = 0;
	if (!hostSet) result += 1;
	if (!apiSet) result += 2;

	if (devices.size() == 0) devices.assign(1,-1);
	sort(devices.begin(), devices.end());
	
	return result;
}

int main(int argc, char* argv[]) {

	

	vector<string> cmdLineArgs(argv, argv+argc);
	string host;
	string port;
	string user;
	string pass;

	bool debug = false;
	bool cpuMine = false;
	bool useTLS = true;
	vector<int32_t> devices;

	uint32_t parsing = cmdParser(cmdLineArgs, host, port, user, pass, debug, cpuMine, devices);

	cout << "-====================================-" << endl;
	cout << "          ZelHash OpenCL miner        " << endl;
	cout << "         v1.0.0, May 27th 2019        " << endl;
	cout << "-====================================-" << endl;

	if (parsing != 0) {
		if (parsing & 0x1) {
			cout << "Error: Parameter --server missing" << endl;
		}

		if (parsing & 0x2) {
			cout << "Error: Parameter --user missing" << endl;
		}

		cout << "Parameters: " << endl;
		cout << " --help / -h 			Showing this message" << endl;
		cout << " --server <server>:<port>	The stratum server and port to connect to (required)" << endl;
		cout << " --user <name>			The wallet address or pool user name  (required)" << endl;
		cout << " --pass <password>		A password for pool login if required (optional)" << endl;
		cout << " --devices <numbers>		A comma seperated list of devices that should be used for mining (default: all in system)" << endl; 
		cout << " --enable-cpu			Enable mining on OpenCL CPU devices" << endl;
		cout << " --version			Prints the version number" << endl;
		exit(0);
	}

	zelMiner::zelStratum myStratum(host, port, user, pass, debug);

	zelMiner::clHost myClHost;
	
	cout << endl;
	cout << "Setup OpenCL devices:" << endl;
	cout << "=====================" << endl;
	
	myClHost.setup(&myStratum, devices, cpuMine, false);

	cout << endl;
	cout << "Waiting for work from stratum:" << endl;
	cout << "==============================" << endl;

	myStratum.startWorking();

	while (!myStratum.hasWork()) {
		this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	cout << endl;
	cout << "Start mining:" << endl;
	cout << "=============" << endl;
	myClHost.startMining();
}

#if defined(_MSC_VER) && (_MSC_VER >= 1900)

FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void) { return _iob; }

#endif
