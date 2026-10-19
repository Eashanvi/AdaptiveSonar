# PC build of the sonar decision + waveform code (same C files the STM32 runs).
# The STM32 binary itself is built in STM32CubeIDE from ../stm32/AdaptiveSonar.
CORE    ?= ../stm32/AdaptiveSonar/Core
CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter \
           -I$(CORE)/Inc -DSONAR_HOST_BUILD=1
LDFLAGS ?= -lm
LIBSRC   = $(CORE)/Src/dds.c $(CORE)/Src/environment.c $(CORE)/Src/adaptation.c

host: build/host_sim

build/host_sim: $(LIBSRC) host_sim.c $(CORE)/Inc/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(LIBSRC) host_sim.c -o $@ $(LDFLAGS)

test: build/host_sim
	./build/host_sim --selftest

run: build/host_sim
	./build/host_sim

plots: run
	python3 ../tools/plot_results.py

wav:
	@mkdir -p build out
	$(CC) $(CFLAGS) $(LIBSRC) make_wav.c -o build/make_wav $(LDFLAGS)
	./build/make_wav

clean:
	rm -rf build out figures

.PHONY: host test run plots wav clean
