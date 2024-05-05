all: xosera_glue.jed

xosera_glue.jed: xosera_glue.pld
	./5vcomp $<

clean:
	rm -f *.pin *.fit *.io *.jed *.pla *.tt2 *.tt3 *.abs *.so *.sim *.wo

.PHONY: clean all

