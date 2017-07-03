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
#include <sys/wait.h>
#include <zconf.h>
#include <signal.h>
#include <cstring>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

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
    Corruption,
    Disconnect,
    Latency,
    Loss,
    Reorder
} EmulationType;

static map<string, EmulationType> EmStringValues;
map<string, EmulationType> MapStringToEmulationType =
        {
            { "Bandwidth", Bandwidth },
            { "Corruption", Corruption },
            { "Disconnect", Disconnect },
            { "Latency", Latency },
            { "Loss", Loss },
            { "Reorder", Reorder }
        };

struct NetworkEmulationProfile
{
    vector<Endpoint> Endpoints;
    int Duration = 0;
    int RunOrder = 0;
    EmulationType Type;
};

inline string hostname2ips(const string &hostname)
{
    struct addrinfo* addrinfo = nullptr;
    struct addrinfo hints;

    int err = 0;
    string ips;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    err = getaddrinfo(hostname.c_str(), nullptr, &hints, &addrinfo);

    if(err != 0)
    {
        printf("error: %s\n", gai_strerror(err));
        return "";
    }

    for(auto ptr = addrinfo; ptr != nullptr; ptr = ptr->ai_next)
    {
        if(ptr->ai_family == AF_INET)
        {
            char address[INET6_ADDRSTRLEN];
            /* Obtain address(es) matching host/port */

            if(inet_ntop(AF_INET, &((struct sockaddr_in *)ptr->ai_addr)->sin_addr, address, sizeof(address)) == nullptr)
            {
                return "";
            }
            string s(address);
            wstring loginfo = L"\nIPv4 - ";
            wstring ip(s.begin(), s.end());
            loginfo += ip;
            ips += s + ",";
        }
        else if(ptr->ai_family == AF_INET6)
        {
            char address[INET6_ADDRSTRLEN];
            if(inet_ntop(AF_INET6, &((struct sockaddr_in *)ptr->ai_addr)->sin_addr, address, sizeof(address)) == nullptr)
            {
                return "";
            }
            string s(address);
            wstring loginfo = L"\nIPv6 - ";
            wstring ip(s.begin(), s.end());
            loginfo += ip;
            ips += s + ",";
        }
    }
    freeaddrinfo(addrinfo);           /* No longer needed */
    return ips;
}

// Fault objects...
// These structs just hold data passed to external console processes (ExecPath)...
struct CpuPressure
{
	int PressureLevel = 0;
	int Duration = 0;
	//int RunOrder = 0;
	string CmdArgs = "Bash/stress-cpu.sh ";
};
struct MemoryPressure
{
	int PressureLevel = 0;
	int Duration = 0;
	//int RunOrder = 0;
	string CmdArgs = "Bash/stress-mem.sh ";
};

//Globals...
string g_logfilepath;
int g_repeat = 0;
Json g_json = nullptr;
int g_delay;
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

