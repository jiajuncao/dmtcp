dmtcp_psm.so: psmwrappers.o psm.o
	g++ -fPIC -g3 -O0 -o dmtcp_psm.so psmwrappers.o psm.o -shared -Wl,--unresolved-symbols=ignore-all

psmwrappers.o: psmwrappers.cpp psmwrappers.h psminternal.h psmutil.h
	g++ -g3 -O0 -fPIC -I. -I../../include -I../../jalib psmwrappers.cpp -c -o psmwrappers.o

psm.o: psm.cpp psminternal.h psmwrappers.h psmutil.h
	g++ -g3 -O0 -fPIC -I. -I../../include -I../../jalib psm.cpp -c -o psm.o

clean:
	rm -f *.o *.so
