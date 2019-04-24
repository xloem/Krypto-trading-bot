_all: wrapper_lib

include ../../Makefile

KLOCAL:=$(CURDIR)/../../$(KLOCAL)

LIB:=$(KLOCAL)/lib
TMP:=$(KLOCAL)/tmp
OBJECTS=$(TMP)/bitshares.o $(TMP)/wrapper.o
EXTRA_LIBS=$(patsubst %,$(LIB)/lib%.a,graphene_account_history graphene_chain fc boost_system boost_thread boost_filesystem boost_chrono boost_coroutine boost_date_time boost_context) $(LIB)/cryptonomex/libsecp256k1.a
EXTRA_LIBS=$(patsubst %,$(LIB)/lib%.a,boost_system boost_thread boost_filesystem boost_chrono boost_coroutine boost_date_time boost_context)

wrapper_lib: $(KLOCAL)/lib/K-$(KHOST).wrapper.a

$(TMP)/%.o: %.cxx
	-@mkdir -p $(TMP)
	$(CHOST)-g++ -c -o $@ -DHAVE_STD_UNIQUE_PTR -DUWS_THREADSAFE -static-libstdc++ -static-libgcc -rdynamic $^ -std=c++14 -O0 -ggdb -pthread -DK_0_GIT='"$(shell cat ../../.git/refs/heads/master 2>/dev/null || echo HEAD)"' -DK_STAMP='"$(shell date "+%Y-%m-%d %H:%M:%S")"' -DK_0_DAY='"v$(MAJOR).$(MINOR).$(PATCH)+$(BUILD)"' -DK_BUILD='"$(KHOST)"' -DK_SOURCE='"K-$(KSRC)"' -I$(KLOCAL)/include -I$(realpath ../../src/lib)

$(LIB)/K-$(KHOST).wrapper.a: $(LIB)/K-$(KHOST).$(ABI).a $(OBJECTS)
	count=0 \
	mkdir -p $(TMP)/00 && cd $(TMP) && $(CHOST)-ar x $(LIB)/K-$(KHOST).$(ABI).a K-$(KHOST).$(ABI).o
	objcopy --redefine-sym $$(nm --format=posix --no-demangle $(LIB)/K-$(KHOST).$(ABI).a|grep new_Gw|head -n1|cut -f 1 -d ' ')=new_Gw__wrapped $(TMP)/K-$(KHOST).$(ABI).o
	for lib in $(EXTRA_LIBS); do \
		count=$$((count+1)) subtmp=$(TMP)/$$(printf %02d $$count) && \
		mkdir -p $$subtmp && cd $$subtmp && \
		$(CHOST)-ar x $$lib || exit 1; \
	done
	-rm $@
	$(CHOST)-ar rcs $@ $(OBJECTS) $(TMP)/K-$(KHOST).$(ABI).o $(TMP)/*/*.o
	-rm -rf $(TMP)

.PHONY: wrapper_lib
