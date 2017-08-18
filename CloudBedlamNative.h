#pragma once

#include <cerrno>
#include <fstream>
#include <vector>
#include "json11.hpp"
#include "pthread.h"
#include <thread>
#include <algorithm>
#include <map>
#include <sstream>
#include <cstdio>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <sys/wait.h>
#include <zconf.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include "spdlog/spdlog.h"
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <time.h>
#include <bits/stat.h>

using namespace json11;
using namespace std;
namespace spd = spdlog;

typedef enum
{
    ALL_PROTOCOL,
    ICMP,
    TCP,
    UDP,
    ESP,
    AH,
    ICMPv6
}Protocol;

typedef enum
{
    ALL_NETWORK,
    Ipv4,
    Ipv6
}NetworkType;

typedef enum
{
    Concurrent,
    Random,
    Sequential
}Orchestration;

//Globals...
Json g_json = nullptr;
unsigned int g_delay = 0;
int g_repeat = 0;
Orchestration g_orchestration;
//Logger...
std::shared_ptr<spd::logger> g_logger;
//Operational settings...
bool g_bCPUConfig, g_bMemConfig, g_bNetemConfig, g_bRepeating = false;
string g_loginfoNetem, g_loginfoCpu, g_logInfoMem;
//container used for sequential/random orchestration of bedlam operations...
vector<pair<void(*)(),int>> g_seqrunVec;

struct Endpoint
{
    string Hostname;
    int Port = 0;
    int Protocol = TCP;
    NetworkType Network = Ipv4;
};

typedef enum
{
    None,
    Bandwidth,
    Corruption,
    Disconnect,
    Latency,
    Loss,
    Reorder
} EmulationType;

// string->object enum/objects mappers...

//Orchestration...
map<string, Orchestration> MapStringToOrchestration;

//TODO: Add support (bash) to specifying protocol...
//Protocol...
map<string, Protocol> MapStringToProtocol;

//TODO: Add support (bash) to specifying network type...
//Network type...
map<string, NetworkType> MapStringToNetworkType;
//Emulation type...
map<string, EmulationType> MapStringToEmulationType;

// Fault objects...
// These structs just hold data passed to bash scripts (CmdArgs)...
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
struct NetworkEmulationProfile
{
    vector<Endpoint> Endpoints;
    int Duration = 0;
    int RunOrder = 0;
    EmulationType Type = None;
    string CmdArgs = "";
};

/**Impls...**/

unsigned long long getTotalSystemMemory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

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
        g_logger->error(gai_strerror(err));
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
                g_logger->error("hostname2ip: Unable to resolve IPv4 address.");
                return "";
            }
            string s(address);
            ips += s + ",";
        }
        else if(ptr->ai_family == AF_INET6)
        {
            char address[INET6_ADDRSTRLEN];
            if(inet_ntop(AF_INET6, &((struct sockaddr_in *)ptr->ai_addr)->sin_addr, address, sizeof(address)) == nullptr)
            {
                g_logger->error("hostname2ip: Unable to resolve IPv6 address.");
                return "";
            }
            string s(address);
            ips += s + ",";
        }
    }
    freeaddrinfo(addrinfo);
    return ips;
}

//Bedlam operation instances...
CpuPressure Cpu;
MemoryPressure Mem;
NetworkEmulationProfile Netem;

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

