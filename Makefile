# Makefile
# For CS194-2 fall 2007, Homework 1 (Monte Carlo)
#
# Revised (mfh 08 Sep 2007) so that libdcmt is a dynamic library.
#####################################################################

# Include platform-dependent settings.
#
include Makefile.include

#
# NOTE: This only works for GNU Make.  On NERSC IBM machines, you'll
# need to load the "GNU" module and use "gmake" instead of "make".
# Please refer to the following webpage for more info:
#
# http://www.nersc.gov/nusers/systems/bassi/software/
#

LDFLAGS += -Ldcmt0.4/lib -ldcmt
CFLAGS += -DUSE_MERSENNE_TWISTER

HW1_INCS = black_scholes.h gaussian.h mock_gaussian.h parser.h random.h timer.h util.h
HW1_C_SRCS = black_scholes.c mock_gaussian.c gaussian.c main.c parser.c random.c timer.c util.c
HW1_C_OBJS = $(HW1_C_SRCS:.c=.o)
HW1_EXE = hw1.x
PARAM = params.txt


all: hw1.x

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

hw1.x: $(HW1_C_OBJS) dcmt0.4/lib/libdcmt.$(DYLIB_SUFFIX)
	$(LINKER) $(CFLAGS) $(HW1_C_OBJS) -o $@ $(LDFLAGS)

dcmt0.4/lib/libdcmt.$(DYLIB_SUFFIX):
	make -C dcmt0.4/lib

black_scholes.o: black_scholes.c black_scholes.h gaussian.h mock_gaussian.h random.h timer.h util.h

gaussian.o: gaussian.c gaussian.h util.h

mock_gaussian.o : mock_gaussian.c mock_gaussian.h

main.o: main.c black_scholes.h parser.h random.h timer.h

parser.o: parser.c parser.h

random.o: random.c random.h

timer.o: timer.c timer.h

util.o: util.c util.h

clean:
	make -C dcmt0.4/lib clean
	rm -f $(HW1_C_OBJS) $(HW1_EXE)

seq_run:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 1 0; \
	done

seq_corr_only1:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 1 1; \
	done

seq_corr_fixed:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 1 2; \
	done

mpi_run:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 2 0; \
	done

mpi_corr_only1:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 2 1; \
	done

mpi_corr_fixed:
	for number in 256 4096 65536 17367040; do \
		./$(HW1_EXE) $(PARAM) $$number 2 2; \
	done
