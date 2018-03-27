# proof-of-work-solver
A server-based proof-of-work solver for the Hashcash mining algorithm used in Bitcoin.
 
The aim of this project is to create a program that interacts with other programs over a network (socket programming), and multi-threaded applications. Speciﬁcally, a server-based proof-of-work solver for the Hashcash mining algorithm used in Bitcoin. The functions are implemented in the Simple Stratum Text Protocol (SSTP), which is used to format messages between clients and server program.

It is important to note, that in general, it is very diﬃcult to produce a valid proof-of-work solution of the mining process as there are 2^256 possible outputs for some random input. Therefore, this is a problem that should be solved with some means of multi-threading, which enables us to distribute the workload.
