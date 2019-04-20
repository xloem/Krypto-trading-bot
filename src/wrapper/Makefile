_all: wrapper_lib

include ../../Makefile

KLOCAL:=../../$(KLOCAL)

wrapper_lib: $(KLOCAL)/lib/K-$(KHOST).wrapper.a

$(KLOCAL)/lib/K-$(KHOST).wrapper.%.o: %.cxx
	$(CHOST)-g++ -c -o $@ -DHAVE_STD_UNIQUE_PTR -DUWS_THREADSAFE -static-libstdc++ -static-libgcc -rdynamic $^ -std=c++17 -O0 -ggdb -pthread -DK_0_GIT='"$(shell cat ../../.git/refs/heads/master 2>/dev/null || echo HEAD)"' -DK_STAMP='"$(shell date "+%Y-%m-%d %H:%M:%S")"' -DK_0_DAY='"v$(MAJOR).$(MINOR).$(PATCH)+$(BUILD)"' -DK_BUILD='"$(KHOST)"' -DK_SOURCE='"K-$(KSRC)"' -I$(KLOCAL)/include -I$(realpath ../../src/lib)

%.a: %.bitshares.o %.wrapper.o
	objcopy --redefine-sym $$(nm --format=posix --no-demangle $(KLOCAL)/lib/K-$(KHOST).$(ABI).a|grep new_Gw|head -n1|cut -f 1 -d ' ')=new_Gw__wrapped $(KLOCAL)/lib/K-$(KHOST).$(ABI).a $@
	$(CHOST)-ar rs $@ $(KLOCAL)/lib/K-$(KHOST).wrappee.a $^

.PHONY: wrapper_lib
