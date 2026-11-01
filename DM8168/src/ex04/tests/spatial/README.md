# Pure-C Q15 spatial convolution test

This directory is the PC-side reference implementation for the course's
real-time spatial-audio stage. It intentionally has no ALSA, SysLink, DSP, or
third-party audio-library dependency.

The program accepts uncompressed PCM16 WAV only, converts mono (or stereo
downmixed to mono) input into binaural stereo, processes it in blocks, keeps
511 samples of history, and flushes the 511-sample FIR tail for finite files.

## Arithmetic contract

- Sample rate: 44100 Hz
- Input: mono or stereo PCM16 WAV
- HRIR: mono PCM16 WAV, 512 samples
- Default block size: 1024 frames
- Coefficients: natural WAV order, **not pre-reversed**
- Multiply/accumulate: signed Q15 with a 64-bit accumulator
- Final conversion: nearest rounding, halves away from zero, then int16
  saturation

Keeping this contract explicit prevents accidental double reversal of the
HRIR and makes the later DSP port directly testable.

## Files already in this directory

`input.wav` is a valid 44.1-kHz stereo PCM16 WAV (220500 frames / 5 s),
ready for the C renderer. `input_old.wav` preserves the earlier M4A container
with a misleading WAV extension; the C reader correctly rejects that file.

`output_40e045.wav` and `output_60e250.wav` are historical listening samples
(256409 frames), also used by GUI file-loading tests. Their lengths do not
match the current input plus the 511-frame tail; do not use them as exact Q15
references. These audio fixtures live here once; the four compatibility HRIR
fixtures and the original MATLAB listening script live in
`../../assets/hrir/original/`. The full generator source is
`../../assets/hrir/mit_kemar/`.

## Build

On Ubuntu (the course VM is suitable):

```sh
cd /path/to/ex04/tests/spatial
make
```

Equivalent direct command:

```sh
gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic \
  -o spatial_q15 main.c spatial_fir.c wav_pcm16.c
```

On Windows, the same command works in a MinGW-w64/MSYS2 shell. This repository
does not require a particular IDE.

## Test 1: automatic algorithm tests

```sh
make test
```

Or run each pair explicitly:

```sh
./spatial_q15 selftest \
  ../../assets/hrir/original/L40e045a.wav \
  ../../assets/hrir/original/R40e045a.wav

./spatial_q15 selftest \
  ../../assets/hrir/original/L60e250a.wav \
  ../../assets/hrir/original/R60e250a.wav
```

Success requires all of these messages:

```text
selftest PASS: impulse response keeps natural HRIR order
selftest PASS: variable-size blocks equal direct convolution
selftest PASS: 511-sample tail flush is correct
DSP spatial FFT selftest PASS: ../../assets/hrir/original/L40e045a.wav + ../../assets/hrir/original/R40e045a.wav (max error <= 2 LSB)
DSP spatial orbit selftest PASS: variable blocks remain within 3 LSB (max error <= 3 LSB)
DSP spatial FFT selftest PASS: ../../assets/hrir/original/L60e250a.wav + ../../assets/hrir/original/R60e250a.wav (max error <= 2 LSB)
DSP spatial dual selftest PASS: independent L->60e250 and R->40e045 remain within 3 LSB (max error <= 3 LSB)
DSP spatial orbit selftest PASS: 4-second triangular path turns smoothly
```

The impulse test catches a reversed HRIR. The randomized variable-block test
compares the stateful implementation against independent direct convolution,
including every block boundary and the final FIR tail.

## Test 2: create an exact MATLAB Q15 reference

In MATLAB, change to this directory and run:

```matlab
matlab_q15_reference( ...
    'input.wav', ...
    '../../assets/hrir/original/L40e045a.wav', ...
    '../../assets/hrir/original/R40e045a.wav', ...
    'input_pcm16_44100.wav', ...
    'matlab_q15_40e045.wav');
```

This performs any required resampling, creates a real PCM16/44.1-kHz mono
input, and produces a reference using the same Q15 rounding and saturation as
the C implementation. It does **not** apply whole-file normalization.

## Test 3: render in 1024-frame blocks

```sh
./spatial_q15 render \
  input_pcm16_44100.wav \
  ../../assets/hrir/original/L40e045a.wav \
  ../../assets/hrir/original/R40e045a.wav \
  rendered_40e045.wav 1024
```

Expected output properties:

- 44100 Hz
- stereo PCM16
- output frame count = input frame count + 511
- no click or discontinuity at 1024-frame boundaries

