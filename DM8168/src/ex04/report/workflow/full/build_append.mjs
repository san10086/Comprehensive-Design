import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from 'file:///C:/Users/30522/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
const ROOT='D:/develop/vscode/projects/ex04', OUT=ROOT+'/report/workflow/full';
const p=await PresentationFile.importPptx(await FileBlob.load(ROOT+'/report/template.pptx'));
const snap=await p.inspect({kind:'slide,shape,textbox,image,chart,table',maxChars:1000000});
const records=snap.ndjson.split('\n').filter(Boolean).map(JSON.parse);
const source=p.slides.items.slice(), sr=records.filter(r=>r.kind==='slide');
const order=[5,7,3,12,16,6,8,18,13,10];
p.slides.keep(order); p.slides.reorder(order.map(i=>sr.find(r=>r.slideIndex===i).id));
const slides=order.map(i=>source[i]);
const BLUE='#2E568C', GREEN='#16BF84', GREY='#545454', LIGHT='#EDF3F8';
const BODY='思源黑体 2', HEAVY='思源黑体 1 Heavy';
const deleted=new Set();
function remove(index,predicate){
 const ids=[...new Set(records.filter(r=>r.slideIndex===index&&r.position&&['shape','textbox','image','chart','table'].includes(r.kind)&&predicate(r)).map(r=>r.id))].filter(id=>!deleted.has(id));
 if(ids.length){p.delete(ids);ids.forEach(id=>deleted.add(id));}
}
function body(index,keep=false){
 remove(index,r=>r.position.left>1600&&r.position.top<145||r.position.top>=145&&r.position.top<1015&&(!keep||r.kind==='textbox'||r.kind==='image'||r.kind==='table'||r.kind==='chart'));
 remove(index,r=>r.kind==='textbox'&&(r.position.top<140||r.position.top>1015));
}
function H(s,name,text,x,y,w,h,role='sub',color=BLUE){
 const sizes={header:56,sub:48,chapter:144,number:188*4/3};
 const sh=s.shapes.add({name:'report.'+role+'.'+name,geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
 sh.text=text; sh.text.style={fontSize:sizes[role],typeface:HEAVY,bold:true,color,wrap:'square',autoFit:'none',insets:0,verticalAlignment:'middle'};return sh;
}
function T(s,name,text,x,y,w,h,size=28,color=GREY){
 const sh=s.shapes.add({name:'report.body.'+name,geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
 sh.text=text; sh.text.style={fontSize:size,typeface:BODY,color,wrap:'square',autoFit:'none',insets:0,verticalAlignment:'top',lineSpacing:1.17};return sh;
}
function box(s,name,x,y,w,h,fill=LIGHT,stroke='none'){
 return s.shapes.add({name:'report.box.'+name,geometry:'rect',position:{left:x,top:y,width:w,height:h},fill,line:{fill:stroke,width:stroke==='none'?0:1.4}});
}
function line(s,name,x,y,w){return s.shapes.add({name:'report.line.'+name,geometry:'line',position:{left:x,top:y,width:w,height:0},fill:'none',line:{fill:'#BDD0E0',width:1.4}});}
function head(s,text,part=2){H(s,'header',text,82.4,49.7,1690,70,'header');T(s,'part',part===2?'PART TWO':'PART THREE',1647,1036,230,32,24,'#FFFFFF');}
const md=await fs.readFile(ROOT+'/docs/中期汇报_第二章与第三章_Qt开发与后续工作.md','utf8');
function note(s,id,extra=''){
 const sec=md.split('## '+id+' ')[1]?.split('\n---')[0]||'';
 const talk=sec.split('### 汇报讲稿')[1]?.split(/\n###|\n制作备注|\n依据：/)[0].trim()||'';
 s.speakerNotes.text=talk+'\n\n'+extra+'\n\n稿件与证据索引：\n'+ROOT+'/docs/中期汇报_第二章与第三章_Qt开发与后续工作.md\n'+ROOT+'/frontend_arm/README.md\n'+ROOT+'/frontend_arm/VALIDATION.md\n'+ROOT+'/docs/evidence/arm-build-2026-11-01/README.md';
}
function chapter(s,index,num,title,green,en,topics){
 body(index);remove(index,r=>r.position.top>=145&&r.position.top<1015);
 H(s,'chapterNumber',num,230,380,460,320,'number','#7192BB');
 box(s,'chapterBorder',180,345,540,420,'none','#7192BB');
 H(s,'chapterTitle',title,850,208,1020,340,'chapter','#FFFFFF');
 H(s,'chapterGreen',green,850,560,1020,178,'chapter',GREEN);
 T(s,'chapterEn',en,858,770,910,50,27,'#FFFFFF');
 topics.forEach((t,i)=>{let x=854+i*320;box(s,'topic'+i,x,867,299,64,'none','#89A4C7');T(s,'topicText'+i,t,x+15,882,275,40,24,'#FFFFFF');});
}
async function image(s,file,x,y,w,h,alt){s.images.add({blob:await fs.readFile(ROOT+'/report/ima/'+file),contentType:'image/png',alt,fit:'contain',position:{left:x,top:y,width:w,height:h}});}
function table(s,name,values,x,y,w,h,widths){
 const tb=s.tables.add({rows:values.length,columns:values[0].length,left:x,top:y,width:w,height:h,columnWidths:widths,values});
 tb.styleOptions={headerRow:true,bandedRows:false};
 tb.borders.assign({style:'solid',fill:'#FFFFFF',width:2});
 for(let r=0;r<values.length;r++)for(let c=0;c<values[0].length;c++){
  const cell=tb.getCell(r,c);cell.fill=r===0?BLUE:r%2?LIGHT:'#F6F8FA';
  cell.text.style={fontSize:r===0?29:28,typeface:BODY,bold:r===0,color:r===0?'#FFFFFF':c===0?BLUE:GREY,wrap:'square',verticalAlignment:'middle',autoFit:'none',insets:{left:18,right:18,top:12,bottom:12}};
 }
 return tb;
}

// Chapter two divider (template page 6).
chapter(slides[0],5,'02.','基于 QT 的\nGUI 图形交互','应用开发上手','QT GUI APPLICATION DEVELOPMENT',['环境与交付','进程控制与 IPC','VNC 联调']);
slides[0].speakerNotes.text='第二章介绍 Qt 图形交互应用开发。先说明三端环境与发布过程，再展示界面和参数接入，随后介绍进程生命周期、错误处理、独立 IPC 验证，以及无屏环境下的 VNC 排错。';

// 2.1 Three environments and reproducible delivery (template page 8).
body(7);head(slides[1],'2.1 开发环境与编译、打包部署');
T(slides[1],'lead','Windows 开发与传输，Ubuntu20 交叉编译，DM8168 运行',130,190,1680,65,34,BLUE);
const env=[['Windows','源码整理与 XFTP','源码包与校验文件\nVNC 查看与回传\n区分源码与目标产物'],['Ubuntu20','ARM / DSP / Qt 构建','ARM GCC 4.8.3 + EZSDK\nQt5.9.7 ARM SDK\nhost / DSP 的 debug、release 通过'],['DM8168','发布包与运行环境','ARM EABI5 程序\n匹配 Qt 库、插件、字体\n部署及中文显示获确认']];
env.forEach((r,i)=>{const x=130+i*570;H(slides[1],'env'+i,r[0],x,290,520,70);T(slides[1],'envRole'+i,r[1],x,389,520,50,32,BLUE);T(slides[1],'envBody'+i,r[2],x,461,520,157,28);if(i<2)T(slides[1],'envArrow'+i,'›',x+520,414,45,75,66,GREEN);});
box(slides[1],'buildCmd',130,690,1660,230,BLUE);
T(slides[1],'cmdLabel','Ubuntu20  /  Qt 构建与安装',158,711,1550,36,25,'#FFFFFF');
T(slides[1],'commands','source /home/user/ex04-build-paths.sh\ncd "$PROJECT_ROOT"\nBUILD_TARGET=arm BUILD_DIR="$QT_BUILD" sh frontend_arm/build.sh "$QMAKE_ARM"\nBUILD_DIR="$QT_BUILD" sh frontend_arm/install.sh "$STAGE_APP"',158,761,1595,145,26,'#FFFFFF');
T(slides[1],'delivery','11 个发布文件 SHA256 复核通过；板端脚本兼容 SHA256 / MD5，更新前备份',130,952,1660,45,27,BLUE);
note(slides[1],'2.1','桌面自动化测试环境另为 WSL Ubuntu22.04 / GCC11 / Qt5.15.3，不与 Ubuntu20 的 ARM 构建混写。GUI 与 IPC peer 已构建为 ARM；11 文件校验来自用户虚拟机发布包日志，板端部署获确认，完整部署日志仍待归档。目标解释器 /lib/ld-linux.so.3。板端精简 shell 缺少 command 和 SHA256 applet，部署脚本改用实际执行探测及 MD5 兼容。');

// 2.2 Parameter screenshot alongside the four-page application (template page 4).
body(3);head(slides[2],'2.2 GUI 页面与空间参数接入');
H(slides[2],'pages','四页任务界面',130,229,650,70);
T(slides[2],'pagesBody','波形：输入 / 输出 L、R 四轨\n参数：模式、路径与方向\n诊断：日志与 IPC 验证入口\n设置：运行环境与业务目录',130,328,650,184,29);
H(slides[2],'directions','710 个方向联动',130,558,650,70);
T(slides[2],'dirBody','先选仰角，再列出有效方位\n单声源与双声源分别配置\nQProcess 以参数列表启动 host',130,660,650,142,29);
await image(slides[2],'屏幕截图 2026-11-01 182404.png',830,226,1000,638,'用户提供的 TigerVNC 参数页，演示环境，双声源 A20/270、B40/141');
T(slides[2],'caption','用户回传参数页，保留“演示”标识；截图不代表实际音频任务通过',830,872,1000,38,22,GREY);
T(slides[2],'command','板端 host 等价参数：\n./app_host DSP file --input input.wav --output qt_dual.wav --algo dual \\\n  --a-elevation 20 --a-azimuth 270 --b-elevation 40 --b-azimuth 141',130,897,1690,105,25,BLUE);
note(slides[2],'2.2','本页采用用户 report/ima/屏幕截图 2026-11-01 182404.png，截图中 A=20/270、B=40/141。命令与图中方向对应，输出名为演示命令示例。默认 DemoBackend 不启动 DSP、不写音频文件。当前 GUI 接入 spatial / dual 具名参数；兼容 CLI 的 orbit 等模式不描述为 GUI 按钮。读取文件限定 44.1 kHz、双声道 PCM16，使用工作线程与有界包络。实际 QProcess 使用 QStringList，不通过 shell 执行这段展示文本。');

// 2.3 Ordered lifecycle and concrete error responses (template page 13).
body(12);head(slides[3],'2.3 进程生命周期与错误处理');
T(slides[3],'lead','分阶段控制 DSP 任务，启动失败与停止均保留界面和日志',130,190,1690,65,34,BLUE);
['配置检查','加载 DSP','运行 host','卸载 DSP','任务回收'].forEach((r,i)=>{let x=130+i*340;box(slides[3],'flow'+i,x,294,300,100,i===2?BLUE:LIGHT);T(slides[3],'flowText'+i,r,x+24,321,260,58,34,i===2?'#FFFFFF':BLUE);if(i<4)T(slides[3],'arrow'+i,'›',x+308,308,35,70,59,GREEN);});
const errors=[['启动前检查','文件、权限与方向检查\nloader 失败不进入 host\n异常退出保留退出码'],['协作停止','等待 main: control ready\n向当前 host 发送 SIGINT\n退出后进入 DSP 卸载'],['异常与重入','阶段或停止等待超时提示\nGUI 文件锁防止重复任务\n启动失败后延迟清理']];
errors.forEach((r,i)=>{let x=130+i*570;H(slides[3],'errorHead'+i,r[0],x,484,520,70);T(slides[3],'errorBody'+i,r[1],x,582,520,150,28);});
box(slides[3],'cmd',130,770,1660,187,BLUE);
T(slides[3],'commands','板端三阶段等价命令（GUI 依次执行）\n./slaveloader startup DSP server_dsp.xe674\n./app_host DSP smoke --algo passthrough\n./slaveloader shutdown DSP',158,787,1600,162,26,'#FFFFFF');
T(slides[3],'boundary','桌面生命周期 / IPC QtTest：15 passed；真实板端业务闭环仍待验收',130,971,1660,39,26,BLUE);
note(slides[3],'2.3','record 模式只启动录音 host，不加载 DSP。30 秒提示只用于 loader 阶段和请求停止后的等待，不是正常业务运行的通用 watchdog。停止不以强杀音频 host 或 DSP loader 为常规路径。/tmp/ex04-qt-audio.lock 仅协调 GUI 实例，不能排斥其他终端的手工音频任务。FailedToStart 回调使用 QTimer 下一轮事件循环继续清理，避免立即重用 QProcess 的重入。15 passed 含初始化与清理项。');

// 2.4 Real kernel IPC demonstration, separate from PCM (template page 17).
body(16);head(slides[4],'2.4 IPC 同步与共享内存数据交互');
T(slides[4],'lead','独立 C peer 与 Qt 接收线程，验证 Linux System V IPC',130,190,1690,65,34,BLUE);
const ipc=[['C peer','等待可写，写序号与内容'],['信号量握手','写完置可读，读完置可写'],['Qt 工作线程','复制数据、校验序号并通知界面']];
ipc.forEach((r,i)=>{let x=130+i*570;box(slides[4],'ipc'+i,x,298,520,154,i===1?BLUE:LIGHT);T(slides[4],'ipcHead'+i,r[0],x+23,317,478,52,34,i===1?'#FFFFFF':BLUE);T(slides[4],'ipcBody'+i,r[1],x+23,386,478,54,26,i===1?'#FFFFFF':GREY);if(i<2)T(slides[4],'arrow'+i,'›',x+531,345,35,65,55,GREEN);});
H(slides[4],'messages','10 条有序消息',130,524,770,70);
T(slides[4],'messagesBody','序号与内容检查，避免丢消息和覆盖\n真实 Linux IPC，覆盖提前停止与异常退出\n当前示例尚未承载 host 的实时 PCM',130,626,770,148,29);
H(slides[4],'resource','线程与资源回收',1050,524,740,70);
T(slides[4],'resourceBody','工作线程非阻塞读取，界面保持响应\nQt 管理信号量、共享内存和 C 子进程\n结束后停止线程并回收会话资源',1050,626,740,148,29);
box(slides[4],'cmd',130,812,1660,135,BLUE);
T(slides[4],'command','Ubuntu 开发机：先构建本机架构 build-desktop，在工程根目录运行\nEX04_IPC_PEER="$PROJECT_ROOT/frontend_arm/build-desktop/bin/ex04_ipc_peer" \\\n  python3 frontend_arm/tests/test_ipc_peer.py',158,829,1600,106,25,'#FFFFFF');
T(slides[4],'ui','界面入口：诊断页，选择“信号量”或“共享内存”，观察 1–10 条消息及停止、重跑',130,971,1660,39,26,BLUE);
note(slides[4],'2.4','桌面验证使用真实 Linux 内核 IPC。IPC_PRIVATE、权限 0600，semid/shmid 进入日志。GUI 接收线程每 20ms 非阻塞检查。C peer 以约 200ms 间隔发送 10 条消息。Qt 负责创建及释放资源。Linux System V 共享内存与 TI SysLink SharedRegion 分别验证。当前截图中的诊断页只显示入口与演示日志，不是板端 IPC PASS 证据，因此不使用其充当运行结果。板端 ARM IPC 的 1–10 条和资源清理仍需归档。');

// 2.5 VNC actual viewer screenshot (template page 7).
body(6);head(slides[5],'2.5 VNC 远程显示与连接排错');
await image(slides[5],'屏幕截图 2026-11-01 182317.png',130,223,900,574,'用户 TigerVNC 波形页，真实输入波形，演示环境，输出等待数据');
T(slides[5],'caption','用户回传的 VNC 波形页；输入 WAV 已加载，输出仍等待数据',130,813,900,42,23,GREY);
const vnc=[['驱动与运行库','Qt4 报 driver not found\n切换 Qt5.9.7 VNC 插件，5900 监听通过'],['黑屏与像素格式','定位 Cannot handle 0 bpp client\n加入 RFB 像素格式兼容转发'],['中文与重连','补充字库后中文、波形可见\n全彩显示与一次重新连接获确认']];
vnc.forEach((r,i)=>{let y=215+i*213;H(slides[5],'vncHead'+i,r[0],1100,y,700,70);T(slides[5],'vncBody'+i,r[1],1100,y+97,700,104,27);});
box(slides[5],'cmd',130,874,1660,131,BLUE);
T(slides[5],'commands','板端：sh /home/root/ex04-fonts/run-gui-vnc.sh > /tmp/ex04-gui.log 2>&1 < /dev/null &\nWindows 工程根目录：.\\tools\\deploy\\start-vnc.cmd\n连接 127.0.0.1::5901，经本机转发至 192.168.1.21:5900',154,884,1610,117,25,'#FFFFFF');
note(slides[5],'2.5','本页采用用户 report/ima/屏幕截图 2026-11-01 182317.png。该图显示真实输入 WAV 的 L/R 波形，输出区仍为等待数据，界面处于演示环境。它支持远程显示结果，不作为 DSP 输出验收。RFB 转发只监听 Windows 本机回环。Qt 插件未重编译，固定 256 色不是最终稳定方案。一次重新连接由用户确认；HDMI、鼠标、长期稳定性及 VPSS 与 DSP 共存仍待验证。');

// Chapter three divider (template page 9).
chapter(slides[6],8,'03.','不足与','后续工作','LIMITATIONS AND NEXT STEPS',['业务闭环与数据回传','RPE 引入准备','分阶段验收']);
slides[6].speakerNotes.text='第三章说明当前还缺少哪些板端证据，以及后续如何推进。重点是补齐 GUI 控制真实音频进程的完整闭环，接入真实 PCM，再按课程要求准备 RPE 例程与算法封装，最后补齐性能和试听评价。';

// 3.1 Native status table (template page 19).
body(18);head(slides[7],'3.1 当前不足与验收边界',3);
T(slides[7],'lead','当前缺口集中在真实任务闭环与实时 PCM 回传',130,190,1660,65,34,BLUE);
table(slides[7],'status',[
 ['环节','已经完成','尚需验证与归档'],
 ['构建与部署','ARM host / DSP / GUI 构建通过\n板端部署获确认','完整部署日志、版本及文件清单'],
 ['GUI 与 VNC','中文、演示波形可见\n一次重新连接正常','长时间刷新、交互和退出重启'],
 ['GUI 音频进程','QProcess 生命周期\n桌面回归完成','板端启动、处理、停止、卸载和重跑'],
 ['实时 PCM','AudioBackend / PcmChunk\n接口已预留','真实采集与处理数据回传、丢块统计'],
 ['显示与 DSP 共存','共存测试方案、DSP map 核对','实际固件、物理内存及同时运行']
],130,292,1660,570,[315,650,695]);
H(slides[7],'desktop','桌面验证',130,907,390,65);
T(slides[7],'desktopCounts','生命周期 / IPC：15 passed      GUI / 波形：10 passed',560,925,1220,50,29,BLUE);
note(slides[7],'3.1','QtTest 数字含初始化与清理条目，不合并成独立功能数量。BoardBackend 目前包装 PipelineRunner，没有发送真实 waveformReady(PcmChunk)，演示数据由 DemoBackend 生成。DSP_PROG 的 map 使用 1,794,278 字节，约 1.71 MiB，已在 16 MiB 配置范围内完成链接核对，但它不是运行期峰值，也不证明与其他核固件共存。');

// 3.2 Planned RPE path and native preparation table (template page 14).
body(13);head(slides[8],'3.2 RPE 引入准备与技术路线',3);
T(slides[8],'lead','RPE（Remote Processor Execute Engine），目前处于引入准备阶段',130,190,1660,65,32,BLUE);
const rpe=['Qt GUI','音频后端适配','RPE 调用','DSP 空间算法'];
rpe.forEach((t,i)=>{let x=130+i*425;box(slides[8],'rpe'+i,x,299,365,100,i===2?BLUE:LIGHT);T(slides[8],'rpeText'+i,t,x+22,326,328,54,32,i===2?'#FFFFFF':BLUE);if(i<3)T(slides[8],'arrow'+i,'›',x+383,314,38,65,55,GREEN);});
table(slides[8],'rpePlan',[
 ['准备环节','拟完成的工作','通过条件'],
 ['SDK 与例程','核对库、头文件、固件与课程例程','原例程可构建，依赖与内存要求清楚'],
 ['最小调用','ARM 调用 DSP 基础算法','固定输入输出一致，错误与退出可观测'],
 ['空间封装','明确帧长、方向、缓冲与跨块状态','结果与现有卷积参考一致'],
 ['GUI 适配','在后端抽象内接入 RPE','界面可启动、停止并显示结果']
],130,484,1660,410,[315,650,695]);
T(slides[8],'boundary','当前工程仍使用 SysLink；RPE 的构建与运行命令在例程核对后补充',130,945,1660,50,28,BLUE);
note(slides[8],'3.2','RPE 名称按用户确认统一。全称来自课程第 1、6 次课。当前没有 RPE 构建与运行结果，也没有可引用的真实启动命令。准备先核对 SDK，跑通最小例程，再封装空间算法。跨块状态的所有权、配置重置、缓冲区与缓存一致性需要明确。更换调用框架不能直接宣称消除显示与 DSP 的固件、内存冲突。');

// 3.3 Timeline carrier from template page 11, with executable future checks.
body(10,true);head(slides[9],'3.3 后续工作与分阶段验收',3);
T(slides[9],'lead','先建立可重复的板端基线，再连接 GUI 与真实音频数据',130,190,1660,65,34,BLUE);
const plan=[['板端闭环','基线与 GUI','核对固件、内存\n逐项验证音频模式\n启动、停止后可重跑'],['真实 PCM','输入与输出波形','按会话及帧序号接收\n缓冲有界，拒绝过期数据\n记录丢块与清理结果'],['RPE 迁移','从最小例程开始','基础调用通过后封装\n与现有参考逐项对照\n接入 GUI 后端'],['量化评价','性能与体验','分开测耗时和链路延迟\n记录 underrun、持续运行\n补受控耳机试听']];
plan.forEach((r,i)=>{let x=135+i*437;let a=H(slides[9],'step'+i,r[0],152+i*437.2,330,310,65);a.text.alignment='center';H(slides[9],'stage'+i,r[1],x,613,390,64);T(slides[9],'stageBody'+i,r[2],x,704,390,135,26);});
box(slides[9],'cmd',130,842,1660,163,BLUE);
T(slides[9],'commands','板端后续验收（固件、内存核对通过后逐条执行）：./run.sh smoke --algo passthrough\n./run.sh record --output verify_record.wav --seconds 5\n./run.sh file --input input.wav --output verify_spatial.wav --algo spatial --elevation 0 --azimuth 90\n./run.sh audio --seconds 10 --algo spatial --elevation 0 --azimuth 90',154,854,1610,147,24,'#FFFFFF');
note(slides[9],'3.3','本页命令为后续验收计划，不代表已通过。先确认输入素材、业务目录、固件和物理内存配置，再逐条执行。失败时停在首个异常阶段并保留退出码、帧块数及日志。GUI 应验证真实任务启动、协作停止、清理和再次运行。持续运行先做短时冒烟，再按课程条件安排更长时间测试。DSP 算法耗时、ARM/DSP 往返和完整链路延迟分别统计，日志与耳机试听分开评价。');

await fs.writeFile(OUT+'/append-plan.json',JSON.stringify(order.map((source,i)=>({slide:i+9,sourceTemplateSlide:source+1,notes:slides[i].speakerNotes.text})),null,2));
await(await PresentationFile.exportPptx(p)).save(OUT+'/append-draft.pptx');
await fs.mkdir(OUT+'/draft-previews',{recursive:true});
for(let i=0;i<slides.length;i++){
 const png=await p.export({slide:slides[i],format:'png',scale:0.8});
 await fs.writeFile(OUT+'/draft-previews/slide-'+String(i+9).padStart(2,'0')+'.png',new Uint8Array(await png.arrayBuffer()));
}
console.log('APPEND_CREATED',slides.length);
