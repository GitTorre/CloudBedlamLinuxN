# CloudBedlam for Linux -- Native (C++) Impl
# What? Why? How?

### "Chaos Engineering is the discipline of experimenting on a distributed system in order to build confidence in the system’s capability to withstand turbulent conditions in production." 

-From Netflix's Principles of Chaos Engineering Manifesto => http://principlesofchaos.org 

CloudBedlam is a simple, configurable, vm-local chaotic operation orchestrator for resiliency experimentation inside cloud services - chaos, or bedlam as we call it, runs inside individual VMs. At it's core, CloudBedlam causes chaotic conditions by injecting "bedlam" faults (today these are machine resource pressure and network emulation (latency, bandwidth, loss, reorder, corruption...) into underlying virtual machines that power a service or cluster of services... It is useful for exercising your resiliency design and implementation in an effort to find bugs. It’s also useful for, say, testing your alerting system (e.g., CPU and Memory Azure Alerts) to ensure you set them up correctly or didn’t break them with a new deployment. Network emulation inside the VM enables you to verify that your Internet-facing code handles network faults correctly and/or verify that your solutions to latent or disconnected traffic states work correctly (and help you to refine your design or even establish for the first time how you react to and recover from transient networking problems in the cloud…).

Unlike, say, Netflix's ChaosMonkey, shooting down VM instances isn't the interesting chaos we create with CloudBedlam (though it is definitely interesting, incredibly useful chaos! – just different from what CloudBedlam is designed to help you experiment with…). Instead, we want to add conditions to an otherwise happy VM that make it sad and troubled, turbulent and angry - not just killing it. We believe that there is a useful difference between VMs that are running in configurably chaotic states versus VMs that pseudo-randomly suddenly disappear from the map (again, that is excellent chaos for systems composed of many instances, just not what this tool is designed for. Please add Chaos Monkey to your Chaos Engineering toolset. Netflix are the leaders in this domain and most of their tools are open source, even if baked into Spinnaker today (their open source CI/CD pipeline technology)), you can find very nice non-Spinnaker-embedded versions right here on GitHub...).

This is meant to run chaos experiments <i>inside</i> VMs as a way to experiment close to your code and help you identify resiliency bugs in your design and implementation.

### .NET (Mono is primary/up to date, .NET Core branch is not synched with Master...) version is <a href="https://github.com/GitTorre/CloudBedlamLinux"><b>here</b></a>

### Note: 

This has only been rigorously tested on Ubuntu 16.04 LTS (Xenial Xerus)... There will be differences in some of the scripts you'll need to take into account for other distros (specifically, auto installation of required programs, for example...), but for the most part, this should work on most mainline distros with only a few mods... In general, if the Linux variant you want to run CB on supports installation of iproute2, stress, and stress-ng, then it's fine... 

Obviously, you need to be <i><b>allowed</b></i> (so, by policy on your team...) to run non-service binaries onboard the VMs that host your cloud services. Further, due to network emulation via tc, which runs code inside the kernel, you have to run CB as a sudo user. 