inline bool RunOperation(const string& command)
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

    int status;
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

    if(pid == 0)
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

        g_logger->error("execl failed!" + c[0][0]);
        return false;
    }

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

                perror("read");
                //Log...

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
    return waitpid(pid, &status, WNOHANG) != 0;
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
            g_logInfoMem = "Starting memory pressure: " + to_string(Mem.PressureLevel) + " " + to_string(Mem.Duration) + "s";
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
            bool isArr = g_json["ChaosConfiguration"]["NetworkEmulation"]["Endpoints"].is_array();
            string hostnames = "{ ";
            string ips;
            if (isArr)
            {
                auto arrEndpoints = g_json["ChaosConfiguration"]["NetworkEmulation"]["Endpoints"].array_items();

                for (size_t i = 0; i < arrEndpoints.size(); i++)
                {
                    if(arrEndpoints[i]["Hostname"].is_string())
                    {
                        string hostname = arrEndpoints[i]["Hostname"].string_value();
                        if (!hostname.empty())
                        {
                            //For logging...
                            hostnames += hostname + " ";

                            //Get ips from endpoint hostname, build the ips string to pass to bash script...
                            string ip = hostname2ips(hostname);
                            if (!ip.empty())
                            {
                                ips += ip;
                            }
                        }
                    }
                }
            }
            //remove trailing character (in our case, a comma will always be the last char...)
            ips = ips.substr(0, ips.size() - 1);

            string s_emType = g_json["ChaosConfiguration"]["NetworkEmulation"]["EmulationType"].string_value();

            if(s_emType.empty())
            {
                g_logger->error("You must supply an emulation type...");
                return;
            }

            string netemLogType;
            EmulationType emType = MapStringToEmulationType[s_emType];
            string bashCmd;

            switch (emType)
            {
                case Bandwidth:
                {
                    Netem.Type = Bandwidth;
                    netemLogType = "Bandwidth emulation";
                    auto upbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["UpstreamSpeed"].number_value();
                    auto dbw = g_json["ChaosConfiguration"]["NetworkEmulation"]["DownstreamSpeed"].number_value();

                    Netem.CmdArgs =
                            "Bash/netem-bandwidth.sh -ips=" + ips + " " + to_string(dbw) + " " + to_string(upbw) + " " +
                            to_string(Netem.Duration) +
                            "s";
                    break;
                }
                case Corruption:
                {
                    Netem.Type = Corruption;
                    netemLogType = "Corruption emulation";
                    auto pt = g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].number_value() * 100; //e.g., 0.05 * 100 = 5...
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
                    string lossType = "Random";
                    bool hasLossType = g_json["ChaosConfiguration"]["NetworkEmulation"]["LossType"].is_string();
                    if(hasLossType)
                    {
                        lossType = g_json["ChaosConfiguration"]["NetworkEmulation"]["LossType"].string_value();
                    }
                    auto lossRate = 0, burstRate = 0;
                    bool isRandom = g_json["ChaosConfiguration"]["NetworkEmulation"]["LossRate"].is_number();
                    if(isRandom)
                    {
                        lossRate = g_json["ChaosConfiguration"]["NetworkEmulation"]["LossRate"].number_value() * 100;
                    }
                    bool isBurst = g_json["ChaosConfiguration"]["NetworkEmulation"]["BurstRate"].is_number();
                    if(isBurst)
                    {
                        burstRate = g_json["ChaosConfiguration"]["NetworkEmulation"]["BurstRate"].number_value() * 100;
                    }
                    netemLogType = lossType + " Loss emulation";

                    Netem.CmdArgs =
                            "Bash/netem-loss.sh -ips=" + ips + " " + to_string(lossRate) + " " +
                                    to_string(burstRate) + " " + to_string(Netem.Duration);
                    break;
                }
                case Latency:
                {
                    Netem.Type = Latency;
                    netemLogType = "Latency emulation";
                    auto delay = g_json["ChaosConfiguration"]["NetworkEmulation"]["LatencyDelay"].number_value();

                    Netem.CmdArgs = "Bash/netem-latency.sh -ips=" + ips + " " + to_string(delay) + "ms " + " " +
                                    to_string(Netem.Duration) + "s";
                    break;
                }
                case Reorder:
                {
                    Netem.Type = Reorder;
                    netemLogType = "Reorder emulation";
                    auto correlationpt =
                            g_json["ChaosConfiguration"]["NetworkEmulation"]["CorrelationPercentage"].number_value() * 100;
                    auto packetpt =
                            g_json["ChaosConfiguration"]["NetworkEmulation"]["PacketPercentage"].number_value() * 100;

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
        g_logger->info("Network emulation operation succeeded.");
    }
    else
    {
        g_logger->error("Network emulation operation failed.");
        return;
    }
    g_logger->info("Stopping network emulation.");
}

