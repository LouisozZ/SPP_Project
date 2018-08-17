test : spp_interface.o spp_LLC.o spp_MAC.o spp_porting.o spp_test.o spp_multiTimer.o spp_connect.o
	gcc -o test spp_interface.o spp_LLC.o spp_MAC.o spp_porting.o spp_test.o spp_multiTimer.o spp_connect.o
spp_test.o : spp_test.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_test.c
spp_interface.o : spp_interface.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_interface.c
spp_LLC.o : spp_LLC.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_LLC.c
spp_MAC.o : spp_MAC.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_MAC.c
spp_porting.o : spp_porting.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_porting.c
spp_multiTimer.o : spp_multiTimer.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_multiTimer.c
spp_connect.o : spp_multiTimer.c spp_def.h spp_global.h spp_interface.h spp_LLC.h spp_MAC.h spp_MAC.h spp_porting.h spp_multiTimer.h spp_connect.h
	gcc -c spp_connect.c