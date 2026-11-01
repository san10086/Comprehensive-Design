function matlab_q15_reference(sourceFile, leftHrirFile, rightHrirFile, ...
        monoOutputFile, referenceOutputFile)
%MATLAB_Q15_REFERENCE Prepare PCM16 input and a Q15 convolution reference.
% The arithmetic intentionally matches spatial_fir.c:
%   - PCM and HRIR are quantized to signed Q15
%   - 64-bit-equivalent accumulation is represented exactly as double
%   - round-to-nearest, halves away from zero
%   - saturate to signed 16-bit output

targetFs = 44100;
hrirLength = 512;

[source, sourceFs] = audioread(sourceFile);
if size(source, 2) > 1
    source = mean(source, 2);
end
if sourceFs ~= targetFs
    source = resample(source, targetFs, sourceFs);
end

[leftHrir, leftFs] = audioread(leftHrirFile);
[rightHrir, rightFs] = audioread(rightHrirFile);

assert(size(leftHrir, 2) == 1 && size(rightHrir, 2) == 1, ...
    'HRIR files must be mono');
assert(leftFs == targetFs && rightFs == targetFs, ...
    'HRIR sample rate must be 44100 Hz');
assert(length(leftHrir) == hrirLength && ...
       length(rightHrir) == hrirLength, ...
    'HRIR length must be 512 samples');

sourceQ15 = quantizeQ15(source);
leftQ15 = quantizeQ15(leftHrir);
rightQ15 = quantizeQ15(rightHrir);

% Double represents every integer accumulator here exactly: the maximum
% 512-tap sum is far below 2^53.
leftAccumulator = conv(double(sourceQ15), double(leftQ15));
rightAccumulator = conv(double(sourceQ15), double(rightQ15));

leftOutput = roundAndSaturateQ15(leftAccumulator);
rightOutput = roundAndSaturateQ15(rightAccumulator);
stereoOutput = [leftOutput, rightOutput];

audiowrite(monoOutputFile, double(sourceQ15) / 32768, targetFs, ...
    'BitsPerSample', 16);
audiowrite(referenceOutputFile, double(stereoOutput) / 32768, targetFs, ...
    'BitsPerSample', 16);

fprintf('PCM16 input written: %s (%d frames)\n', ...
    monoOutputFile, length(sourceQ15));
fprintf('Q15 reference written: %s (%d stereo frames)\n', ...
    referenceOutputFile, size(stereoOutput, 1));
end

function q = quantizeQ15(x)
scaled = round(double(x) * 32768);
scaled = min(max(scaled, -32768), 32767);
q = int16(scaled);
end

function q = roundAndSaturateQ15(accumulator)
positive = accumulator >= 0;
scaled = zeros(size(accumulator));
scaled(positive) = floor((accumulator(positive) + 16384) / 32768);
scaled(~positive) = -floor((-accumulator(~positive) + 16384) / 32768);
scaled = min(max(scaled, -32768), 32767);
q = int16(scaled);
end
