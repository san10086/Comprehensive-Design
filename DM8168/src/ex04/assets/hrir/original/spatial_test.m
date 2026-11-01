clear;
clc;
close all;

% 文件名
inputFile = '../../../tests/spatial/input.wav';
leftHrirFile = 'L40e045a.wav';
rightHrirFile = 'R40e045a.wav';
outputFile = '../../../tests/spatial/rendered_matlab_40e045.wav';

% 检查文件是否存在
assert(isfile(inputFile), ...
    '找不到输入音频：%s', inputFile);
assert(isfile(leftHrirFile), ...
    '找不到左耳HRIR：%s', leftHrirFile);
assert(isfile(rightHrirFile), ...
    '找不到右耳HRIR：%s', rightHrirFile);

% 读取输入声源
[x, fs] = audioread(inputFile);

fprintf('输入音频：%s\n', inputFile);
fprintf('输入采样率：%d Hz\n', fs);
fprintf('输入声道数：%d\n', size(x, 2));
fprintf('输入时长：%.2f 秒\n', size(x, 1) / fs);

% 双声道输入转换为单声道声源
if size(x, 2) > 1
    x = mean(x, 2);
end

% HRIR目标采样率
targetFs = 44100;

% 必要时重采样输入音频
if fs ~= targetFs
    fprintf('将输入音频从 %d Hz 重采样到 %d Hz\n', ...
        fs, targetFs);
    x = resample(x, targetFs, fs);
end

% 读取左右耳HRIR
[hL, fsL] = audioread(leftHrirFile);
[hR, fsR] = audioread(rightHrirFile);

fprintf('左耳HRIR：%d Hz，%d点\n', fsL, length(hL));
fprintf('右耳HRIR：%d Hz，%d点\n', fsR, length(hR));

% 检查HRIR
assert(fsL == targetFs, ...
    '左耳HRIR采样率不是44100 Hz');
assert(fsR == targetFs, ...
    '右耳HRIR采样率不是44100 Hz');
assert(size(hL, 2) == 1 && size(hR, 2) == 1, ...
    '左右HRIR必须是单声道');
assert(length(hL) == 512 && length(hR) == 512, ...
    '左右HRIR长度应为512点');

% 双耳卷积
fprintf('开始卷积……\n');

yL = conv(x, hL);
yR = conv(x, hR);

% 合并成双声道
y = [yL, yR];

% 避免输出超过[-1, 1]产生削波
peakValue = max(abs(y(:)));

fprintf('归一化前输出峰值：%.6f\n', peakValue);

if peakValue > 1
    y = y / peakValue;
    fprintf('输出存在溢出风险，已整体缩放。\n');
end

% 保存双声道空间音频
audiowrite(outputFile, y, targetFs, ...
    'BitsPerSample', 16);

fprintf('处理完成：%s\n', outputFile);
fprintf('输出采样率：%d Hz\n', targetFs);
fprintf('输出声道数：2\n');
fprintf('输出时长：%.2f 秒\n', size(y, 1) / targetFs);

% 绘制HRIR
figure;

subplot(2,1,1);
plot(hL);
grid on;
title('Left HRIR - L40e045a');
xlabel('Sample');
ylabel('Amplitude');

subplot(2,1,2);
plot(hR);
grid on;
title('Right HRIR - R40e045a');
xlabel('Sample');
ylabel('Amplitude');

% 播放处理结果
fprintf('开始播放，请佩戴耳机试听。\n');
sound(y, targetFs);