#pragma once

#include <errno.h>
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
#include "spdlog/spdlog.h"
#include <iostream>
#include <memory>
#include <sys/stat.h>

#define	ALL_PROTOCOL 0
#define	ICMP 1
#define	TCP 6
#define	UDP 17
#define	ESP 50		//! Flag for Encapsulating Security Payload Header, RFC 2406
#define	AH 51		//! Flag for authentication header, RFC 2402
#define	ICMPv6 58

#define SPDLOG_TRACE_ON
#define SPDLOG_DEBUG_ON

using namespace std;
using namespace json11;
//TODO Add an mt logger...
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
    string CmdArgs = "";
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
	int RunOrder = 0;
	string CmdArgs = "Bash/stress-cpu.sh ";
};
struct MemoryPressure
{
	int PressureLevel = 0;
	int Duration = 0;
	int RunOrder = 0;
	string CmdArgs = "Bash/stress-mem.sh ";
};

//Globals...

Json g_json = nullptr;
int g_delay = 0;
int g_repeat = 0;
//Bedlam operations...
CpuPressure Cpu;
MemoryPressure Mem;
NetworkEmulationProfile Netem;
//Operational settings...
bool g_bCPUConfig, g_bMemConfig, g_bNetemConfig, g_bRepeating = false;
string g_loginfoNetem, g_loginfoCpu, g_logInfoMem = "";
//container used for sequential orchestration of bedlam operations...
vector<pair<void(*)(),int>> g_seqrunVec;
//Logger...
std::shared_ptr<spdlog::logger> g_logger;


enum Orchestration
{
	Concurrent,
	Random,
	Sequential
};
Orchestration g_orchestration;

// string->object enum/objects mappers...
map<string, Orchestration> MapStringToOrchestration =
{
	{ "Concurrent", Concurrent },
	{ "Random", Random },
	{ "Sequential", Sequential }
};

map<string, int> MapStringToProtocol =
{
	{ "ALL", ALL_PROTOCOL },
    { "AH", AH },
	{ "ESP",  ESP },
	{ "ICMP", ICMP },
	{ "ICMPv6", ICMPv6 },
	{ "TCP",  TCP },
	{ "UDP",  UDP }
};

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
    //capture stdout from child...

    int filedes[2];
    bool canCaptureop = true;
    if (pipe(filedes) == -1)
    {
        perror("pipe");
        //log... don't die...
        canCaptureop = false;
    }

    pid_t pid;

    int ret, status;
    auto s = Split(command, ' ');
    char* c[] = {
            &s[0][0u],
            &s[1][0u],
            &s[2][0u],
            &s[3][0u],
            &s[4][0u]
    };
    if((pid = fork()) < 0)
    {
        g_logger->error("an error occurred while forking!");
        return false;
    }
    else if(pid == 0)
    {
        /* child */
        if(canCaptureop)
        {
            while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
            close(filedes[1]);
            close(filedes[0]);
        }
        //printf("child's pid is: %d\n", getpid());
        char* cc = nullptr;
        char* cc1 = nullptr;
        //e.g., netem bandwidth, loss, corruption...
        if(s.size() == 4)
        {
            cc = c[3];
        }
        //e.g., netem Reorder cmd...
        if(s.size() == 5)
        {
            cc = c[3];
            cc1 = c[4];
        }

        if(execl("/bin/bash", "-c", c[0], c[1], c[2], cc, cc1, nullptr))
        {
            return true;
        }

        printf("execl failed\n");
        return false;
    }
    else
    {
        /* parent */
        if(canCaptureop)
        {
            close(filedes[1]);
            char buffer[4096] = { '\0' };
            while (1)
            {
                ssize_t count = read(filedes[0], buffer, sizeof(buffer));
                if (count == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        perror("read");
                        //Log...
                    }
                }
                else if (count == 0)
                {
                    break;
                }
                else
                {
                    unsigned long size = sizeof(buffer);
                    size = std::remove(buffer, buffer + size, '\n') - buffer;
                    g_logger->info(buffer);
                }
            }
            close(filedes[0]);
        }
        //wait for child to exit...
        return waitpid(pid, nullptr, WNOHANG) != 0;
        //Failure...
    }
}

