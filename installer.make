#!/usr/bin/make
# Makes the distribution and source files

.SILENT :

Name = ihex
Bin = $(Name)-linux
Src = $(Name)-src
LgiPath = ../Lgi
Build = Release
Gui = X


installer :
	# init
	-mkdir $(Bin)
	-rm $(Bin)/* > /dev/null 2> /dev/null
	-mkdir $(Bin)/Help
	-rm $(Bin)/Help/* > /dev/null 2> /dev/null
	# copy files
	cp ihex $(Bin)
	cp $(LgiPath)/$(Build)$(Gui)/liblgi*.so $(Bin)
	cp $(LgiPath)/Gel/$(Build)$(Gui)/liblgiskin*.so $(Bin)
	cp Code/ihex.lr8 $(Bin)
	cp Code/*.gif $(Bin)
	cp Code/*.png $(Bin)
	cp Help/* $(Bin)/Help
	# archive
	-rm $(Bin).* 2> /dev/null
	tar -cf $(Bin).tar $(Bin)/*
	bzip2 -z $(Bin).tar 
	ls -l $(Bin).tar.bz2
	# clean up
	rm -rf $(Bin)
	
source :
	# init
	-mkdir $(Src)
	-rm $(Src)/* > /dev/null 2> /dev/null
	-mkdir $(Src)/Code
	-rm $(Src)/Code/* > /dev/null 2> /dev/null
	-mkdir $(Src)/Help
	-rm $(Src)/Help/* > /dev/null 2> /dev/null
	# copy
	cp *.ds* $(Src)
	cp *.nsi $(Src)
	cp Installer.make $(Src)
	cp Makefile $(Src)
	cp Code/* $(Src)/Code
	cp Help/* $(Src)/Help
	# archive
	-rm $(Src).* 2> /dev/null
	tar -cf $(Src).tar $(Src)/*
	bzip2 -z $(Src).tar 
	ls -l $(Src).tar.bz2
	# clean up
	rm -rf $(Src)
	
	