inline vector<string> Split(const string &s, char delim)
{
    vector<string> elems;
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

inline bool RunOperation(string command)
{
    pid_t pid;
    printf("before fork\n");
    int ret, status;
    //convert string to char*...
    //char *cstr = command[0u];
    auto s = Split(command, ' ');
    char* c[] = {
            &s[0][0u],
            &s[1][0u],
            &s[2][0u],
            &s[3][0u]
    };
    if((pid = fork()) < 0)
    {
        printf("an error occurred while forking\n");
        return false;
    }
    else if(pid == 0)
    {
        /* this is the child */
        printf("the child's pid is: %d\n", getpid());
        char* cc = nullptr;
        if(s.size() == 4)
        {
            cc = c[3];
        }

        if(execl("/bin/bash", "-c", c[0], c[1], c[2], cc, nullptr))
        {
            return true;
        }

        printf("if this line is printed then execv failed\n");
        return false;
    }
    else
    {
        /* this is the parent */
        if (-1 == (ret = wait(&status)))
        {
            printf ("parent:error\n");
            return false;
        }
        printf("parent continues execution\n");
        return true;
    }
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
		Cpu.CmdArgs += to_string(Cpu.PressureLevel) + " " + to_string(Cpu.Duration);
		wstring loginfo = L"Starting CPU pressure: " + wstring(to_wstring(level));
		//LOGINFO(g_logger, loginfo.c_str());
		if (RunOperation(Cpu.CmdArgs))
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
		Mem.CmdArgs += to_string(Mem.PressureLevel) + " " + to_string(Mem.Duration);
		wstring loginfo = L"Starting memory pressure: " + wstring(to_wstring(level));
		//LOGINFO(g_logger, wstring(loginfo.begin(), loginfo.end()).c_str());

		if (RunOperation(Mem.CmdArgs))
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
		bool isDuration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].is_number();
		if(isDuration)
		{
			int duration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].int_value();
            Netem.Duration = duration;
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
        //remove trailing character (in our case, a comma will always be the last char...)
        ips = ips.substr(0, ips.size() - 1);

		string s_emType = g_json["ChaosConfiguration"]["NetworkEmulation"]["EmulationType"].string_value();
		wstring netemLogType;
		EmulationType emType = MapStringToEmulationType[s_emType];
		string bashCmd;

		switch (emType)
		{
			case Bandwidth:
			{
				Netem.Type = Bandwidth;
				netemLogType = L"Bandwidth emulation";
				int upbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthUpstreamSpeed"].int_value();
				int dbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthDownstreamSpeed"].int_value();

                bashCmd = "Bash/netem-bandwidth.sh -ips=" + ips + " " + to_string(dbw) + " " + to_string(Netem.Duration) + "s";
				break;
			}
            case Corruption:
            {
                Netem.Type = Corruption;
                netemLogType = L"Corruption emulation";
                auto pt = g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].int_value() * 100; //e.g., 0.05 * 100 = 5...
                bashCmd = "Bash/netem-corrupt.sh -ips=" + ips + " " +
                       to_string(pt) + " " + to_string(Netem.Duration) + "s";
                break;
            }
			case Disconnect: //TODO...
			{
				/*
                netemLogType = L"Disconnection emulation";

				auto PeriodicDisconnectionRate = g_json["ChaosConfiguratioin"]["NetworkEmulation"]["PeriodicDisconnectionRate"].int_value();
				auto ConnectionTime = (unsigned int) g_json["ChaosConfiguration"]["NetworkEmulation"]["ConnectionTime"].int_value();
				auto DisconnectionTime = (unsigned int) g_json["ChaosConfiguration"]["NetworkEmulation"]["DisconnectionTime"].int_value();
				*/
                break;
			}
			case Loss:
			{
				Netem.Type = Loss;
                netemLogType = L"Loss (Random) emulation";
				auto inloss = g_json["ChaosConfiguration"]["NetworkEmulation"]["RandomLossRate"].int_value();
                auto lossRate = inloss * 100; //e.g., 0.05 * 100 = 5...

                bashCmd = "Bash/netem-loss.sh -ips=" + ips + " " + to_string(lossRate) + " " + to_string(Netem.Duration);
				break;
			}
			case Latency:
			{
				Netem.Type = Latency;
                netemLogType = L"Latency emulation";
				int delay = g_json["ChaosConfiguration"]["NetworkEmulation"]["LatencyDelay"].int_value();

                bashCmd = "Bash/netem-latency.sh -ips=" + ips + " " + to_string(delay) + "ms " + " " + to_string(Netem.Duration) + "s";
				break;
			}
            case Reorder:
            {
                Netem.Type = Reorder;
                netemLogType = L"Reorder emulation";
                auto correlationpt = g_json["ChaosConfiguration"]["NetworkEmulation"]["CorrelationPercentage"].int_value() * 100;
                auto packetpt = g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].int_value() * 100;

                bashCmd = "Bash/netem-reorder.sh -ips=" + ips + " " + to_string(packetpt) + " " + to_string(correlationpt) + " " + to_string(Netem.Duration);
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
        if(RunOperation(bashCmd))
        {
            //LOG.....
        }
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
                g_delay = g_json["ChaosConfiguration"]["RunDelay"].int_value() * 1000;
                sleep(g_delay);
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