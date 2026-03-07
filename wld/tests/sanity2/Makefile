all: run-sanity2ab run-sanity2ba

%.o: %.s
	${AS} $< -o $@

sanity2ab: ${WLD} sanity2a.o sanity2b.o
	${WLD} sanity2a.o sanity2b.o -o sanity2ab -static

sanity2ba: ${WLD} sanity2a.o sanity2b.o
	${WLD} sanity2b.o sanity2a.o -o sanity2ba -static

run-sanity2ab: sanity2ab
	@MSG=$$(./sanity2ab); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ab test passed"

run-sanity2ba: sanity2ba
	@MSG=$$(./sanity2ba); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ba test passed"

clean:
	@rm -f *.o
	@rm -f sanity2ab
	@rm -f sanity2ba
