var L=localStorage.lang||'zh';
function t(k){var m={zh:{off:'关',led_on:'已开启',led_off:'已关闭',shutdown:'关机',screen_off:'关屏幕',screen_on:'开屏幕',wake:'开机',wake_done:'已开机',running:'运行中',standby:'待机中',rpm:'转/分',connected:'已连接',disconnected:'未连接',applied:'已应用',saved:'已保存',failed:'失败',saving:'保存中...',testing:'测试中...',executing:'执行中...',shut_down:'已关机',screen_off_done:'屏幕已关',screen_on_done:'屏幕已开',connect_ok:'连接成功',mqtt_restart:'已保存! 即将重启...',hs_restart:'已保存! 即将重启...',today:'今天',tomorrow:'明天',day_after:'后天',every_day:'每天',weekdays:'工作日',weekends:'周末',mon:'周一',tue:'周二',wed:'周三',thu:'周四',fri:'周五',sat:'周六',sun:'周日',uploading:'上传中...',ota_ok:'升级成功，设备重启中...',ota_fail:'升级失败',network_err:'网络错误',pick_bin:'请选择 .bin 文件',upload_btn:'上传并升级'},en:{off:'Off',led_on:'On',led_off:'Off',shutdown:'Shutdown',screen_off:'Screen Off',screen_on:'Screen On',wake:'Power On',wake_done:'Powered on',running:'Running',standby:'Standby',rpm:'RPM',connected:'Connected',disconnected:'Disconnected',applied:'Applied',saved:'Saved',failed:'Failed',saving:'Saving...',testing:'Testing...',executing:'Running...',shut_down:'Shut down',screen_off_done:'Screen off',screen_on_done:'Screen on',connect_ok:'Connected',mqtt_restart:'Saved! Restarting...',hs_restart:'Saved! Restarting...',today:'Today',tomorrow:'Tomorrow',day_after:'Day After',every_day:'Every Day',weekdays:'Weekdays',weekends:'Weekends',mon:'Mon',tue:'Tue',wed:'Wed',thu:'Thu',fri:'Fri',sat:'Sat',sun:'Sun',uploading:'Uploading...',ota_ok:'Upgrade successful, rebooting...',ota_fail:'Upgrade failed',network_err:'Network Error',pick_bin:'Select a .bin file',upload_btn:'Upload & Upgrade'}};
return m[L]&&m[L][k]||k;
}
function toggleLang(){localStorage.lang=L==='zh'?'en':'zh';
location.reload();
}
document.getElementById('lang_btn').textContent=L==='zh'?'EN':'中';
if(L==='en'){var M={'AiRFLOW 配网':'AiRFLOW Config','清新空气 智慧生活':'Fresh Air, Smart Living','手机连接':'Connect to','AiRFLOW 热点':'AiRFLOW Hotspot','选择你的':'Select your','WiFi 网络':'WiFi Network','输入密码':'Enter Password','完成连接':'Connect','&#9432;
 手机通常会自动弹出此页面。<br>如未弹出，请在浏览器地址栏输入':'&#9432;
 Phone usually opens this page automatically.<br>If not, enter','&#9432;
 本设备已连接互联网，此页面仅用于查看状态与管理。':'&#9432;
 Device is online. This page is for status and management only.','传感器数据':'Sensor Data','风扇控制':'Fan Control','网络信息':'Network Info','MQTT 配置':'MQTT Config','设备状态':'Device Status','报警设置':'Alarm Settings','自定义首页':'Custom Home','状态指示灯':'Status LED','电源管理':'Power Management','报警阈值':'Alarm Thresholds','TVOC 阈值':'TVOC Threshold','CO₂ 阈值':'CO₂ Threshold','甲醛 阈值':'CH₂O Threshold','自动风扇':'Auto Fan','保存':'Save','返回':'Back','超标自动开启风扇':'Auto fan on alarm','启动后默认显示':'Default after boot','开关':'On/Off','亮度':'Brightness','效果':'Effect','风速':'Speed','WiFi':'WiFi','IP 地址':'IP Address','MQTT 代理':'MQTT Broker','MQTT 状态':'MQTT Status','未连接':'Disconnected','已连接':'Connected','控制面板':'Control Panel','测试连接':'Test','保存并重启':'Save & Restart','保存定时':'Save Schedule','关屏幕':'Screen Off','关机':'Shutdown','定时开关机':'Scheduled Power','关机时间':'Off Time','开机时间':'On Time','今天':'Today','明天':'Tomorrow','后天':'Day After','每天':'Every Day','工作日':'Weekdays','周末':'Weekends','周一':'Mon','周二':'Tue','周三':'Wed','周四':'Thu','周五':'Fri','周六':'Sat','周日':'Sun','实时监测温度、TVOC、CO₂、甲醛':'Real-time temperature, TVOC, CO₂, CH₂O monitoring','设定传感器报警值与自动风扇':'Set alarm thresholds and auto fan','选择设备启动后默认显示的页面':'Choose default screen after boot','调节风速，开关风扇与自动模式':'Adjust speed, on/off and auto mode','查看WiFi连接与MQTT状态':'View WiFi connection and MQTT status','设置MQTT代理地址，连接智能家居平台':'Set MQTT broker for smart home','调节LED颜色、亮度与显示效果':'Adjust LED color, brightness and effects','关屏幕、关机与定时开关机':'Screen off, shutdown and scheduling','开屏幕':'Screen On','开机':'Power On','常亮':'Steady','慢闪':'Slow Blink','呼吸':'Breathe','快闪':'Fast Blink','彩虹':'Rainbow','转/分':'RPM','待机中':'Standby','运行中':'Running','已连接':'Connected','未连接':'Disconnected','已应用':'Applied','已保存':'Saved','失败':'Failed','保存中...':'Saving...','测试中...':'Testing...','执行中...':'Running...','已关机':'Shut down','屏幕已关':'Screen off','连接成功':'Connected','已保存! 即将重启...':'Saved! Restarting...','输入密码':'Enter Password','显示密码':'Show','隐藏':'Hide','首页':'Home','仪表盘':'Dashboard','天气':'Weather','网络设置':'Network','设置':'Settings','声音':'Sound','电源':'Power','固件升级':'Firmware Update','选择 .bin 文件上传，设备自动重启':'Select .bin file to upload, device will reboot','上传并升级':'Upload & Upgrade','各状态 LED 颜色':'Per-state LED Color','正常':'Normal','报警':'Alarm','WiFi失败':'WiFi Fail','WiFi连接中':'WiFi Connecting','温度':'Temperature','甲醛':'CH₂O','保存阈值':'Save Thresholds','调色盘':'Color Picker'};
function walk(n){if(n.nodeType===3&&n.parentNode.nodeName!=='SCRIPT'&&n.parentNode.nodeName!=='STYLE'){var s=n.textContent,t;
var keys=Object.keys(M).sort(function(a,b){return b.length-a.length;
});
for(var ki=0;
ki<keys.length;
ki++){var k=keys[ki];
var i=s.indexOf(k);
if(i!==-1){t=M[k];
n.textContent=s.substring(0,i)+t+s.substring(i+k.length);
s=n.textContent;
}}}else if(n.nodeType===1)for(var c=n.firstChild;
c;
c=c.nextSibling)walk(c);
}walk(document.body);
document.title='AiRFLOW Config';
}
var fanOn=false,fanSpd=0,changing=false,lastFanFetch=0,thrInit=false;
function poll(){fetch('/status').then(function(r){return r.json();
}).then(function(j){var el;
el=document.getElementById('temp');
el.textContent=j.t.toFixed(1);
el.className=j.t<-10?'s-value bad':j.t>50?'s-value warn':'s-value';
el=document.getElementById('tvoc');
el.textContent=j.tvoc;
el.className=j.tvoc>500?'s-value bad':j.tvoc>200?'s-value warn':'s-value';
el=document.getElementById('co2');
el.textContent=j.co2;
el.className=j.co2>1500?'s-value bad':j.co2>800?'s-value warn':'s-value';
el=document.getElementById('ch2o');
el.textContent=j.ch2o;
el.className=j.ch2o>100?'s-value bad':j.ch2o>50?'s-value warn':'s-value';
if(!changing){document.getElementById('fs').value=j.fs;
document.getElementById('fo').checked=j.fo;
}
var pct=j.fo?j.fs+'%':t('off');
document.getElementById('fv').textContent=pct;
document.getElementById('frpm2').textContent=j.fo&&j.rpm?j.rpm+' '+t('rpm'):'-- '+t('rpm');
document.getElementById('fstLabel').textContent=j.fo?t('running'):t('standby');
var ico=document.getElementById('fanIco');
if(j.fo&&j.fs>0)ico.classList.add('on');
else ico.classList.remove('on');
if(!thrInit){document.getElementById('tvoc_thr_sl').value=j.tvoc_thr||500;
document.getElementById('co2_thr_sl').value=j.co2_thr||1000;
document.getElementById('ch2o_thr_sl').value=j.ch2o_thr||100;
document.getElementById('af_chk').checked=j.auto_fan!==0;
onThrChange();
thrInit=true;
}
var hb=document.getElementById('holiday_badge');
if(hb){if(j.holiday){hb.textContent='🎉'+j.holiday;
hb.style.display='';
}else{hb.style.display='none';
}}});
fetch('/netinfo').then(function(r){return r.json();
}).then(function(j){document.getElementById('ni_ssid').textContent=j.ssid;
document.getElementById('ni_ip').textContent=j.ip;
document.getElementById('ni_mqtt').textContent=j.mqtt;
var inp=document.getElementById('mqtt_inp');
if(!inp.value&&j.mqtt!='N/A')inp.value=j.mqtt;
var hs=document.getElementById('hs_sel');
if(hs&&!hs._init){hs.value=j.home_scr||0;
hs._init=true;
}
var e=document.getElementById('ni_mqtts');
if(j.mqtt_ok){e.innerHTML="<span class='status-dot ok'></span>"+t('connected');
e.className='val ok';
}else{e.innerHTML="<span class='status-dot err'></span>"+t('disconnected');
e.className='val err';
}
var led=document.getElementById('led_chk');
if(led && !led._init){led.checked=j.led_on;
led._init=true;
}
var lr=document.getElementById('led_r');
if(lr && !lr._init){var r=j.led_r!==undefined?j.led_r:0,g=j.led_g!==undefined?j.led_g:255,b=j.led_b!==undefined?j.led_b:0;
syncLedUI(r,g,b);
document.getElementById('led_bri').value=j.led_bri||100;
document.getElementById('led_bri_val').textContent=j.led_bri||100;
lr._init=true;
}});
}
setInterval(poll,2000);
poll();
function onThrChange(){document.getElementById('tvoc_thr_val').textContent=document.getElementById('tvoc_thr_sl').value;
document.getElementById('co2_thr_val').textContent=document.getElementById('co2_thr_sl').value;
document.getElementById('ch2o_thr_val').textContent=document.getElementById('ch2o_thr_sl').value;
}
function saveAlarm(){var el=document.getElementById('alarm_res');
el.textContent=t('saving');
el.style.color='var(--text3)';
var body='tvoc_thr='+document.getElementById('tvoc_thr_sl').value+'&co2_thr='+document.getElementById('co2_thr_sl').value+'&ch2o_thr='+document.getElementById('ch2o_thr_sl').value+'&auto_fan='+(document.getElementById('af_chk').checked?1:0);
fetch('/alarm_cfg',{method:'POST',body:body}).then(function(r){return r.json();
}).then(function(j){if(j.ok){el.textContent=t('saved');
el.style.color='var(--green)';
}else{el.textContent=t('failed');
el.style.color='#e53935';
}});
}
document.getElementById('fs').oninput=function(){changing=true;
};
document.getElementById('fs').onchange=function(){var v=parseInt(this.value);
changing=false;
fetch('/set',{method:'POST',body:'fs='+v+'&fo='+(document.getElementById('fo').checked?1:0)});
};
document.getElementById('fo').onchange=function(){changing=false;
fetch('/set',{method:'POST',body:'fs='+document.getElementById('fs').value+'&fo='+(this.checked?1:0)});
};
function testMqtt(){var inp=document.getElementById('mqtt_inp').value.trim(),res=document.getElementById('mqtt_res');
if(!inp){res.textContent='请输入代理地址';
res.style.color='#e53935';
return;
}res.textContent=t('testing');
res.style.color='var(--text3)';
fetch('/mqtt_test?broker='+encodeURIComponent(inp)).then(function(r){return r.json();
}).then(function(j){if(j.ok){res.textContent=t('connect_ok')+' — '+j.ip+':'+j.port;
res.style.color='var(--green)';
}else{res.textContent=t('failed')+': '+j.error;
res.style.color='#e53935';
}});
}
function saveMqtt(){var inp=document.getElementById('mqtt_inp').value.trim(),res=document.getElementById('mqtt_res');
if(!inp){res.textContent='请输入代理地址';
res.style.color='#e53935';
return;
}res.textContent=t('saving');
res.style.color='var(--text3)';
fetch('/mqtt_save',{method:'POST',body:'broker='+encodeURIComponent(inp)}).then(function(r){return r.json();
}).then(function(j){if(j.ok){res.textContent=t('mqtt_restart');
res.style.color='var(--green)';
}else{res.textContent=t('failed');
res.style.color='#e53935';
}});
}
function saveHomeScreen(){var v=document.getElementById('hs_sel').value,res=document.getElementById('hs_res');
res.textContent=t('saving');
res.style.color='var(--text3)';
fetch('/home_screen',{method:'POST',body:'hs='+v}).then(function(r){return r.json();
}).then(function(j){if(j.ok){res.textContent=t('hs_restart');
res.style.color='var(--green)';
}else{res.textContent=t('failed');
res.style.color='#e53935';
}});
}
function onLedToggle(){var ck=document.getElementById('led_chk').checked,res=document.getElementById('led_res');
fetch('/led',{method:'POST',body:'on='+(ck?1:0)}).then(function(r){return r.json();
}).then(function(j){if(j.ok){res.textContent=ck?t('led_on'):t('led_off');
res.style.color='var(--green)';
setTimeout(function(){res.textContent='';
},2000);
}else{res.textContent=t('failed');
res.style.color='#e53935';
document.getElementById('led_chk').checked=!ck;
}});
}
var rh=function(x){var s=x.toString(16);
return s.length<2?'0'+s:s;
};
function onLedBri(){var bri=document.getElementById('led_bri').value;
document.getElementById('led_bri_val').textContent=bri;
fetch('/led_cfg',{method:'POST',body:'bri='+bri});
}
fetch('/led_cfg').then(function(r){return r.json();
}).then(function(j){if(!j||j.bri===undefined)return;
document.getElementById('led_bri').value=j.bri;
document.getElementById('led_bri_val').textContent=j.bri;
});
var g_led_states=[];
fetch('/led_states').then(function(r){return r.json();
}).then(function(j){if(!j||!j.length)return;
g_led_states=j;
onLedStateSel();
});
function onLedStateSel(){var id=+document.getElementById('ls_sel').value;
var s=g_led_states[id];
if(!s)return;
var hex='#'+rh(s.r)+rh(s.g)+rh(s.b);
document.getElementById('ls_pick').value=hex;
document.getElementById('ls_prev').style.background='rgb('+s.r+','+s.g+','+s.b+')';
document.getElementById('ls_eff').value=s.eff;
}
function saveLedState(){var id=+document.getElementById('ls_sel').value;
var v=document.getElementById('ls_pick').value;
var r=parseInt(v.substr(1,2),16),g=parseInt(v.substr(3,2),16),b=parseInt(v.substr(5,2),16);
var eff=+document.getElementById('ls_eff').value;
document.getElementById('ls_prev').style.background='rgb('+r+','+g+','+b+')';
g_led_states[id]={r:r,g:g,b:b,eff:eff};
fetch('/led_states',{method:'POST',body:'id='+id+'&r='+r+'&g='+g+'&b='+b+'&eff='+eff});
}
function onLedStatePick(){saveLedState();
}
function onLedStateEff(){saveLedState();
}
function onPower(action){var res=document.getElementById('power_res');
res.textContent=t('executing');
res.style.color='var(--text3)';
fetch('/power',{method:'POST',body:'action='+action}).then(function(r){return r.json();
}).then(function(j){if(j.ok){res.textContent=action==='shutdown'?t('shut_down'):action==='screen_off'?t('screen_off_done'):action==='wake'?t('wake_done'):t('screen_on_done');
res.style.color='var(--green)';
setTimeout(function(){res.textContent='';
},3000);
}else{res.textContent=t('failed');
res.style.color='#e53935';
}});
}
var g_presets=['今天','明天','后天','每天','工作日','周末','自定义'];
var g_wdays_cn=['一','二','三','四','五','六','日'];
var g_wdays_en=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
function spin(dir,id){var sp=document.getElementById('sp_'+id);
if(!sp)return;
var parts=id.split('_');
var max=parts[1]==='h'?23:59;
var el=sp.querySelector('.sp-val');
var v=(parseInt(el.value||0))+dir;
if(v<0)v=max;
if(v>max)v=0;
el.value=(v<10?'0':'')+v;
el.setAttribute('data-v',v);
}
function stepperVal(id){var sp=document.getElementById('sp_'+id);
if(!sp)return 0;
var el=sp.querySelector('.sp-val');
return el?parseInt(el.getAttribute('data-v')||0):0;
}
function setStepperHm(id,val){var sp=document.getElementById('sp_'+id);
if(!sp)return;
var el=sp.querySelector('.sp-val');
if(el){el.value=(val<10?'0':'')+val;
el.setAttribute('data-v',val);
}}
function stepperInput(id){var sp=document.getElementById('sp_'+id);
if(!sp)return;
var parts=id.split('_');
var max=parts[1]==='h'?23:59;
var el=sp.querySelector('.sp-val');
if(!el)return;
var v=parseInt(el.value);
if(isNaN(v)||v<0)v=0;
if(v>max)v=max;
el.value=(v<10?'0':'')+v;
el.setAttribute('data-v',v);
}
var g_day_mode={off:0,on:0};
var g_day_mask={off:0,on:0};
function daySelectPreset(prefix,mode){g_day_mode[prefix]=mode;
var mask=0;
if(mode==3)mask=0x7F;
else if(mode==4)mask=0x1F;
else if(mode==5)mask=0x60;
else if(mode>=6&&mode<=12){mask=1<<(mode==12?6:mode-6);
}else if(mode==13)mask=g_day_mask[prefix];
g_day_mask[prefix]=mask;
updateDayUI(prefix);
}
function dayToggleWday(prefix,bit){g_day_mode[prefix]=13;
g_day_mask[prefix]^=(1<<bit);
updateDayUI(prefix);
}
function updateDayUI(prefix){var mode=g_day_mode[prefix],mask=g_day_mask[prefix];
var btns=document.querySelectorAll('#'+prefix+'_presets .dp-btn');
btns.forEach(function(b){var v=+b.getAttribute('data-v');
b.classList.toggle('active',v===mode);
});
var wds=document.querySelectorAll('#'+prefix+'_wdays .wd-btn');
wds.forEach(function(b){var v=+b.getAttribute('data-v');
b.classList.toggle('active',!!(mask&(1<<v)));
});
}
function setupDayPickers(){var isZh=document.documentElement.lang.indexOf('zh')===0;
['off','on'].forEach(function(px){document.getElementById(px+'_presets').addEventListener('click',function(e){var b=e.target.closest('.dp-btn');
if(!b)return;
daySelectPreset(px,+b.getAttribute('data-v'));
});
document.getElementById(px+'_wdays').addEventListener('click',function(e){var b=e.target.closest('.wd-btn');
if(!b)return;
dayToggleWday(px,+b.getAttribute('data-v'));
});
var pbs=document.getElementById(px+'_presets').querySelectorAll('.dp-btn');
var gdl=isZh?['今天','明天','后天','每天','工作日','周末','自定义']:['Today','Tmrw','D+2','Every','Wdys','Wknd','Custom'];
pbs.forEach(function(b,i){b.textContent=gdl[i]||b.textContent;
});
var wbs=document.getElementById(px+'_wdays').querySelectorAll('.wd-btn');
var wdl=isZh?['一','二','三','四','五','六','日']:['Mo','Tu','We','Th','Fr','Sa','Su'];
wbs.forEach(function(b,i){b.textContent=wdl[i];
});
});
document.getElementById('btn_save_off_lbl').textContent=isZh?'保存关机':'Save Off';
document.getElementById('btn_save_on_lbl').textContent=isZh?'保存开机':'Save On';
}
function toggleSched(which){var el=document.getElementById('sched_'+which+'_en');
fetch('/schedule',{method:'POST',body:which+'_en='+(el.checked?1:0)});
}
function saveOffSchedule(){var res=document.getElementById('sched_off_res');
var oh=stepperVal('off_h'),om=stepperVal('off_m');
var ts=(oh<10?'0':'')+oh+':'+(om<10?'0':'')+om;
res.textContent=t('saving')+' '+ts+'...';
res.style.color='var(--text3)';
var el=document.getElementById('sched_off_en');
el.checked=true;
var body='off_en=1'+'&off_day='+g_day_mode.off+'&off_h='+oh+'&off_m='+om+'&off_days='+g_day_mask.off;
fetch('/schedule',{method:'POST',body:body}).then(function(r){return r.json();
}).then(function(j){if(j.ok){setStepperHm('off_h',j.off_h);
setStepperHm('off_m',j.off_m);
g_day_mode.off=j.off_day;
var ts2=(j.off_h<10?'0':'')+j.off_h+':'+(j.off_m<10?'0':'')+j.off_m;
res.textContent=t('saved')+': '+ts2;
res.style.color='var(--green)';
updateSchedStatus(j);
setTimeout(function(){res.textContent='';
},3000);
}else{res.textContent=t('failed');
res.style.color='#e53935';
}});
}
function saveOnSchedule(){var res=document.getElementById('sched_on_res');
var oh=stepperVal('on_h'),om=stepperVal('on_m');
var ts=(oh<10?'0':'')+oh+':'+(om<10?'0':'')+om;
res.textContent=t('saving')+' '+ts+'...';
res.style.color='var(--text3)';
var el=document.getElementById('sched_on_en');
el.checked=true;
var body='on_en=1'+'&on_day='+g_day_mode.on+'&on_h='+oh+'&on_m='+om+'&on_days='+g_day_mask.on;
fetch('/schedule',{method:'POST',body:body}).then(function(r){return r.json();
}).then(function(j){if(j.ok){setStepperHm('on_h',j.on_h);
setStepperHm('on_m',j.on_m);
g_day_mode.on=j.on_day;
var ts2=(j.on_h<10?'0':'')+j.on_h+':'+(j.on_m<10?'0':'')+j.on_m;
res.textContent=t('saved')+': '+ts2;
res.style.color='var(--green)';
updateSchedStatus(j);
setTimeout(function(){res.textContent='';
},3000);
}else{res.textContent=t('failed');
res.style.color='#e53935';
}});
}
function loadSchedule(){fetch('/schedule').then(function(r){return r.json();
}).then(function(j){if(!j)return;
var el=document.getElementById('sched_off_en');
if(el&&!el._init){el.checked=j.off_en!=0;
el._init=true;
}el=document.getElementById('sched_on_en');
if(el&&!el._init){el.checked=j.on_en!=0;
}
if(j.off_h!==undefined)setStepperHm('off_h',j.off_h);
if(j.off_m!==undefined)setStepperHm('off_m',j.off_m);
if(j.on_h!==undefined)setStepperHm('on_h',j.on_h);
if(j.on_m!==undefined)setStepperHm('on_m',j.on_m);
if(j.off_day!==undefined){g_day_mode.off=j.off_day;
}
if(j.off_days!==undefined){g_day_mask.off=j.off_days;
}
if(j.on_day!==undefined){g_day_mode.on=j.on_day;
}
if(j.on_days!==undefined){g_day_mask.on=j.on_days;
}updateDayUI('off');
updateDayUI('on');
updateSchedStatus(j);
});
}
var g_day_names=['今天','明天','后天','每天','工作日','周末','周一','周二','周三','周四','周五','周六','周日','自定义'];
function schedTimeStr(day,h,m,en){if(!en)return null;
var ts=(h<10?'0':'')+h+':'+(m<10?'0':'')+m;
var dn=g_day_names[day]||('mode'+day);
return dn+' '+ts;
}
function cancelSched(which){var el=document.getElementById('sched_'+which+'_en');
if(el)el.checked=false;
fetch('/schedule',{method:'POST',body:which+'_en=0'}).then(function(r){return r.json();
}).then(function(j){if(j.ok){loadSchedule();
}});
}
function updateSchedStatus(j){var el=document.getElementById('sched_status');
if(!el)return;
var parts=[];
if(j.off_en){var s=schedTimeStr(j.off_day,j.off_h,j.off_m,true);
if(s)parts.push('关机: '+s+" <span onclick=event.stopPropagation();
cancelSched('off') style=cursor:pointer;
color:var(--red);
font-size:14px title='取消'>&#10005;
</span>");
}
if(j.on_en){var s=schedTimeStr(j.on_day,j.on_h,j.on_m,true);
if(s)parts.push('开机: '+s+" <span onclick=event.stopPropagation();
cancelSched('on') style=cursor:pointer;
color:var(--red);
font-size:14px title='取消'>&#10005;
</span>");
}
if(parts.length){el.innerHTML='⏰ '+parts.join(' &nbsp;
|&nbsp;
 ');
el.style.display='block';
}else{el.style.display='none';
}}setupDayPickers();
loadSchedule();
function doOta(){var f=document.getElementById('ota_file').files[0];
if(!f){var r=document.getElementById('ota_res');
r.textContent=t('pick_bin');
r.style.color='#e53935';
return;
}
var b=document.getElementById('ota_btn'),r=document.getElementById('ota_res');
b.disabled=true;
b.textContent=t('uploading');
r.textContent='';
var x=new XMLHttpRequest();
x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);
document.getElementById('ota_prog').style.display='';
document.getElementById('ota_bar').style.width=p+'%';
document.getElementById('ota_pct').textContent=p+'%';
}};
x.onload=function(){if(x.status===200){r.textContent=t('ota_ok');
r.style.color='var(--green)';
setTimeout(function(){location.reload();
},8000);
}else{r.textContent=t('ota_fail')+' ('+x.status+')';
r.style.color='#e53935';
b.disabled=false;
b.textContent=t('upload_btn');
}};
x.onerror=function(){r.textContent=t('network_err');
r.style.color='#e53935';
b.disabled=false;
b.textContent=t('upload_btn');
};
x.open('POST','/update',true);
x.send(f);
}/* ── Sensor history chart ── */var g_chart=null,g_metric=0,g_range=1;
var g_colors=['#f59e0b','#6366f1','#0891b2','#8b5cf6'];
var g_bg=['rgba(245,158,11,.1)','rgba(99,102,241,.1)','rgba(8,145,178,.1)','rgba(139,92,246,.1)'];
var g_label_zh=['温度 (°C)','TVOC (μg/m³)','CO₂ (ppm)','甲醛 (μg/m³)'];
var g_label_en=['Temperature (°C)','TVOC (μg/m³)','CO₂ (ppm)','CH₂O (μg/m³)'];
var g_title_zh=['温度趋势','TVOC 趋势','CO₂ 趋势','甲醛趋势'];
var g_title_en=['Temperature Trend','TVOC Trend','CO₂ Trend','CH₂O Trend'];
function openHistory(m){g_metric=m;
g_range=1;
document.getElementById('mod_ov').classList.add('show');
var lang=localStorage.lang||'zh';
document.getElementById('mod_title').textContent=(lang==='zh'?g_title_zh:g_title_en)[m];
document.querySelectorAll('.r-btn').forEach(function(b){b.classList.remove('active');
});
var b=document.querySelector('.r-btn[data-h="1"]');
if(b)b.classList.add('active');
loadHistory();
}
function closeHistory(e){if(e&&e.target!==document.getElementById('mod_ov'))return;
document.getElementById('mod_ov').classList.remove('show');
if(g_chart){g_chart.destroy();
g_chart=null;
}}
function setRange(h){g_range=h;
document.querySelectorAll('.r-btn').forEach(function(b){b.classList.remove('active');
});
var b=document.querySelector('.r-btn[data-h="'+h+'"]');
if(b)b.classList.add('active');
loadHistory();
}
function loadHistory(){if(typeof Chart==='undefined'){console.log('Chart.js not loaded');
return;
}
var c=document.getElementById('hist_canvas');
if(g_chart){g_chart.destroy();
g_chart=null;
}
var emp=document.getElementById('hist_empty');
fetch('/history?hours='+g_range).then(function(r){return r.json();
}).then(function(d){if(!d||!d.length){if(emp){c.style.display='none';
emp.style.display='flex';
}
return;
}c.style.display='block';
if(emp)emp.style.display='none';
var labels=[],vals=[],key=g_metric===0?'t':g_metric===1?'tvoc':g_metric===2?'co2':'ch2o';
var lang=localStorage.lang||'zh',lbls=lang==='zh'?g_label_zh:g_label_en;
for(var i=0;
i<d.length;
i++){labels.push(fmtTime(d[i].ts));
vals.push(g_metric===0?d[i].t:d[i][key]);
}
var co=g_colors[g_metric];
g_chart=new Chart(c,{type:'line',data:{labels:labels,datasets:[{label:lbls[g_metric],data:vals,borderColor:co,backgroundColor:g_bg[g_metric],borderWidth:2,pointRadius:0,pointHoverRadius:5,pointHoverBorderWidth:2,pointHoverBorderColor:co,tension:.3,fill:true}]},options:{responsive:true,maintainAspectRatio:false,interaction:{mode:'index',intersect:false},scales:{x:{ticks:{maxTicksLimit:12,color:'#81a089'},grid:{color:'rgba(129,160,137,.12)'}},y:{ticks:{color:'#81a089'},grid:{color:'rgba(129,160,137,.12)'},beginAtZero:g_metric!==0}},plugins:{legend:{display:false},tooltip:{backgroundColor:'rgba(30,40,35,.92)',titleColor:'#a0b8a5',bodyColor:'#fff',borderColor:co,borderWidth:1,cornerRadius:6,displayColors:false,callbacks:{label:function(ctx){return ctx.dataset.label+': '+ctx.raw;
}}}}});
}).catch(function(e){console.log('history fetch error',e);
if(emp){c.style.display='none';
emp.style.display='flex';
}});
}
function fmtTime(ts){var d=new Date(ts*1000),h=d.getHours(),m=d.getMinutes();
return (h<10?'0':'')+h+':'+(m<10?'0':'')+m;
}