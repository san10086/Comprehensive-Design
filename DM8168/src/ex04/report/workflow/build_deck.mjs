import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
process.on('uncaughtException',e=>{console.error('BUILD_ERROR:',e.message);console.error(e.stack?.split('\n').slice(-5).join('\n'));process.exit(1);});

const ROOT='D:/develop/vscode/projects/ex04';
const OUT=ROOT+'/report/workflow';
const p=await PresentationFile.importPptx(await FileBlob.load(ROOT+'/report/template.pptx'));
const snap=await p.inspect({kind:'slide,shape,textbox,image,chart,table',maxChars:1000000});
await fs.writeFile(OUT+'/source-elements.ndjson',snap.ndjson);
const records=snap.ndjson.split('\n').filter(Boolean).map(l=>JSON.parse(l));
const source=p.slides.items.slice();
const slideRecords=records.filter(r=>r.kind==='slide');
const order=[0,1,2,10,12,3,6,13,15];
p.slides.keep(order);
p.slides.reorder(order.map(i=>slideRecords.find(r=>r.slideIndex===i).id));
const BLUE='#2E568C', GREEN='#16BF84', GREY='#545454', LIGHT='#EDF3F8';
const HEAVY='思源黑体 1 Heavy', BLACK='思源黑体-超粗体', BODY='思源黑体 2';
const deleted=new Set();
const H=(s,name,text,x,y,w,h,role='sub',color=BLUE)=>{
  const sizes={header:56,sub:48,cover:104*4/3,coverSub:63.99*4/3,toc:48*4/3,tocTitle:87.99*4/3,chapter:144,number:188*4/3};
  const a=s.shapes.add({name:'report.'+role+'.'+name,geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
  a.text=text;a.text.style={fontSize:sizes[role],typeface:['cover','toc','tocTitle'].includes(role)?BLACK:HEAVY,bold:!['cover','toc','tocTitle'].includes(role),color,wrap:'square',autoFit:'none',insets:0,verticalAlignment:'middle'};
  return a;
};
const T=(s,name,text,x,y,w,h,size=28,color=GREY,mono=false)=>{
  const a=s.shapes.add({name:'report.body.'+name,geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
  a.text=text;a.text.style={fontSize:size,typeface:BODY,color,wrap:'square',autoFit:'none',insets:0,verticalAlignment:'top',lineSpacing:1.18};
  return a;
};
const box=(s,name,x,y,w,h,fill=LIGHT,line='none')=>s.shapes.add({name:'report.box.'+name,geometry:'rect',position:{left:x,top:y,width:w,height:h},fill,line:{fill:line,width:line==='none'?0:1.4}});
const line=(s,name,x,y,w,color='#BDD0E0')=>s.shapes.add({name:'report.line.'+name,geometry:'line',position:{left:x,top:y,width:w,height:0},fill:'none',line:{fill:color,width:1.4}});
function remove(index,predicate){
  const rr=records.filter(r=>r.slideIndex===index&&r.position&&['shape','textbox','image','chart','table'].includes(r.kind)&&predicate(r));
  const ids=[...new Set(rr.map(r=>r.id))].filter(id=>!deleted.has(id));if(ids.length){p.delete(ids);ids.forEach(id=>deleted.add(id));}
}
function body(index,keepMotifs=false){
  remove(index,r=>{
    const a=r.position;
    if(a.left>1600&&a.top<145)return true; // Canva wordmark
    if(a.top>=145&&a.top<1015){
      return !keepMotifs||r.kind==='textbox'||r.kind==='image'||r.kind==='chart'||r.kind==='table';
    }
    return false;
  });
  // Replace the source title and the part footer with exact template typography.
  remove(index,r=>r.kind==='textbox'&&((r.position.top<140)||(r.position.top>1015)));
}
function head(s,text){H(s,'header',text,82.4,49.7,1640,70,'header');T(s,'part','PART ONE',1647,1036,191,32,24,'#FFFFFF');}
function note(s,text,refs=[]){s.speakerNotes.text=text+'\n\n资料依据：\n'+refs.map(x=>ROOT+'/'+x).join('\n');}
const slides=order.map(i=>source[i]);

// 1. Cover, preserving the template's architectural background and title scale.
remove(0,r=>r.kind==='textbox'||(r.position.left>1600&&r.position.top<145));
remove(0,r=>(r.position.top>260&&r.position.top<310)||(r.geometry==='line'&&r.position.top>600&&r.position.top<670));
H(slides[0],'main','中期汇报',127,300,900,187,'cover');
H(slides[0],'secondary','空间音效处理系统',171,498,1050,114,'coverSub');
T(slides[0],'strap','综合设计 III',82,73,600,40,24,BLUE);
T(slides[0],'en','MIDTERM REPORT',171,610,650,42,30,GREEN);
T(slides[0],'scope','空间音效处理算法研究与实现',171,666,1000,50,29,BLUE);
T(slides[0],'period','阶段：中期汇报',190,731,290,44,26,'#FFFFFF');
T(slides[0],'date','2026 年 11 月',495,731,330,44,26,BLUE);
note(slides[0],'本次中期汇报围绕空间音效处理系统展开，整体分为算法、Qt 图形交互开发和不足与后续工作三章。本文件先完成第一章，介绍空间音效处理算法的研究过程、实现与验证。');

// 2. Overall contents: the source 6-item grid becomes a 3-row outline.
body(1);remove(1,r=>r.position.top>=145&&r.position.top<1015);
H(slides[1],'contents','目录',108,228,460,145,'tocTitle');
T(slides[1],'contentsEn','CONTENTS',116,402,420,50,32,BLUE);
const toc=[['01.','空间音效处理算法\n研究与实现'],['02.','基于 QT 的 GUI 图形交互\n应用开发上手'],['03.','不足与后续工作']];
toc.forEach((r,i)=>{const y=225+i*245;T(slides[1],'tocNum'+i,r[0],620,y+16,110,64,48,BLUE);H(slides[1],'toc'+i,r[1],770,y,1070,i===2?100:180,'toc');if(i<2)line(slides[1],'tocDivider'+i,770,y+205,1000);});
note(slides[1],'汇报分为三章。第一章介绍空间音效处理算法的研究与实现。第二章为基于 Qt 的 GUI 图形交互应用开发上手，第三章为不足与后续工作。下面进入第一章。');

// 3. Chapter opening plus its concise contents; source dark divider style.
body(2);remove(2,r=>r.position.top>=145&&r.position.top<1015);
H(slides[2],'chapterNumber','01.',230,380,460,320,'number','#7192BB');
box(slides[2],'chapterBorder',180,345,540,420,'none','#7192BB');
H(slides[2],'chapterTitle','空间音效\n处理算法',850,208,1000,340,'chapter','#FFFFFF');
H(slides[2],'chapterGreen','研究与实现',850,560,1000,178,'chapter',GREEN);
T(slides[2],'chapterEn','SPATIAL AUDIO PROCESSING',858,770,910,50,27,'#FFFFFF');
const chips=['研究过程与算法选型','MIT KEMAR 与方向参数','功能演示与验证'];
chips.forEach((t,i)=>{const x=854+i*320;box(slides[2],'chip'+i,x,867,299,64,'none','#89A4C7');T(slides[2],'chipText'+i,t,x+15,882,275,40,24,'#FFFFFF');});
note(slides[2],'第一章分三部分介绍。先说明从 MATLAB 原型到 C 参考和 DSP 分块实现的开发过程，再介绍 MIT KEMAR 系数库导入和方向参数的接通，最后展示功能命令与验证结果。');

// 4. Development, source slide 11's four-step timeline is retained.
body(10,true);head(slides[3],'1.1 研究目标与开发过程');
T(slides[3],'goal','目标：通过左右耳 HRIR 卷积，为输入音频加入可选方向的双耳线索',132,200,1640,70,34,BLUE);
const stages=[['MATLAB','原理复现','读取声源与 HRIR\n分别卷积，合成立体声\n准备耳机试听结果'],['C / Q15','数值基准','统一舍入与饱和规则\n保留 511 点历史\n建立直接卷积参考'],['DSP','工程集成','2048 点 FFT 重叠相加\nAudioAlgo 统一入口\n接入 ARM / SysLink'],['KEMAR','方位库扩展','两组响应扩展至 710 方向\n贯通仰角与方位角\n支持双声源独立配置']];
stages.forEach((r,i)=>{const x=135+i*437;const label=H(slides[3],'step'+i,r[0],152+i*437.2,330,275,65,'sub');label.text.alignment='center';H(slides[3],'stage'+i,r[1],x,613,330,64,'sub');T(slides[3],'stageBody'+i,r[2],x,704,360,170,27);});
T(slides[3],'formula','双耳渲染：yL = x ∗ hL       yR = x ∗ hR',135,916,1600,55,34,BLUE);
note(slides[3],'研究工作先围绕双耳卷积建立 MATLAB 原型，再用纯 C 定点版本明确数值规则，最后以 FFT 分块实现接入 DSP。原有工程只使用两组固定响应，本阶段进一步导入 MIT KEMAR 方位库，让方向选择通过参数进入算法，并扩展双声源的独立方向配置。这里按实现用途组织开发阶段；Q15 基准与 FFT 模块在同一空间算法提交中加入。',['assets/hrir/original/spatial_test.m','tests/spatial/spatial_fir.c','dsp/dsp_spatial.c','tools/generate_hrir.py']);

// 5. Catalog and parameter path, source slide 13's wide-left / facts-right layout.
body(12);head(slides[4],'1.2 MIT KEMAR 系数导入与参数贯通');
H(slides[4],'catalog','原始系数自动导入',160,230,870,68,'sub');
T(slides[4],'format','44.1 kHz · 单声道 · PCM16 · 512 点\n保留采样顺序与幅值，排除 11 个非方向 WAV',160,319,1010,100,29);
const nodes=[['原始 WAV','左右耳配对'],['格式校验','重复 / 缺耳检查'],['方向目录','ARM / DSP 共用']];
nodes.forEach((r,i)=>{let x=160+i*350;box(slides[4],'flow'+i,x,478,310,145,i===2?BLUE:LIGHT);T(slides[4],'flowName'+i,r[0],x+20,497,275,50,34,i===2?'#FFFFFF':BLUE);T(slides[4],'flowDesc'+i,r[1],x+20,560,275,42,24,i===2?'#FFFFFF':GREY);if(i<2)T(slides[4],'arrow'+i,'›',x+317,522,36,60,54,BLUE);});
T(slides[4],'path','角度参数  /  ARM 校验  /  SysLink + ACK  /  DSP 选系数',160,680,1100,55,29,BLUE);
T(slides[4],'importCmd','make hrir\nmake hrir-check\n./run.sh directions',160,778,1040,140,29,BLUE,true);
[['710','离散方向，仰角 −40° 至 90°'],['1420','方向 HRIR WAV，左右耳成对'],['1.39 MiB','原始 PCM16 系数，频谱按需缓存']].forEach((r,i)=>{const y=430+i*175;H(slides[4],'fact'+i,r[0],1320,y,500,70,'sub');T(slides[4],'factBody'+i,r[1],1320,y+82,485,65,27);});
T(slides[4],'exact','方向精确匹配；一次运行内固定；ARM 与 DSP 须成套部署',160,949,1650,45,25,GREY);
note(slides[4],'本地目录 temp/MIT KERMER 中有 1420 个方向 HRIR WAV，配成 710 组双耳响应。生成器检查采样率、声道、PCM16 和 512 点长度，同时检查缺耳、重复与非法角度，保留原始系数幅度和顺序。ARM 只保留方向目录，DSP 保存原始系数并缓存当前方向的频谱。仰角和方位角经 ARM 校验、SysLink 方向确认后进入算法。当前精确匹配已存在的离散点，不做近邻或插值。\n\n参数预检查命令：\n./run.sh audio --seconds 10 --algo spatial --elevation -40 --azimuth 45 --check\n该命令不加载 DSP。当前新角度通过 CLI 配置，Qt 仍使用兼容模式。',['tools/generate_hrir.py','shared/hrir_catalog.h','host/syslink/syslinkDsp.c','dsp/Server.c','docs/evidence/spatial-kemar-update-2026-11-01/catalog-summary.json']);

// 6. Trial-and-choice, source slide 4's stacked narrative / right illustration.
body(3);head(slides[5],'1.3 试错与算法选型');
const pitfalls=[['输入格式','旧 WAV 扩展名实际为 M4A，统一为真实 PCM16 WAV'],['数值对照','试听输出可能归一化，另建统一舍入与饱和的 Q15 参考'],['连续处理','保留 511 点状态，短块保留旧尾部，输入读完再写回'],['方位库扩展','自动配对与目录生成，稳定编码，拒绝不存在的方向']];
pitfalls.forEach((r,i)=>{const y=230+i*183;H(slides[5],'pitfall'+i,r[0],134,y,800,64,'sub');T(slides[5],'pitBody'+i,r[1],134,y+76,800,84,28);if(i<3)line(slides[5],'pitLine'+i,134,y+167,800);});
H(slides[5],'fftTitle','2048 点 FFT',1090,245,690,75,'sub');
T(slides[5],'fftEquation','1024 + 512 − 1\n= 1535 点',1090,360,690,170,60,BLUE);
T(slides[5],'fftReason','选择满足线性卷积长度的二次幂\n复用输入频谱与已准备的 HRIR 频谱',1090,575,690,115,29);
box(slides[5],'choice',1070,733,710,180,BLUE);
H(slides[5],'choiceWhite','Q15 基准  /  FFT 部署',1100,756,650,64,'sub','#FFFFFF');
T(slides[5],'choiceBody','保留直接卷积对照，DSP 采用浮点重叠相加',1100,838,650,57,27,'#FFFFFF');
note(slides[5],'试错主要集中在数据格式、数值规则和块间连续性。当前 input.wav 已经是标准 PCM16 WAV，旧格式对应 input_old.wav。MATLAB 试听脚本可能整体缩放，因此另建统一量化规则的参考。卷积使用自然顺序 HRIR，与 x[n-k]h[k] 的索引配套。FFT 长度由输入块和滤波器长度决定：最大 1024 帧和 512 点 HRIR 需要 1535 点线性卷积，选用 2048 点 FFT。实际加速幅度与实时预算仍需板端计时；块时长 23.22 ms 不作为端到端延迟。',['tests/spatial/matlab_q15_reference.m','tests/spatial/spatial_fir.c','dsp/dsp_spatial.h','dsp/dsp_spatial.c']);

// 7. Single-source and moving effect, source slide 7's large-left / two-right layout.
body(6);head(slides[6],'1.4 指定方向与动态过渡');
H(slides[6],'file','指定方向文件渲染',130,233,850,70,'sub');
T(slides[6],'fileDesc','左右输入平均为单声源，选择仰角与方位角',130,328,850,78,29);
box(slides[6],'fileCommand',130,451,860,349,BLUE);
T(slides[6],'fileCode','./run.sh file --input input.wav \\\n  --output midterm_spatial.wav \\\n  --algo spatial \\\n  --elevation 60 --azimuth 250\n\naplay -D default midterm_spatial.wav',160,483,800,280,27,'#FFFFFF',true);
H(slides[6],'audio','采集、处理、播放',1080,233,710,70,'sub');
T(slides[6],'audioCode','./run.sh audio --seconds 10 \\\n  --algo spatial \\\n  --elevation -40 --azimuth 45',1080,350,710,157,26,BLUE,true);
line(slides[6],'modeDivider',1080,553,710);
H(slides[6],'orbit','镜像往返过渡',1080,600,710,70,'sub');
T(slides[6],'orbitDesc','40e045 与左右耳互换输出交叉淡化，周期约 4 秒',1080,692,710,83,27);
T(slides[6],'orbitCode','./run.sh file --input input.wav \\\n  --output midterm_orbit.wav \\\n  --algo orbit',1080,802,710,145,26,BLUE,true);
T(slides[6],'condition','板端运行条件：ARM/DSP 成套部署，输入 44.1 kHz / 双声道 / PCM16',130,954,1670,42,25,GREY);
note(slides[6],'单声源模式将两个输入声道平均，再选取方向进行双耳渲染。文件路径先保存结果再使用 aplay 播放，方便检查数据。audio 模式接入采集、DSP 和播放，是连续音频链路的入口。orbit 保持原有实现，在 40e045 双耳输出和左右互换输出之间逐样本交叉淡化，往返约四秒，属于镜像往返演示。\n\n完整命令：\n./run.sh file --input input.wav --output midterm_spatial.wav --algo spatial --elevation 60 --azimuth 250\n./run.sh audio --seconds 10 --algo spatial --elevation -40 --azimuth 45\n./run.sh file --input input.wav --output midterm_orbit.wav --algo orbit\naplay -D default midterm_orbit.wav\n\n角度须存在于 directions 列表。板端性能与听感尚需实测。',['run.sh','host/app/main.c','dsp/dsp_spatial.c']);

// 8. Independent dual sources, source slide 14's top 3 blocks and lower detail.
body(13);head(slides[7],'1.5 双独立声源与兼容模式');
const dual=[['源 A：左输入','方向 60e250\n独立双耳卷积与尾部'],['源 B：右输入','方向 40e045\n独立双耳卷积与尾部'],['双耳混合','两源各 0.5 权重\n输出继续饱和处理']];
dual.forEach((r,i)=>{const x=160+i*560;if(i===0)box(slides[7],'dualHighlight',140,229,520,232,BLUE);H(slides[7],'dualHead'+i,r[0],x+20,250,490,70,'sub',i===0?'#FFFFFF':BLUE);T(slides[7],'dualBody'+i,r[1],x+20,346,480,93,29,i===0?'#FFFFFF':GREY);});
H(slides[7],'dualCmdTitle','A / B 方向分别配置',160,518,1500,64,'sub');
T(slides[7],'dualCmd','./run.sh file --input dual_footsteps.wav \\\n  --output midterm_dual.wav --algo dual \\\n  --a-elevation 60 --a-azimuth 250 \\\n  --b-elevation 40 --b-azimuth 45\naplay -D default midterm_dual.wav',160,586,1650,207,29,BLUE,true);
line(slides[7],'legacyLine',160,805,1650);
T(slides[7],'legacy','兼容命令',160,847,250,50,30,BLUE);
T(slides[7],'legacyCmd','./run.sh file input.wav midterm_40.wav spatial40\n./run.sh file input.wav midterm_60.wav spatial60\n./run.sh file dual_footsteps.wav midterm_fixed.wav dual4060',443,842,1367,137,25,BLUE,true);
note(slides[7],'双声源模式保留输入左右声道的独立性。A、B 各自经过两耳卷积并保留独立尾部，随后以 0.5 权重混合为耳机双声道。新的具名参数可以分别选择两个方向；原固定模式继续保留。演示文件 dual_footsteps.wav 应事先把不同节奏的两路素材放到左右声道。\n\n完整命令：\n./run.sh file --input dual_footsteps.wav --output midterm_dual.wav --algo dual --a-elevation 60 --a-azimuth 250 --b-elevation 40 --b-azimuth 45\naplay -D default midterm_dual.wav\n\n旧语法 spatial40、spatial60、dual4060 保持原固定方向，不混用具名和位置参数。0.5 权重增加余量，不保证任意输入永不削波。',['dsp/dsp_spatial.c','host/app/main.c','run.sh']);

// 9. Actual verification, source slide 16's left evidence / right results.
body(15);head(slides[8],'1.6 验证结果与阶段成效');
H(slides[8],'logTitle','本机测试日志节选',145,234,790,70,'sub');
box(slides[8],'logBackground',145,345,905,516,LIGHT);
T(slides[8],'realLog','HRIR check PASS:\n710 directions, 1420 WAVs\n1454080 coefficient bytes\n\nRan 5 tests\nOK\n\nDSP full catalog PASS:\n710 directions, variable blocks,\ntails, generic dual\n\nDSP cache/invalid-direction\nselftest PASS',176,373,847,460,26,BLUE,true);
[['710 方向','变长块、511 帧尾部与 Q15 对照通过'],['5 项测试','生成数组、格式与配对校验通过'],['1 LSB','本次兼容模式最大误差，非全输入上界']].forEach((r,i)=>{const y=251+i*198;H(slides[8],'result'+i,r[0],1130,y,670,75,'sub');T(slides[8],'resultBody'+i,r[1],1130,y+90,665,86,28);});
T(slides[8],'testCommands','make hrir-check\nmake -C tests/spatial test test-catalog',145,907,1620,83,27,BLUE,true);
T(slides[8],'boundary','Windows / MinGW 数值复核；协议模拟测试待 Linux 补验，板端性能待实测',1130,865,665,112,25,GREY);
note(slides[8],'本次实际通过系数库检查、生成器五项测试和全部 710 方向的卷积回归，代表性双源组合、缓存切换与非法方向检查也通过。日志节选来自当前方位库版本，保持原始数字。兼容固定方向、orbit 和固定双源本次显示最大误差均为 1 LSB；这只对应测试输入与 PC 构建，不是任意输入上界。协议模拟测试在 MinGW 下未能链接部分 TI 模拟符号，测试逻辑未执行，仍需 Linux 补验。目标板的 TI 链接、共享内存硬件与声卡实时性能也需单独验证。\n\n测试命令面向 Ubuntu 开发机：\nmake hrir-check\nmake -C tests/spatial test test-catalog',['docs/evidence/spatial-kemar-update-2026-11-01/import-tests.log','docs/evidence/spatial-kemar-update-2026-11-01/dsp-catalog-tests.log','docs/evidence/spatial-kemar-update-2026-11-01/README.md']);

await fs.writeFile(OUT+'/slide-plan.json',JSON.stringify(order.map((src,i)=>({slide:i+1,sourceTemplateSlide:src+1,notes:slides[i].speakerNotes.text})),null,2));
await fs.mkdir(OUT+'/draft-previews',{recursive:true});
for(let i=0;i<slides.length;i++){
  const png=await p.export({slide:slides[i],format:'png',scale:0.8});
  await fs.writeFile(OUT+'/draft-previews/slide-'+String(i+1).padStart(2,'0')+'.png',new Uint8Array(await png.arrayBuffer()));
  console.log('rendered draft',i+1);
}
await(await PresentationFile.exportPptx(p)).save(OUT+'/draft.pptx');
await fs.writeFile(OUT+'/draft-inspect.ndjson',(await p.inspect({kind:'slide,textbox,notes',maxChars:400000})).ndjson);
console.log('DRAFT_CREATED',OUT+'/draft.pptx');
