test : spp_interface.o spp_LLC.o spp_MAC.o spp_porting.o spp_test.o spp_multiTimer.o spp_connect.o
	gcc -o -std=c99 test spp_interface.o spp_LLC.o spp_MAC.o spp_porting.o spp_test.o spp_multiTimer.o spp_connect.o  -lpthread
spp_test.o : spp_test.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_test.c -lpthread
spp_interface.o : spp_interface.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_interface.c -lpthread
spp_LLC.o : spp_LLC.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_LLC.c -lpthread
spp_MAC.o : spp_MAC.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_MAC.c -lpthread
spp_porting.o : spp_porting.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_porting.c -lpthread
spp_multiTimer.o : spp_multiTimer.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_multiTimer.c -lpthread
spp_connect.o : spp_multiTimer.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c -std=c99 spp_connect.c -lpthread