inline void InitGlobals()
{
    struct stat st = {0};

    if (stat("bedlamlogs", &st) == -1)
    {
        mkdir("bedlamlogs", 0700);
    }

    char filename[46];
    struct tm *ctime;
    time_t now = time(nullptr);
    ctime = gmtime(&now);
    strftime(filename, sizeof(filename), "bedlamlogs/%Y-%m-%d_%H:%M:%S_bedlam.log", ctime);
    g_logger = spd::basic_logger_mt("cb_logger", filename);
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
			string buffer(static_cast<unsigned long>(size), ' ');
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
            MapStringToOrchestration =
            {
                { "Concurrent", Concurrent },
                { "Random", Random },
                { "Sequential", Sequential }
            };

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
                g_delay = (unsigned int)g_json["ChaosConfiguration"]["RunDelay"].int_value();
                g_logger->info("Delay: " + to_string(g_delay) + "s");
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
            //If Sequential OR Random orc, then set g_seqrunVec...
            if (g_orchestration == Orchestration::Sequential || g_orchestration == Orchestration::Random)
            {
                if (g_bCPUConfig)
                {
                    bool isRunOrder = g_json["ChaosConfiguration"]["CpuPressure"]["RunOrder"].is_number();
                    if (isRunOrder)
                    {
                        int r = g_json["ChaosConfiguration"]["CpuPressure"]["RunOrder"].int_value();
                        g_seqrunVec.emplace_back(RunCpuPressure, r);
                    }
                    else // no order specified...
                    {
                        auto r = g_seqrunVec.size();
                        r > 0 ? r : (r = 0);
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
                    else // no order specified...
                    {
                        auto r = g_seqrunVec.size();
                        r > 0 ? r : (r = 0);
                        g_seqrunVec.emplace_back(RunMemoryPressure, r);
                    }
                }
                //Netem
                if (g_bNetemConfig)
                {
                    MapStringToEmulationType =
                    {
                            { "Bandwidth", Bandwidth },
                            { "Corruption", Corruption },
                            { "Disconnect", Disconnect },
                            { "Latency", Latency },
                            { "Loss", Loss },
                            { "Reorder", Reorder }
                    };
                    MapStringToProtocol =
                    {
                            { "ALL", ALL_PROTOCOL },
                            { "AH", AH },
                            { "ESP",  ESP },
                            { "ICMP", ICMP },
                            { "ICMPv6", ICMPv6 },
                            { "TCP",  TCP },
                            { "UDP",  UDP }
                    };
                    MapStringToNetworkType =
                    {
                            { "ALL", ALL_NETWORK },
                            { "IPv4", Ipv4 },
                            { "IPv6", Ipv6 },
                    };

                    bool isRunOrder = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].is_number();
                    if (isRunOrder)
                    {
                        int r = g_json["ChaosConfiguration"]["NetworkEmulation"]["RunOrder"].int_value();
                        g_seqrunVec.emplace_back(RunNetworkEmulation, r);
                    }
                    else // no order specified...
                    {
                        auto r = g_seqrunVec.size();
                        r > 0 ? r : (r = 0);
                        g_seqrunVec.emplace_back(RunNetworkEmulation, r);
                    }
                }
            }
            return true;
        }

        g_logger->error("Invalid Json (Not a ChaosConfiguration...). Make sure you supplied the correct format...");
        return false;
	}
    //No need to re-parse JSON if repeat > 0... All of the data is already in memory...
    g_bRepeating = true;
    return true;

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
			threads.emplace_back(RunCpuPressure);
			threads.emplace_back(RunMemoryPressure);
			threads.emplace_back(RunNetworkEmulation);
			//Join... (wait for the processes/code running in the threads to exit/complete)
			for (auto& thread : threads) 
			{
				thread.join();
			}
			break;
		}
		case Random:
		{
			// Must set seed here.
            // obtain a time-based seed:
            unsigned seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
            // built-in shuffle
            std::shuffle(g_seqrunVec.begin(), g_seqrunVec.end(), std::default_random_engine(seed));
            //run each function in the randomly shuffled vector...
            for (auto &it : g_seqrunVec)
            {
                it.first();
            }

			break;
		}
		case Sequential: 
		{
			// sort on RunOrder (the int in vector<pair<void(*)(),int>>...)
			sort(g_seqrunVec.begin(), g_seqrunVec.end(), sort_pair_asc);
			// run each function in the asc sorted vector...
			for (auto &it : g_seqrunVec)
            {
                it.first();
			}
			break;
		}
	}
}

inline void SetOperationsFromJsonAndRun()
{
	if (ParseConfigurationObjectAndInitialize())
	{
        //Delay run?
        if (g_delay > 0)
        {
            sleep(g_delay);
        }
        //Bedlam time...
        MakeBedlam();
	}
}
