myport: myport.c port-master.c vessel.c monitor.c s_header.h
	gcc myport.c -o myport -lpthread
	gcc port-master.c -o port-master -lpthread
	gcc vessel.c -o vessel -lpthread
	gcc monitor.c -o monitor -lpthread
