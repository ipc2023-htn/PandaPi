CC ?= gcc

CFLAGS = -O3 -pipe
CFLAGS += -march=native
#CFLAGS += -Wall -pedantic
CFLAGS += -I. -Ishared -Ibfp -Ibfp/bfp_LUSOL -Ibfp/bfp_LUSOL/LUSOL -Icolamd
CFLAGS += -DYY_NEVER_INTERACTIVE
CFLAGS += -DPARSER_LP
CFLAGS += -DINVERSE_ACTIVE=INVERSE_LUSOL
CFLAGS += -DRoleIsExternalInvEngine

OBJS  = lp_MDO.o
OBJS += shared/commonlib.o
OBJS += shared/mmio.o
OBJS += shared/myblas.o
OBJS += ini.o
OBJS += fortify.o
OBJS += colamd/colamd.o
OBJS += lp_rlp.o
OBJS += lp_crash.o
OBJS += bfp/bfp_LUSOL/lp_LUSOL.o
OBJS += bfp/bfp_LUSOL/LUSOL/lusol.o
OBJS += lp_Hash.o
OBJS += lp_lib.o
OBJS += lp_wlp.o
OBJS += lp_matrix.o
OBJS += lp_mipbb.o
OBJS += lp_MPS.o
OBJS += lp_params.o
OBJS += lp_presolve.o
OBJS += lp_price.o
OBJS += lp_pricePSE.o
OBJS += lp_report.o
OBJS += lp_scale.o
OBJS += lp_simplex.o
OBJS += lp_SOS.o
OBJS += lp_utils.o
OBJS += yacc_read.o

TARGETS = liblpsolve.a

all: $(TARGETS)

liblpsolve.a: $(OBJS)
	ar cr $@ $(OBJS)
	ranlib $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS)
	rm -f *.o
	rm -f */*.o
	rm -f */*/*.o
	rm -f */*/*/*.o

.PHONY: all clean check
