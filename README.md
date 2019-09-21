# PortSimulation

The purpose of this application is to simulate the operation of a port by simultaneously running processes. There are three types 
of processes: 1) vessels that want to stay for a finite amount of time in the port, 2) port master that maintains the safe
operation of the port and 3) port monitor that provides the port status. It also provides statistics, with a user-defined periodicity, for the total port income so far, average standby time and income per category and overall for all vessels.
The above types of processes are indepedent programs that can run simultaneously and implement collaboratively
what's happening in the port area.

# Execution

The vessel program can be called as follows:

./vessel -t type -u postype -p parkperiod -m mantime -s shmid

where

• vessel is the executable of the vessel (can be created by the "makefile"),

• -t type flag is the type of boat: S, M and L.

• the -u flag gives the vessel the chance to possibly get a larger parking slot (such as
is quoted by postype) from what would correspond to it and which may not be available.

• The -p parkperiod flag provides the maximum duration for which the vessel will remain docked before leaving for departure. 

• The -m mantime flag provides docking / departure maneuver time.

• The -s shmid flag is the key of the shared memory segment that has structures, semaphores, and any other
auxiliary / variable structures that are required.

The port-master program can be called as follows:

./port-master -c charges -s shmid

where

• port-master is the executable of the port manager,

• the -c flag provides the charges-file which provides the different values for the various
types of ships.

• The -s shmid flag is the key of the shared memory segment that has structures, semaphores, and any other
auxiliary / variable structures that are required.

The supervisor program can be called as follows:

./monitor -d time -t stattimes -s shmid

where

• monitor is the executable,

• The -d time flag indicates the period at which the monitor prints the port status.

• The -t stattimes flag provides the time that the monitor calculates and
provides statistics and the so far generated income.

• The -s shmid flag is the key of the shared memory segment that has structures, semaphores, and any other
auxiliary / variable structures that are required.

The myport program can then be called as follows:

./myport -l configfile

where

• myport is the executable,

• the -l flag configfile procides the config file which provides information on everything to do with identifying
the port, the cost policy and the executables that will be used.
