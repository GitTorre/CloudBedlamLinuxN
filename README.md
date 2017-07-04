# CBLinuxN
# What? Why? How?

"Chaos Engineering is the discipline of experimenting on a distributed system in order to build confidence in the system’s capability to withstand turbulent conditions in production." 

-From Netflix's Principles of Chaos Engineering Manifesto => http://principlesofchaos.org 

CloudBedlam is a simple, configurable, local chaotic operation orchestrator for resiliency experimentation inside cloud services - chaos, or bedlam as we call it, runs inside individual VMs. At it's core, CloudBedlam causes chaotic conditions by injecting "bedlam" faults (today these are machine resource pressure and network emulation (latency, bandwidth, loss, reorder, disconnection...) into underlying virtual machines that power a service or cluster of services... It is useful for exercising your resiliency design and implementation in an effort to find bugs. It’s also useful for, say, testing your alerting system (e.g., CPU and Memory Azure Alerts) to ensure you set them up correctly or didn’t break them with a new deployment. Network emulation inside the VM enables you to verify that your Internet-facing code handles network faults correctly and/or verify that your solutions to latent or disconnected traffic states work correctly (and help you to refine your design or even establish for the first time how you react to and recover from transient networking problems in the cloud…).

Unlike, say, Netflix's ChaosMonkey, shooting down VM instances isn't the interesting chaos we create with CloudBedlam (though it is definitely interesting chaos – just different from what CloudBedlam is designed to help you experiment with…). Instead, we want to add conditions to an otherwise happy VM that make it sad and troubled, turbulent and angry - not just killing it. We believe that there is a useful difference between a VM that is running in a configurably chaotic state versus a VM that suddenly disappears from the map...

This is meant to run chaos experiments <i>inside</i> VMs as a way to experiment close to your code and help you identify resiliency bugs in your design and implementation.

### Easy to use 

Step 0.

Just change JSON settings to meet your specific chaotic needs. The default config will run CPU, Memory and Networking chaos. You can remove the CPU and Memory objects and just do Network emulation or remove Network and just do CPU/Mem. It's configurable, so do what you want! 

Currently supported IPv4/IPV6 network emulation operations:

Packet Corruption  
Packet Loss  
Packet Reordering  
Bandwidth Rate Limiting  
Latency  

For example, the JSON configuration below sequentially runs (according to specified run order) a CPU pressure fault of 90% CPU utilization across all CPUs for 15 seconds, Memory pressure fault eating 90% of available memory for 15 seconds, and Network Bandwidth emulation of 56kbits for 30 seconds for specified target endpoints. The experiment runs 2 times successively (Repeat=”1”). See [TODO] for more info on available configuration settings, including samples. CloudBedlam will execute (and log) the orchestration of these bedlam operations. You just need to modify some JSON and then experiment away. Enjoy!
<pre><code>
{
  "ChaosConfiguration": {
    "Orchestration": "Sequential",
    "Duration": 60,
    "RunDelay": 0,
    "Repeat": 1,
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
      "BandwidthDownstreamSpeed" : 56,
      "RunOrder": 0,
      "TargetEndpoints": {
        "Endpoint": [
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
}
</pre></code>
## Building and Running
<pre><code>
clone...
whomever:~/work/src$ git clone https://github.com/gittorre/CBLinuxN.git
whomever:~/work/src$ cd CBLinuxN
build...(really fast and easy with gcc... you can also use cmake...)
whomever:~/work/src/CBLinuxN$ g++ -std=c++11 CloudBedlamNative.cpp json11.cpp -lpthread -o CBLinuxN
run... (this has run as sudo...)
whomever:~/work/src/CBLinuxN$ sudo ./CBLinuxN
</code></pre>
When running CloudBedlam, a bedlamlogs folder will be created in the folder where the CloudBedlam binary is running. Output file will contain INFO and ERROR data (ERROR info will include error messages and stack traces).

## Contributing

Of course, please help make this better 😊 – and add whatever you need around and inside the core bedlam engine (which is what this is, really). The focus for us is on making a *very easy to use, simple to configure, lightweight solution for chaos engineering and experimentation inside virtual machines*.

Have fun and hopefully this proves useful to you in your service resiliency experimentation. It should be clear that this is a development tool at this stage and not a DevOps workflow orchestrator. You should run this in individual VMs to vet the quality of your code in terms of resiliency and fault tolerance. 

## Feedback

Any and all feedback very welcome. Let us know if you use this and if it helps uncover resiliency/fault tolerance issues in your service implementation. You can send mail to ctorre@microsoft.com and/or <a href="https://github.com/GitTorre/CloudBedlamLinux/issues">create Issues here</a>. Thank you! This will continue to evolve and your contributions, in whatever form (words or code), will be greatly appreciated!


Make bedlam, not war!

--CloudBedlam Team
