#pragma once

#include <fstream>
#include <vector>
#include "json11.hpp"
#include "pthread.h"
#include <thread>
#include <algorithm>
#include <map>
#include <sstream>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>

#define	ALL_PROTOCOL 0
#define	ICMP 1
#define	TCP 6
#define	UDP 17
#define	ESP 50		//! Flag for Encapsulating Security Payload Header, RFC 2406
#define	AH 51		//! Flag for authentication header, RFC 2402
#define	ICMPv6 58

using namespace std;
using namespace json11;

/// <summary>
/// Enumeration of connection type to filter.
/// </summary>
typedef enum
{
    /// <summary>
    /// Filter IPv4 connections.
    /// </summary>
            Ipv4 = 4,
    /// <summary>
    /// Filter IPv6 connections.
    /// </summary>
            Ipv6 = 6,
    /// <summary>
    /// Filter all network connections.
    /// </summary>
            ALL_NETWORK = 0
} NetworkType;

using namespace std;

struct Endpoint
{
    string Hostname;
    int Port = 0;
    int Protocol = TCP;
    NetworkType Network = Ipv4;
};

typedef enum
{
    Bandwidth,
    Disconnect,
    Latency,
    Loss
} EmulationType;

static map<string, EmulationType> EmStringValues;
map<string, EmulationType> MapStringToEmulationType =
        {
                { "Bandwidth", Bandwidth },
                { "Disconnect", Disconnect },
                { "Latency", Latency },
                { "Loss", Loss }
        };

struct NetworkEmulationProfile
{
    vector<Endpoint> Endpoints;
    int Duration = 0;
    int RunOrder = 0;
};

inline wstring Hostname2Ips(const wstring &hostname, const wstring &port)
{

    return L"";
}

inline string hostname2ips(string &hostname)
{
    struct addrinfo * _addrinfo;
    struct addrinfo * _res;
    char _address[INET6_ADDRSTRLEN];
    int err = 0;
    string ips;

    err = getaddrinfo(hostname.c_str(), nullptr, nullptr, &_addrinfo);

    if(err != 0)
    {
        printf("error: %s\n", gai_strerror(err));
        return "";
    }

    for(_res = _addrinfo; _res != nullptr; _res = _res->ai_next)
    {
        if(_res->ai_family == AF_INET)
        {

            if(inet_ntop(AF_INET, &((struct sockaddr_in *)_res->ai_addr)->sin_addr, _address, sizeof(_address)) == nullptr)
            {
                return "";
            }
            string s(_address);
            wstring loginfo = L"\nIPv4 - ";
            wstring ip(s.begin(), s.end());
            loginfo += ip;
            ips += s + ",";
        }
        else if(_res->ai_family == AF_INET6)
        {
            if(inet_ntop(AF_INET6, &((struct sockaddr_in *)_res->ai_addr)->sin_addr, _address, sizeof(_address)) == nullptr)
            {
                return "";
            }
            string s(_address);
            wstring loginfo = L"\nIPv6 - ";
            wstring ip(s.begin(), s.end());
            loginfo += ip;
            ips += s + ",";
        }
    }
}


