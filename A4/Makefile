

all :  util.o ext2_mkdir.o
	gcc -Wall -g -o ext2_mkdir $^

%.o : %.c ext2.h util.h
	gcc -Wall -g -c $<
	
clean : 
	rm -f *.o ext2_mkdir *~