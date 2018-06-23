
sim :  sim.o pagetable.o swap.o rand.o clock.o lru.o fifo.o opt.o
	gcc -Wall -g -o sim $^

%.o : %.c pagetable.h sim.h
	gcc -Wall -g -c $<

rand :
	sim -f ./traceprogs/1_trace -m 8 -s 12 -a rand
	sim -f ./traceprogs/4_trace -m 8 -s 12 -a rand

lru :
	sim -f ./traceprogs/1_trace -m 8 -s 12 -a lru
	sim -f ./traceprogs/4_trace -m 8 -s 12 -a lru

fifo :
	sim -f ./traceprogs/1_trace -m 8 -s 12 -a fifo
	sim -f ./traceprogs/4_trace -m 8 -s 12 -a fifo

opt :
	sim -f ./traceprogs/1_trace -m 8 -s 12 -a opt
	sim -f ./traceprogs/4_trace -m 8 -s 12 -a opt

clean : 
	rm -f *.o sim *~