inline vector<wstring> split(const wstring &s, wchar_t delim)
{
    vector<wstring> elems;
    wstringstream ss;
    ss.str(s);
    wstring item;
    while (getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

// Fault objects...
// These structs just hold data passed to external console processes (ExecPath)...
struct CpuPressure
{
	int PressureLevel = 0;
	int Duration = 0;
	//int RunOrder = 0;
	wstring ExePath = L"BedlamBinaries\\Stressfulx64.exe";
	wstring RunCommand;
};
struct MemoryPressure
{
	int PressureLevel = 0;
	int Duration = 0;
	//int RunOrder = 0;
	wstring ExePath = L"BedlamBinaries\\testlimitx64.exe";
	wstring RunCommand;
};

//Globals...
wstring g_logfilepath;
wstring g_execPath;
int g_repeat = 0;
Json g_json = nullptr;

CpuPressure Cpu;
MemoryPressure Mem;
NetworkEmulationProfile Netem;
bool g_bCPUConfig, g_bMemConfig, g_bNetemConfig = false;
vector<pair<void(*)(),int>> seqrunVec;

enum Orchestration
{
	Concurrent,
	Random,
	Sequential
};
Orchestration g_orchestration;

// wstring->object enum/objects mappers...
static map<string, Orchestration> OrchwstringValues;
map<string, Orchestration> MapStringToOrchestration =
{
	{ "Concurrent", Concurrent },
	{ "Random", Random },
	{ "Sequential", Sequential }
};

static map<string, int> ProtocolStringValues;
map<string, int> MapStringToProtocol =
{
	{ "ALL", ALL_PROTOCOL },
	{ "ESP",  ESP },
	{ "ICMP", ICMP },
	{ "ICMPv6", ICMPv6 },
	{ "TCP",  TCP },
	{ "UDP",  UDP }
};
static map<string, NetworkType> NetworkTypeStringValues;
map<string, NetworkType> MapStringToNetworkType =
{
	{ "ALL", ALL_NETWORK },
	{ "IPv4", Ipv4 },
	{ "IPv6", Ipv6 },
};

inline unsigned long long GetAvailableSystemMemory()
{
	//MEMORYSTATUSEX status;
	//status.dwLength = sizeof(status);
	//GlobalMemoryStatusEx(&status);
	//return status.ullAvailPhys;
}

inline int GetMemPressureAllocation(int pressureLevelPt)
{
	auto allocation = static_cast<int>(round(pressureLevelPt / 100.0 * GetAvailableSystemMemory() / (1024.0 * 1024.0)));
	return allocation;
}

inline bool RunOperation(const wstring &command, int duration)
{
	if(duration/1000 < 1)
	{
		duration *= 1000; //to ms...
	}
	auto cmd = command.c_str();
	
	return true;
}
//CPU
inline void RunCpuPressure()
{
	if (g_bCPUConfig == false)
	{
		return;
	}
	bool isCpuObj = g_json["ChaosConfiguration"]["CpuPressure"].is_object();
	if (isCpuObj)
	{
		auto isDuration = g_json["ChaosConfiguration"]["CpuPressure"]["Duration"].is_number();
		if (isDuration)
		{
			auto duration = g_json["ChaosConfiguration"]["CpuPressure"]["Duration"].int_value();
			Cpu.Duration = duration;
		}
		auto isLevel = g_json["ChaosConfiguration"]["CpuPressure"]["PressureLevel"].is_number();
		int level = 0;
		if (isLevel)
		{
			level = g_json["ChaosConfiguration"]["CpuPressure"]["PressureLevel"].int_value();
			Cpu.PressureLevel = level;
		}
		
		Cpu.RunCommand = g_execPath + L"\\" + Cpu.ExePath + L" " + wstring(to_wstring(Cpu.PressureLevel));
		wstring loginfo = L"Starting CPU pressure: " + wstring(to_wstring(level));
		//LOGINFO(g_logger, loginfo.c_str());
		if (RunOperation(Cpu.RunCommand, Cpu.Duration))
		{
			//LOGINFO(g_logger, L"Stopping CPU pressure.")
		}
		else
		{
			//LOGERROR(g_logger, L"CPU Pressure run failed.")
		}
	}
	else
	{
		//LOGINFO(g_logger, L"No CPU pressure specified...");
	}
}
//Memory pressure...
inline void RunMemoryPressure()
{
	if (g_bMemConfig == false)
	{
		return;
	}
	bool isMemObj = g_json["ChaosConfiguration"]["MemoryPressure"].is_object();
	if (isMemObj)
	{
		auto isDuration = g_json["ChaosConfiguration"]["MemoryPressure"]["Duration"].is_number();
		if (isDuration)
		{
			auto duration = g_json["ChaosConfiguration"]["MemoryPressure"]["Duration"].int_value();
			Mem.Duration = duration;
		}
		auto isLevel = g_json["ChaosConfiguration"]["MemoryPressure"]["PressureLevel"].is_number();
		int level = 0;
		if (isLevel)
		{
			level = g_json["ChaosConfiguration"]["MemoryPressure"]["PressureLevel"].int_value();
			Mem.PressureLevel = level;
		}
		
		Mem.RunCommand = g_execPath + L"\\" + Mem.ExePath + L" -d " + wstring(to_wstring(GetMemPressureAllocation(Mem.PressureLevel))) + L" -c 1";
		wstring loginfo = L"Starting memory pressure: " + wstring(to_wstring(level));
		//LOGINFO(g_logger, wstring(loginfo.begin(), loginfo.end()).c_str());

		if (RunOperation(Mem.RunCommand, Mem.Duration))
		{
			//LOGINFO(g_logger, L"Stopping Memory pressure.")
		}
		else
		{
			//LOGERROR(g_logger, L"Memory Pressure run failed.")
		}
	}
	else
	{
		//LOGINFO(g_logger, L"No Memory pressure specified...");
	}
}
// Network emulation... 
inline void RunNetworkEmulation()
{
	if(g_bNetemConfig == false)
	{
		return;
	}

	bool isNetemObj = g_json["ChaosConfiguration"]["NetworkEmulation"].is_object();
	if (isNetemObj)
	{
		NetworkEmulationProfile profile;
		bool isDuration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].is_number();
		if(isDuration)
		{
			int duration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].int_value();
			profile.Duration = duration;
		}
		else
		{
			//LOGERROR(g_logger, L"Must specifiy a duration for network emulation...");
			return;
		}
        //Parse endpoints, build ip string...
        bool isArr = g_json["ChaosConfiguration"]["NetworkEmulation"]["TargetEndpoints"]["Endpoint"].is_array();
        wstring hostnames = L"{ ";
        string ips;
        if (isArr)
        {
            auto arrEndpoints = g_json["ChaosConfiguration"]["NetworkEmulation"]["TargetEndpoints"]["Endpoint"].array_items();

            for (size_t i = 0; i < arrEndpoints.size(); i++)
            {
                string hostname = arrEndpoints[i]["Hostname"].string_value();
                //For logging... TODO...
                wstring wshostname = wstring(hostname.begin(), hostname.end());
                hostnames += wshostname + L" ";

                //Get ips from endpoint hostname, build the ips string to pass to bash script...
                ips += hostname2ips(hostname);

            }
        }
		
		string s_emType = g_json["ChaosConfiguration"]["NetworkEmulation"]["EmulationType"].string_value();
		wstring netemLogType;
		EmulationType emType = MapStringToEmulationType[s_emType];
		string bashCmd;

		switch (emType)
		{
			case Bandwidth:
			{
				//profile.Type = Bandwidth;
				netemLogType = L"Bandwidth emulation";
				int upbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthUpstreamSpeed"].int_value();
				int dbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthDownstreamSpeed"].int_value();
				auto DownlinkBandwidthBps = dbw;
				auto UplinkBandwidthBps = upbw;
                bashCmd = "Bash/netem-bandwidth.sh -ips=" + ips.substr(0, ips.size()-1) + " " +
                          to_string(dbw) + " " + to_string(profile.Duration) + "s";
				break;
			}
			case Disconnect: //TODO for linux...
			{
				//profile.Type = Disconnect;
				netemLogType = L"Disconnection emulation";

				auto PeriodicDisconnectionRate = g_json["ChaosConfiguratioin"]["NetworkEmulation"]["PeriodicDisconnectionRate"].int_value();
				auto ConnectionTime = (unsigned int) g_json["ChaosConfiguration"]["NetworkEmulation"]["ConnectionTime"].int_value();
				auto DisconnectionTime = (unsigned int) g_json["ChaosConfiguration"]["NetworkEmulation"]["DisconnectionTime"].int_value();
				break;
			}
			case Loss: //TODO
			{
				netemLogType = L"Loss (Random) emulation";
				//auto inloss = g_json["ChaosConfiguration"]["NetworkEmulation"]["RandomLossRate"].int_value();
				break;
			}
			case Latency:
			{
				//profile.Type = Latency;
				netemLogType = L"Latency emulation";
				int delay = g_json["ChaosConfiguration"]["NetworkEmulation"]["LatencyDelay"].int_value();
				//profile.Latency.Type = FIXED_LATENCY;
				auto IncomingLatency = delay;
				auto OutgoingLatency = delay;
				break;
			}
			default:
				{
					//LOGINFO(g_logger, L"Emulation type unrecognized.");
					return;
				}
		}

		auto loginfo = L"Starting " + netemLogType + L" for " + hostnames + L" }";
		//LOGINFO(g_logger, loginfo.c_str());
		//RunNetworkEmulator(profile);
		//LOGINFO(g_logger, L"Stopping network emulation")
	}
	else
	{
		//LOGINFO(g_logger, L"No Network emulation specified...");
	}
}

