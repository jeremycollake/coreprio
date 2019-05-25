# coreprio
Coreprio - CPU optimization utility for the AMD 2990wx

Coreprio is a Windows freeware CPU optimization utility for the AMD 2990wx and 2970wx processors. It offers two features: (1) our implementation of AMD’s Dynamic Local Mode and (2) our new ‘NUMA Dissociater’.

Our DLM is more configurable and robust than AMD’s implementation, allowing the user to set the prioritized CPU affinity, (software) thread count, refresh rate, and which processes to include/exclude.

Dynamic Local Mode (DLM) was conceived for the AMD Threadripper 2990wx and 2970wx CPUs, addressing their asymmetric die performance. There are potentially other use cases.

DLM works by dynamically migrating the most active software threads to the prioritized CPU cores. The Windows CPU scheduler is free to choose specifically where within that set of CPUs to assign threads.

Since no hard CPU affinity is set, applications are still free to expand across the entire CPU. For this reason, we call it a prioritized soft CPU affinity.

DLM is designed for loads that don’t max out all available CPU cores. Applications that put a full load across the entire CPU could perform marginally worse. This is because when the entire CPU is loaded, there are no CPU cores to prioritize threads to since all must be utilized. Thus it may be ideal to exclude some highly multi-threaded applications.

Coreprio also offers an experimental feature named ‘NUMA Dissociater‘. The NUMA Dissociater is confirmed to work on EPYC 7551 and TR 2970/2990, but may work on other HCC NUMA platforms.

You can see details of Coreprio’s thread management by using the console utility or an elevated instance of DebugView with ‘Capture Global Win32‘ enabled.