Listen with headphones, then compare numerically.

## Test 4: compare C and MATLAB output

```sh
./spatial_q15 compare rendered_40e045.wav matlab_q15_40e045.wav 1
```

The final argument is the allowed difference in PCM least-significant bits.
The expected result is:

```text
compare PASS
max absolute error: 0 or 1 LSB
over tolerance (1): 0
```

Repeat tests 2-4 with `L60e250a.wav` and `R60e250a.wav`.

## Block-size invariance check

The result must not depend on the block size:

```sh
./spatial_q15 render input_pcm16_44100.wav \
  ../../assets/hrir/original/L40e045a.wav \
  ../../assets/hrir/original/R40e045a.wav rendered_511.wav 511

./spatial_q15 render input_pcm16_44100.wav \
  ../../assets/hrir/original/L40e045a.wav \
  ../../assets/hrir/original/R40e045a.wav rendered_1024.wav 1024

./spatial_q15 compare rendered_511.wav rendered_1024.wav 0
```

This must report an exact zero-LSB match.

## DSP integration

The verified algorithm provides generic spatial modes plus four compatibility modes:

- `--algo spatial --elevation E --azimuth A`: exact selection from the generated catalog
- `--algo dual --a-elevation E --a-azimuth A --b-elevation E --b-azimuth A`:
  independent input left/right sources at two selected directions, 0.5 gain each

- `spatial40`: `L40e045a.wav` + `R40e045a.wav`
- `spatial60`: `L60e250a.wav` + `R60e250a.wav`
- `orbit`: a four-second triangular crossfade between `spatial40` and its
  left/right-ear-swapped mirror
- `dual4060`: independent input left at `60e250` plus independent input right
  at `40e045`, mixed with 0.5 gain per source

`make test` also compiles `dsp/dsp_spatial.c` with GCC and compares its
2048-point FFT overlap-add processing against the direct Q15 reference above.
This checks the embedded coefficients, variable block boundaries, overlap
state, rounding, and saturation. The accepted FFT numerical error is at most
2 PCM LSB; orbit and dual4060 allow at most 3 PCM LSB after final mixing.
The direct-Q15 reference tests remain bit exact.

After rebuilding and deploying the ARM/DSP project, test a WAV file with:

```sh
./run.sh file input.wav dsp_40.wav spatial40
./run.sh file input.wav dsp_60.wav spatial60
./run.sh file input.wav dsp_orbit.wav orbit
./run.sh file dual_footsteps.wav dsp_dual.wav dual4060
```

Then test the real-time path with headphones:

```sh
./run.sh audio 10 spatial40
./run.sh audio 10 spatial60
./run.sh audio 10 orbit
./run.sh audio 10 dual4060
```

Spatial modes require stereo PCM16 at 44100 Hz. `spatial40`, `spatial60`, and
`orbit` average each input frame to mono. `dual4060` instead preserves both
input channels: left is source A at `60e250`, and right is source B at
`40e045`. The real-time and file pipelines keep their original frame count and
therefore do not append the
511-frame finite-file tail produced by the offline renderer.

## Complete MIT catalog and protocol tests

From the repository root:

```sh
make hrir-check
make -C tests/spatial test test-catalog
python3 tests/spatial/test_spatial_protocol.py
python3 frontend_arm/tests/test_host_stop.py
```

`test-catalog` checks every original HRIR sample against generated coefficients,
then tests all 710 directions against direct Q15 convolution, variable block
boundaries and a 511-frame tail flush. Representative generic dual tests include
same and different directions, followed by cache-transition and invalid-direction
tests. Override `HRIR_SOURCE` in this directory to change the catalog WAV root
(default `../../assets/hrir/mit_kemar`). The catalog test expects `elevE/LEeAAAa.wav`
and `elevE/REeAAAa.wav`, as in the supplied MIT distribution.

The Python generator tests also exercise malformed/truncated WAVs, missing ears,
duplicates, negative elevations, deterministic output and failed-import preservation.
The protocol test compiles the actual ARM SysLink sender, DSP server, algorithm
adapter and spatial code against minimal TI transport mocks. It checks direction
acknowledgement, invalid/missing configurations and DSP algorithm dispatch, without
claiming to validate TI ABI, shared-memory hardware or real-time performance.
The host tests cover new/legacy CLI parsing, wrapper preflight, propagation of
both direction codes and existing cooperative shutdown.