//CPU
inline void RunCpuPressure()
{
	if (!g_bCPUConfig)
	{
		return;
	}

    if(!g_bRepeating)
    {
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
            g_loginfoCpu = "Starting CPU pressure: " + to_string(Cpu.PressureLevel) + "% " + to_string(Cpu.Duration) + "s";
        }
    }
    g_logger->info(g_loginfoCpu);
    //Run...
    if (RunOperation(Cpu.CmdArgs))
    {
        g_logger->info("CPU pressure operation succeeded.");
    }
    else
    {
        g_logger->info("CPU pressure operation failed.");
    }
}
//Memory pressure...
inline void RunMemoryPressure()
{
	if (!g_bMemConfig)
	{
		return;
	}

    if(!g_bRepeating)
    {
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
            g_logInfoMem = "Starting memory pressure: " + to_string(Mem.PressureLevel) + "% " + to_string(Mem.Duration) + "s";
        }
    }
    g_logger->info(g_logInfoMem);
    //Run...
    if (RunOperation(Mem.CmdArgs))
    {
        g_logger->info("Memory pressure operation succeeded.");
    }
    else
    {
        g_logger->info("Memory pressure operation failed.");
    }
}
// Network emulation... 
inline void RunNetworkEmulation()
{
	if(!g_bNetemConfig)
	{
		return;
	}

    if(!g_bRepeating)
    {
        bool isNetemObj = g_json["ChaosConfiguration"]["NetworkEmulation"].is_object();
        if (isNetemObj)
        {
            bool isDuration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].is_number();
            if (isDuration)
            {
                int duration = g_json["ChaosConfiguration"]["NetworkEmulation"]["Duration"].int_value();
                Netem.Duration = duration;
            }
            else
            {
                g_logger->error("Must specify a duration for network emulation...");
                return;
            }
            //Parse endpoints, build ip string...
            bool isArr = g_json["ChaosConfiguration"]["NetworkEmulation"]["TargetEndpoints"]["Endpoint"].is_array();
            string hostnames = "{ ";
            string ips;
            if (isArr)
            {
                auto arrEndpoints = g_json["ChaosConfiguration"]["NetworkEmulation"]["TargetEndpoints"]["Endpoint"].array_items();

                for (size_t i = 0; i < arrEndpoints.size(); i++)
                {
                    string hostname = arrEndpoints[i]["Hostname"].string_value();
                    //For logging... TODO...
                    //wstring wshostname = wstring(hostname.begin(), hostname.end());
                    hostnames += hostname + " ";

                    //Get ips from endpoint hostname, build the ips string to pass to bash script...
                    ips += hostname2ips(hostname);

                }
            }
            //remove trailing character (in our case, a comma will always be the last char...)
            ips = ips.substr(0, ips.size() - 1);

            string s_emType = g_json["ChaosConfiguration"]["NetworkEmulation"]["EmulationType"].string_value();
            string netemLogType;
            EmulationType emType = MapStringToEmulationType[s_emType];
            string bashCmd;

            switch (emType)
            {
                case Bandwidth:
                {
                    Netem.Type = Bandwidth;
                    netemLogType = "Bandwidth emulation";
                    int upbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthUpstreamSpeed"].int_value();
                    int dbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["BandwidthDownstreamSpeed"].int_value();

                    Netem.CmdArgs =
                            "Bash/netem-bandwidth.sh -ips=" + ips + " " + to_string(dbw) + " " +
                            to_string(Netem.Duration) +
                            "s";
                    break;
                }
                case Corruption:
                {
                    Netem.Type = Corruption;
                    netemLogType = "Corruption emulation";
                    auto pt = g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].int_value() *
                              100; //e.g., 0.05 * 100 = 5...
                    Netem.CmdArgs = "Bash/netem-corrupt.sh -ips=" + ips + " " +
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
                    netemLogType = "Loss (Random) emulation";
                    auto inloss = g_json["ChaosConfiguration"]["NetworkEmulation"]["RandomLossRate"].int_value();
                    auto lossRate = inloss * 100; //e.g., 0.05 * 100 = 5...

                    Netem.CmdArgs =
                            "Bash/netem-loss.sh -ips=" + ips + " " + to_string(lossRate) + " " +
                            to_string(Netem.Duration);
                    break;
                }
                case Latency:
                {
                    Netem.Type = Latency;
                    netemLogType = "Latency emulation";
                    int delay = g_json["ChaosConfiguration"]["NetworkEmulation"]["LatencyDelay"].int_value();

                    Netem.CmdArgs = "Bash/netem-latency.sh -ips=" + ips + " " + to_string(delay) + "ms " + " " +
                                    to_string(Netem.Duration) + "s";
                    break;
                }
                case Reorder:
                {
                    Netem.Type = Reorder;
                    netemLogType = "Reorder emulation";
                    auto correlationpt =
                            g_json["ChaosConfiguration"]["NetworkEmulation"]["CorrelationPercentage"].int_value() * 100;
                    auto packetpt =
                            g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].int_value() * 100;

                    Netem.CmdArgs = "Bash/netem-reorder.sh -ips=" + ips + " " + to_string(packetpt) + " " +
                                    to_string(correlationpt) + " " + to_string(Netem.Duration);
                    break;
                }
                default:
                {
                    g_logger->error("Emulation type unrecognized.");
                    return;
                }
            }
            g_loginfoNetem = "Starting " + netemLogType + " for " + hostnames + " }";
        }
    }
    //Run operation...
    g_logger->info(g_loginfoNetem);

    if (RunOperation(Netem.CmdArgs))
    {
        g_logger->info("Netem run operation successful.");
    }
    else
    {
        g_logger->error("Netem run operation failed.");
        return;
    }
    g_logger->info("Stopping network emulation");
}