inline void InitGlobalPaths()
{
	//TCHAR NPath[MAX_PATH];
	//GetModuleFileName(nullptr, NPath, sizeof(NPath));
	//GetCurrentDirectory(MAX_PATH, NPath);
	//g_execPath = wstring(NPath);
	//wstring dir = g_execPath + L"\\bedlamlogs";

	//if (CreateDirectory(dir.c_str(), nullptr) || ERROR_ALREADY_EXISTS == GetLastError())
	//{
		//g_logfilepath = dir + L"\\CloudBedlam.log";
		//g_logger.AddOutputStream(new wofstream(g_logfilepath.c_str()), true, LogLevel::Info);
	//	g_logger.AddOutputStream(new wofstream(g_logfilepath.c_str()), true, LogLevel::Error);
	//}
}
inline bool ParseConfigurationObjectAndInitialize()
{
	if (g_json == nullptr)//so, first run (not a repeat run...)...
	{
		ifstream chaos_config("chaos.json");
		if (chaos_config.is_open())
		{
			chaos_config.seekg(0, ios::end);
			auto size = chaos_config.tellg();
			string buffer(size, ' ');
			chaos_config.seekg(0);
			chaos_config.read(&buffer[0], size);
			string err;
			g_json = Json::parse(buffer, err, STANDARD);
			chaos_config.close();
		}
		else
		{
			//LOGERROR(g_logger, L"Can't open configuration file. Aborting...");
			exit(-1);
		}
	}

	bool isChaosConfigObj = g_json["ChaosConfiguration"].is_object();
	if (isChaosConfigObj)
	{
		auto isOrch = g_json["ChaosConfiguration"]["Orchestration"].is_string();
		if (isOrch)
		{
			string orc = g_json["ChaosConfiguration"]["Orchestration"].string_value();
			g_orchestration = MapStringToOrchestration[orc];
			string orch = "Orchestration: " + orc;
			
			//LOGINFO(g_logger, wstring(orch.begin(), orch.end()).c_str());
		}
		auto isRepeat = g_json["ChaosConfiguration"]["Repeat"].is_number();
		if (isRepeat)
		{
			g_repeat = g_json["ChaosConfiguration"]["Repeat"].int_value();
		}
		auto isDelay = g_json["ChaosConfiguration"]["RunDelay"].is_number();
		if (isDelay)
		{
			//sleep(g_json["ChaosConfiguration"]["RunDelay"].int_value() * 1000);
		}
		//What's in the config?...
		bool isCpuObject = g_json["ChaosConfiguration"]["CpuPressure"].is_object();
		if (isCpuObject)
		{
			g_bCPUConfig = true;
		}
		//Mem
		bool isMemObject = g_json["ChaosConfiguration"]["MemoryPressure"].is_object();
		if (isMemObject)
		{
			g_bMemConfig = true;
		}
		//Netem
		bool isNetemObject = g_json["ChaosConfiguration"]["NetworkEmulation"].is_object();
		if (isNetemObject)
		{
			g_bNetemConfig = true;
		}
		//If Sequential orc, then set runorders...
		if (g_orchestration == Orchestration::Sequential)
		{
			if (g_bCPUConfig)
			{
				bool isRunOrder = g_json["ChaosConfiguration"]["CpuPressure"]["RunOrder"].is_number();
				if (isRunOrder)
				{
					int r = g_json["ChaosConfiguration"]["CpuPressure"]["RunOrder"].int_value();
					seqrunVec.emplace_back(RunCpuPressure, r);
				}
			}
			//Mem
			if (g_bMemConfig)
			{
				bool isRunOrder = g_json["ChaosConfiguration"]["MemoryPressure"]["RunOrder"].is_number();
				if (isRunOrder)
				{
					int r = g_json["ChaosConfiguration"]["MemoryPressure"]["RunOrder"].int_value();
					seqrunVec.emplace_back(RunMemoryPressure, r);
				}
			}
			//Netem
			if (g_bNetemConfig)
			{
				bool isRunOrder = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].is_number();
				if (isRunOrder)
				{
					int r = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].int_value();
					seqrunVec.emplace_back(RunNetworkEmulation, r);
				}
			}
		}
		return true;
	}
	//LOGERROR(g_logger, L"Invalid Json (Not a ChaosConfiguration...). Make sure you supplied the correct format...");
	return false;
}

