# makefile for testing and benchmarking the bch library

$(ARCH)_XCC	:= $(CROSS)gcc
$(ARCH)_XSTRIP	:= $(CROSS)strip
$(ARCH)_XPROG	:= $(ARCH)_tu
$(ARCH)_XCFLAGS	:= $(XCFLAGS)
$(ARCH)_XSHELL	:= $(XSHELL)
$(ARCH)_XRUN	:= $(XRUN)

XPROG	:= $(ARCH)_tu
BINS	:= tool gf mem unaligned correct poly4
BINS	+= bench_dyn bench_m13t4 bench_m13t8 bench_m13t4c bench_m13t8c
SCRIPTS := bench.sh short.sh medium.sh long.sh
XPROGS	:= $(addprefix $(XPROG)_,$(BINS))
XSCRIPTS:= $(addprefix $(XPROG)_,$(SCRIPTS))

PROGS	+= $(XPROGS) $(XSCRIPTS)

$(XPROG)_bench_%: arch := $(ARCH)
$(XPROG)_bench_%: tu_bench.c $(SRC) $(HEADER)
	$($(arch)_XCC) $($(arch)_XCFLAGS) $(SRC) $< -lrt -lm -o $@
	$($(arch)_XSTRIP) $@

$(XPROG)_%: arch := $(ARCH)
$(XPROG)_%: tu_%.c $(SRC) $(HEADER)
	$($(arch)_XCC) $($(arch)_XCFLAGS) $< -o $@
	$($(arch)_XSTRIP) $@

$(XPROG)_bench_m13t4:  $(ARCH)_XCFLAGS += $(M13T4)
$(XPROG)_bench_m13t4c: $(ARCH)_XCFLAGS += $(M13T4) $(CHIEN)
$(XPROG)_bench_m13t8:  $(ARCH)_XCFLAGS += $(M13T8)
$(XPROG)_bench_m13t8c: $(ARCH)_XCFLAGS += $(M13T8) $(CHIEN)

$(XPROG)_%.sh: arch := $(ARCH)
$(XPROG)_%.sh: tu_%.sh.template $(XPROGS)
	@sed \
	-e 's|@XSHELL|$($(arch)_XSHELL)|' \
	-e 's|@XPROG|$($(arch)_XPROG)|' \
	-e 's|@XRUN|$($(arch)_XRUN)|' > $@ < $<
	@chmod a+x $@

$(ARCH): $(XPROGS) $(XSCRIPTS)