inline void InitGlobals()
{
    struct stat st = {0};

    if (stat("bedlamlogs", &st) == -1)
    {
        mkdir("bedlamlogs", 0700);
    }

    g_logger = spdlog::basic_logger_mt("basic_logger", "bedlamlogs/bedlam.log");
    g_logger->info("globals initialized and set...");

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
			g_logger->error("Can't open configuration file. Aborting...");
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

                g_logger->info(orch);
            }
            auto isRepeat = g_json["ChaosConfiguration"]["Repeat"].is_number();
            if (isRepeat)
            {
                g_repeat = g_json["ChaosConfiguration"]["Repeat"].int_value();
                g_logger->info("Repeat: " + to_string(g_repeat));
            }
            auto isDelay = g_json["ChaosConfiguration"]["RunDelay"].is_number();
            if (isDelay)
            {
                g_delay = g_json["ChaosConfiguration"]["RunDelay"].int_value() * 1000;
                g_logger->info("Delay: " + to_string(g_delay) + "ms");
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
                        g_seqrunVec.emplace_back(RunCpuPressure, r);
                    }
                }
                //Mem
                if (g_bMemConfig)
                {
                    bool isRunOrder = g_json["ChaosConfiguration"]["MemoryPressure"]["RunOrder"].is_number();
                    if (isRunOrder)
                    {
                        int r = g_json["ChaosConfiguration"]["MemoryPressure"]["RunOrder"].int_value();
                        g_seqrunVec.emplace_back(RunMemoryPressure, r);
                    }
                }
                //Netem
                if (g_bNetemConfig)
                {
                    bool isRunOrder = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].is_number();
                    if (isRunOrder)
                    {
                        int r = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].int_value();
                        g_seqrunVec.emplace_back(RunNetworkEmulation, r);
                    }
                }
            }
            return true;
        }

        g_logger->error("Invalid Json (Not a ChaosConfiguration...). Make sure you supplied the correct format...");
        return false;
	}
    else
    {
        g_bRepeating = true;
        return true;
    }
}

inline bool sort_pair_asc(const pair<void(*)(),int>& vec1, const pair<void(*)(), int>& vec2) 
{
	return vec1.second < vec2.second;
}

inline void MakeBedlam()
{
	switch(g_orchestration)
	{
        //TODO: This impl doesn't work for linux...
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
			sort(g_seqrunVec.begin(), g_seqrunVec.end(), sort_pair_asc);
			// run each function in the sorted pair...
			for (auto it = g_seqrunVec.begin(); it != g_seqrunVec.end(); ++it)
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
		if(g_delay > 0)
        {
            sleep(g_delay);
        }
        MakeBedlam();
	}
}