
# temporary hardcoded makefile to get things going

SOURCES := profile.C profiledata.C totaltime.C machine-profile.S funcsummary.C nautilus-leak-symbol-lookup.C
HEADERS := profile.h profiledata.h totaltime.h symbol-table.h funcsummary.h nautilus-leak-symbol-lookup.h profileP.h machine-profileP.h machine-profile.h

LIBS := -lpthread -lglib -ldl -Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic

libprofiler.so: $(SOURCES) $(HEADERS)
	g++ -g -W -Wall -I/gnome/include -I/gnome/include/glib-1.2 -I/gnome/lib/glib/include -L/gnome/lib $(SOURCES) $(LIBS) -shared -o libprofiler.so

test: test.cpp
	g++ -DTEST_LIB -g -O0 -finstrument-functions test.cpp  -shared -o libtest.so
	g++ -g -O0 -finstrument-functions test.cpp -L. -ltest -lprofiler -lpthread -o test