Currently, there is no big red button (Stop) implemented, but it will be coming as this evolves into a system that can be remote controlled over SSH by a mutually trusted service (where having a big red button really matters given in that case you'd probably have multiple VMs running CB...)... In this impl, you control lifetimes of chaotic operations via configuration settings (duration) for each operation you want to use to induce controlled, directed, deterministic chaos as part of your chaos experimentation.


### Easy to use 
Step 0.

Just change JSON settings to meet your specific chaotic needs. The default config will run CPU, Memory and Networking chaos. You can remove the CPU and Memory objects and just do Network emulation or remove Network and just do CPU/Mem. It's configurable, so do what you want!

#### Operational Orchestration (Orchestration setting):  

       Concurrent (run all operations at the same time)  
       Random (run operations sequentially, in random order)  
       Sequential (run operations sequentially, based on specified run order (RunOrder))  

#### Currently supported machine resource pressure operations:  

       CPU (all CPUs) - CpuPressure setting (0 - 100)  
       Memory (non-swap.. TODO) - MemoryPressure setting (0 - 100)  

#### Currently<a href="https://github.com/GitTorre/CloudBedlamLinuxN/blob/master/NetemReadMe.md" target="blank"> supported IPv4/IPV6 network emulation operations</a> - (NetworkEmulation EmulationType settings)

       Bandwidth 
       Corruption
       Latency 
       Loss  
       Reorder 
       
#### TODO (right now, it's ALL...) Network Protocols: 

       ALL
       ICMP
       TCP
       UDP
       ESP
       AH
       ICMPv6
       
#### Network emulation is done for specific endpoints (specified as hostnames) only - Endpoints object. CloudBedlam determines currrent Network type (IPv4 or IPv6) and IPs from hostnames.     

#### NOTE: 
Network emulation requires iproute2 tools (tc and ip, particularly, in CB's case...). This should be present on most mainline distros already, but make sure...

Description: networking and traffic control tools
 The iproute2 suite is a collection of utilities for networking and
 traffic control.

 These tools communicate with the Linux kernel via the (rt)netlink
 interface, providing advanced features not available through the
 legacy net-tools commands 'ifconfig' and 'route'.
Homepage: http://www.linux-foundation.org/en/Net:Iproute2


### Example configuration:

The JSON below instructs CloudBedlam to sequentially run (according to specified run order) a CPU pressure fault of 90% CPU utilization across all CPUs for 15 seconds, Memory pressure fault eating 90% of available memory for 15 seconds, and Network Bandwidth emulation of 56kbits for 30 seconds for specified target endpoints. The experiment runs 3 times successively ("Repeat": 2) with a start delay of 5 seconds (RunDelay: 5) per iteration.  

CloudBedlam will execute (and log) the orchestration of these bedlam operations. You just need to modify some JSON and then experiment away. Enjoy!
<pre><code>
{
  "ChaosConfiguration": {
    "Orchestration": "Sequential",
    "RunDelay": 5,
    "Repeat": 2,
    "CpuPressure": {
      "Duration": 15,
      "PressureLevel": 90,
      "RunOrder": 1 
    }, 
    "MemoryPressure": {
      "Duration": 15,
      "PressureLevel": 90,
      "RunOrder": 2 
    },
    "NetworkEmulation": {
      "Duration": 15,
      "EmulationType": "Bandwidth",
      "UpstreamSpeed" : 56,
      "DownstreamSpeed" : 33.6,
      "RunOrder": 0,
      "Endpoints": [
        {
          "Port": 443,
          "Hostname": "www.bing.com",
          "Protocol": "ALL"
        },
        {
          "Port": 80,
          "Hostname": "www.msn.com",
          "Protocol": "ALL"
        },
        {
          "Port": 443,
          "Hostname": "www.google.com",
          "Protocol": "ALL"
        }
      ]
    }
  }
}
</pre></code>

All orchestration, repeat, and delay settings are implemented. You can run all the bedlam ops at once (and create true bedlam...) using Concurrent as the Orchestration setting. Sequential runs through the operations according to the RunOrder you provide for each object in the json config file. Random runs the operations in random order, sequentially. You can delay the start of bedlam by setting Delay to some number of seconds. You can repeat the orchestration of bedlam by setting Repeat to some integer value... 

There is more work to do vis a vis network emulation: I need to add support for specifying port ranges, protocols, network layer. This is in flight. However, in it's current form, CloudBedlam is highly useful for enabling chaotic experimentation inside Linux VMs. Please make pull requests if you find bugs or have great ideas for improvement. Thank you!

## Building and Running
<pre><code>
clone...
whomever:~/work/src$ git clone https://github.com/gittorre/CBLinuxN.git
whomever:~/work/src$ cd CBLinuxN
build...(really fast and easy with g++... you can also use cmake, if that is what you need to do...)
g++:
whomever:~/work/src/CBLinuxN$ g++ -std=c++11 CloudBedlamNative.cpp json11.cpp -lpthread -o CBLinuxN
run... (this has run as sudo...)
whomever:~/work/src/CBLinuxN$ sudo ./CBLinuxN
</code></pre>
When running CloudBedlam, a bedlamlogs folder will be created in the folder where the CloudBedlam binary is running. Output file will contain info and error data...

## Deploying
When deploying to a target VM, you must include the <i>CloudBedlam</i> binary (or whatever you name it when you build...), the <i>chaos.json</i> file, and the Bash <b>folder</b> in the same <b>directory</b>. e.g.,:

<b>CloudBedlam</b>  
    ------ &nbsp;&nbsp;&nbsp;&nbsp;<b>Bash</b>  
    ------ &nbsp;&nbsp;&nbsp;&nbsp;<i>CloudBedlam</i>  
    ------ &nbsp;&nbsp;&nbsp;&nbsp;<i>chaos.json</i>  

## Contributing - Please Join In!

Of course, please help make this better 😊 – and add whatever you need around and inside the core bedlam engine (which is what this is, really). The focus for us is on making a *very easy to use, simple to configure, lightweight solution for chaos engineering and experimentation inside virtual machines*.

Have fun and hopefully this proves useful to you in your service resiliency experimentation. It should be clear that this is a development tool at this stage and not a DevOps workflow orchestrator. You should run this in individual VMs to vet the quality of your code in terms of resiliency and fault tolerance. 

# Feedback/Issues

Any and all feedback very welcome. Let us know if you use this and if it helps uncover resiliency/fault tolerance issues in your service implementation. Please <a href="https://github.com/GitTorre/CBLinuxN/issues">submit bugs/requests/feedback</a>. Thank you! This will continue to evolve and your contributions, in whatever form (words or code), will be greatly appreciated!
