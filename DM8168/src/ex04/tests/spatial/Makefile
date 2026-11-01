CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic

TARGET = spatial_q15
SOURCES = main.c spatial_fir.c wav_pcm16.c
HEADERS = spatial_fir.h wav_pcm16.h
DSP_TEST = dsp_spatial_selftest
DSP_TEST_SOURCES = dsp_spatial_selftest.c spatial_fir.c wav_pcm16.c \
	../../dsp/dsp_spatial.c
DSP_TEST_HEADERS = spatial_fir.h wav_pcm16.h ../../dsp/dsp_spatial.h \
	../../shared/hrir_catalog.h \
	../../dsp/dsp_spatial_coefficients.inc \
	../../dsp/dsp_spatial_fft_twiddles.inc
HRIR_DIR = ../../assets/hrir/original
HRIR_SOURCE ?= ../../assets/hrir/mit_kemar
PYTHON ?= python3

.PHONY: all test test-catalog clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

$(DSP_TEST): $(DSP_TEST_SOURCES) $(DSP_TEST_HEADERS)
	$(CC) $(CFLAGS) -o $@ $(DSP_TEST_SOURCES)

test: $(TARGET) $(DSP_TEST)
	./$(TARGET) selftest \
		$(HRIR_DIR)/L40e045a.wav $(HRIR_DIR)/R40e045a.wav
	./$(TARGET) selftest \
		$(HRIR_DIR)/L60e250a.wav $(HRIR_DIR)/R60e250a.wav
	./$(DSP_TEST) \
		$(HRIR_DIR)/L40e045a.wav $(HRIR_DIR)/R40e045a.wav \
		$(HRIR_DIR)/L60e250a.wav $(HRIR_DIR)/R60e250a.wav

test-catalog: $(DSP_TEST)
	$(PYTHON) test_hrir_generator.py
	./$(DSP_TEST) --catalog "$(HRIR_SOURCE)"
	$(PYTHON) test_spatial_protocol.py

clean:
	rm -f $(TARGET) spatial_q15.exe $(DSP_TEST) $(DSP_TEST).exe \
		rendered_*.wav input_pcm16_44100.wav \
		matlab_q15_*.wav
