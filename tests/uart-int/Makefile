input-test: 2-input-test-1-echo.check 2-input-test-0-small-input.check

2-input-test-1-echo.check: 2-input-test-1-echo.bin
	my-install $< < test-input/trivial.c  > trivial.tmp  2>&1 
	@grep "[0-9][0-9][0-9][0-9]:" trivial.tmp | grep -v BOOT: > trivial.test
	@diff trivial.test test-input/trivial.c
	
	my-install $< < test-input/switch.c  > switch.tmp  2>&1 
	@grep "[0-9][0-9][0-9][0-9]:" switch.tmp | grep -v BOOT: > switch.test
	@diff switch.test test-input/switch.c
	
	@echo "about to echo the first 300 lines of unix source: will take awhile";
	my-install $< < test-input/300-unix-source.c  > unix-source.tmp  2>&1 
	@grep "[0-9][0-9][0-9][0-9]:" unix-source.tmp | grep -v BOOT: > unix-source.test
	@diff -Bb unix-source.test test-input/300-unix-source.c
	@rm -f *.tmp

2-input-test-0-small-input.check: 2-input-test-0-small-input.bin 
	my-install $< < input-hello.txt