inline bool sort_pair_asc(const pair<void(*)(),int>& vec1, const pair<void(*)(), int>& vec2) 
{
	return vec1.second < vec2.second;
}

inline void MakeBedlam()
{
	switch(g_orchestration)
	{
		case Concurrent:
		{
			//Run all functions concurrently, blocking the main thread (which we want...)...
			vector<thread> threads;
			threads.push_back(thread(RunCpuPressure));
			threads.push_back(thread(RunMemoryPressure));
			threads.push_back(thread(RunNetworkEmulation));
			//Join... (wait for the processes/code running in the threads to exit/complete)
			for (auto& thread : threads) 
			{
				thread.join();
			}
			break;
		}
		case Random:
		{
			// Run each function based on random number between 0 and 3
			// Must set seed here...
			srand(time(nullptr));
			for (auto i = 0; i < 3; ++i) 
			{
				switch (rand() % 3)
				{
					case 0: RunCpuPressure(); break;
					case 1: RunMemoryPressure(); break;
					case 2: RunNetworkEmulation(); break;
				default: ;
				}
			}
			break;
		}
		case Sequential: 
		{
			// sort on RunOrder (the int in vector<pair<void(*)(),int>>...)
			sort(seqrunVec.begin(), seqrunVec.end(), sort_pair_asc);
			// run each function in the sorted pair...
			for (auto it = seqrunVec.begin(); it != seqrunVec.end(); ++it)
			{
				(*it).first();
			}
			break;
		}
	}
}

inline void SetOperationsFromJsonAndRun()
{
	if (ParseConfigurationObjectAndInitialize())
	{
		MakeBedlam();
	}
}