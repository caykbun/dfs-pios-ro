## It Takes Three (DFS on Pi-OS)

#### Group: Xiyu Zhang, Yezhen Cong, Yuxin Pei

#### Link to demo video: https://drive.google.com/file/d/1GSCHs5FIDpNyGdP2GYTQUpiJ1eknKIUw/view?usp=sharing

### Overview

Our project is a simple distributed file system inspired by Google File System (GFS). Major components include a simple yet practical distributed protocol, extended FAT32, preemptive threading, robust NRF and UART with interrupts, thread-safe IO/network using locks and syscalls, and a simple shell. 

### Usage
While the system is on, a client should be able to connect to the file system at anytime, exit, and reconnect. A client should be able to change directory, list directory, create/delete directory/file, and read/write file. Multiple users should be able to access a shared folder. If a chunkserver is out of service, other active chunkserver should be able to serve the clients seamlessly. 

### BOM/Components

- **Distributed Protocol**: 
    ![](https://s3.hedgedoc.org/demo/uploads/3a2cf1e3-aa36-48c7-8b00-83acf37a0713.png)
    <p style="text-align: center;">DFS architecture inspired by GFS</p>

    - Master Server: As specified by GFS, each file is split into multiple chunks with their unique chunk ID. The master server stores the file system structure, file metadata, and a mapping from chunk ID to the addresses of 3 replica chunk servers. 
    - Chunk Server: Stores chunks as individual files, with chunk ID as their filenames. When a chunk is created/written to, the (primary) chunk server queries the master server for two secondary chunk servers to store the chunk replica. 
    - Client: The client always follows a transaction-style communication with the file system, namely, querying the master server for chunk handle, talking to the relevant chunk server for the actual chunk data, commiting/declining the operation to the master server. 

- **Heartbeat Protocol**: The master server sends heartbeat to chunk servers periodically, and the active chunk servers reply with current status (e.g. storage cosumption). The master server allows a lag of three heartbeats. If the master server loses contact to a chunkserver for a longer period, it marks the server as down and won't advertise this server to any clients until the contact is regained. We also added LEDs for all the servers to better visualize heartbeating, as shown in the following photo:

![](https://s3.hedgedoc.org/demo/uploads/4eae23c4-ebfa-4d95-a5bc-20613608adf3.jpg)


- **Extended FAT32**: We supported file writing in our FAT32 implementation, and other functionalities including creating files/directories, deleting files/directories, changing current working directory, listing directory contents.


- **Preemptive threading**: We implemented a preemptive threading library using timer interrupts. With carefully designed state saving and restoring assembly code, we are able to switch among interrupt mode, working threads, and the scheduler thread. We automatically resume global system interrupts when exiting from privileged modes by restoring the original cpsr. The following figure is a flow diagram of the threading system:

![](https://s3.hedgedoc.org/demo/uploads/65cbc826-3730-4dd9-8d2d-6182d8d9108d.jpg)


- **Robust UART and NRF with interrupts**: We implemented the interrupt version of UART and NRF. In practice, enabling interrupts for UART output and NRF TX makes the program slower due to the cost of context switching, so we only enabled interrupts for UART input and NRF RX so that we won't miss incoming packets when the current thread is not the thread that handles the packets.


- **Thread-safe IO/network using locks and syscalls**: To avoid data race, we also implemented a very simple lock library and leveraged system calls to make sure SPI and I/O can only be occupied by one thread at one time. When one thread is occupying I/O, for example, other threads that request to use I/O will be blocked.


- **Simple shell**: This shell was built on our thread-safe `scank` and interrupt-based UART. We conformed to Linux user commands, supporting `cd`, `ls`, `mkdir`, `touch`, `cat`, `echo`, and `rm`.
