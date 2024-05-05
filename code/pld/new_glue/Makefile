all: xosera_glue.vcd

xosera_glue.vcd: xosera_glue.jed
	ruby vis.rb xosera_glue

xosera_glue.jed: xosera_glue.pld
	./5vcomp $<

clean:
	rm -f *.pin *.fit *.io *.jed *.pla *.tt2 *.tt3 *.abs *.so *.sim *.wo *.vcd

.PHONY: clean all

