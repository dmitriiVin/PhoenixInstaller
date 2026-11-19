const invoke = window.__TAURI__.core.invoke;
const titles = ['Добро пожаловать', 'Диск', 'Способ установки', 'Раздел Windows', 'Программы', 'Настройка компьютера', 'Подтверждение'];
const state = {device:'', mode:'reinstall_windows', windows_size_gb:150, edition:2, drivers:true, change_name:false, computer_name:'', packages:[]};
let step=0, catalog=[], diskList=[];
const page=document.querySelector('#page'), error=document.querySelector('#error');
function element(tag, text, parent=page) { const e=document.createElement(tag); if(text) e.textContent=text; parent.append(e); return e; }
function input(label, key, type='text') { const l=element('label',label); const i=element('input','',l); i.type=type; if(type==='checkbox') i.checked=state[key]; else i.value=state[key]; i.oninput=()=>{state[key]=type==='checkbox'?i.checked:type==='number'?Number(i.value):i.value;}; return i; }
async function render() {
  error.textContent=''; page.replaceChildren(); document.querySelector('#steps').replaceChildren();
  titles.forEach((t,i)=>{const li=element('li',t,document.querySelector('#steps')); li.style.color=i===step?'#ac92ff':'';});
  element('h2',titles[step]); document.querySelector('#back').disabled=step===0; document.querySelector('#next').hidden=step===6;
  if(step===0) element('p','Установка Windows, драйверов, программ и настройка системы.');
  if(step===1) {
    const refresh=element('button','Обновить диски'); refresh.onclick=async()=>{try { const data=await invoke('disks'); diskList=data.blockdevices.filter(d=>d.type==='disk'); render(); } catch(e){error.textContent=String(e);} };
    for(const d of diskList) {const l=element('label',`${d.model||d.name} · ${(Number(d.size)/2**30).toFixed(1)} ГиБ · ${d.tran||''}`);const radio=element('input','',l);radio.type='radio';radio.name='disk';radio.checked=state.device===d.name;radio.onchange=()=>{state.device=d.name;};}
    if(!diskList.length) element('p','Нажмите «Обновить диски». Обнаружение доступно в Linux Live.');
  }
  if(step===2) for(const [value,title,description] of [
    ['reinstall_windows','Переустановить Windows','Форматирование C:. Данные D: и AMDZ сохраняются.'],
    ['reinstall_windows_and_format_data','Полная очистка и переустановка','C: размером 120 ГиБ, остальное — D:. AMDZ сохраняется.'],
    ['clean_disk','Ручная разметка','Выберите размер C:. Остальное — D:. AMDZ сохраняется.']]) {
      const l=element('label',title);element('p',description,l);const i=element('input','',l);i.type='radio';i.name='mode';i.checked=state.mode===value;i.onchange=()=>{state.mode=value;};
    }
  if(step===3) {if(state.mode==='clean_disk'){const i=input('Размер Windows, ГиБ','windows_size_gb','number');i.min='100';}else element('p',state.mode==='reinstall_windows'?'Используется существующий раздел Windows.':'Размер Windows: 120 ГиБ.');}
  if(step===4) {for(const p of catalog){const l=element('label',`${p.name} — ${p.description}`);const i=element('input','',l);i.type='checkbox';i.checked=state.packages.includes(p.id);i.onchange=()=>{state.packages=state.packages.filter(id=>id!==p.id);if(i.checked)state.packages.push(p.id);};}}
  if(step===5) {input('Изменить имя компьютера','change_name','checkbox');input('Имя компьютера','computer_name').maxLength=15;}
  if(step===6) {try{
    const config=await invoke('preview_config',{settings:state});
    if(step!==6)return;
    const summary=element('dl');summary.className='summary';
    const row=(title,value)=>{element('dt',title,summary);element('dd',value,summary);};
    const disk=diskList.find(d=>d.name===state.device);
    const size=bytes=>`${(Number(bytes)/2**30).toLocaleString('ru-RU',{maximumFractionDigits:1})} ГиБ`;
    row('Диск',disk?`${disk.model||disk.name} · /dev/${disk.name} · ${size(disk.size)}`:`/dev/${state.device}`);
    row('Способ установки',({reinstall_windows:'Переустановка Windows',reinstall_windows_and_format_data:'Полная очистка и переустановка',clean_disk:'Ручная разметка'})[state.mode]);
    row('Раздел Windows (C:)',state.mode==='reinstall_windows'?'Существующий раздел будет отформатирован':`${state.mode==='clean_disk'?state.windows_size_gb:120} ГиБ`);
    row('Данные (D:)',state.mode==='reinstall_windows'?'Сохраняются':'Будут удалены; раздел займёт оставшееся место');
    row('Имя компьютера',config.computer.change_name?config.computer.name:'Без изменений');
    row('Драйверы','Будут установлены автоматически');
    row('Программы',state.packages.length?state.packages.map(id=>catalog.find(p=>p.id===id)?.name||id).join(', '):'Не выбраны');
    if(state.mode!=='reinstall_windows'){
      const plan=await invoke('preview_partition_plan',{settings:state});
      if(step!==6)return;
      element('h3','Разметка после установки');
      const table=element('table');table.className='partition-table';
      const header=element('tr','',element('thead','',table));
      for(const title of ['Раздел','Размер'])element('th',title,header);
      const body=element('tbody','',table);
      const partition=(name,sectors)=>{const tr=element('tr','',body);element('td',name,tr);element('td',size(sectors*512),tr);};
      if(plan.table==='dos'){partition('Windows (C:)',plan.windows.sectors);partition('Data (D:)',plan.data.sectors);}
      else{for(const p of plan.create)partition(({EFI:'EFI — загрузчик',MSR:'MSR — служебный',Windows:'Windows (C:)',Data:'Data (D:)'})[p.name]||p.name,p.end-p.start+1);
        if(plan.preserved.length)element('p',`Сохраняемые разделы (AMDZ и служебный, если найден): № ${plan.preserved.join(', № ')}`);
      }
    }
    element('p',state.mode==='reinstall_windows'?'Будут удалены данные на разделе Windows (C:). После успешной установки компьютер перезагрузится.':'Данные на C: и D: будут удалены. После успешной установки компьютер перезагрузится.').className='installation-notice';
    const start=element('button','Начать установку');start.onclick=async()=>{
 const back=document.querySelector('#back');start.disabled=true;back.disabled=true;
 const bar=element('progress');bar.max=1;bar.value=0;const status=element('p','Подготовка установки');
 let unlisten;
 try{unlisten=await window.__TAURI__.event.listen('install-progress',e=>{bar.value=e.payload.progress;status.textContent=e.payload.status;});await invoke('start_install',{settings:state});status.textContent='Установка завершена';}
 catch(e){error.textContent=String(e);start.disabled=false;back.disabled=false;}
 finally{if(unlisten)unlisten();}
};}catch(e){error.textContent=String(e);}}
}
document.querySelector('#back').onclick=()=>{if(step>0){step--;render();}};
document.querySelector('#next').onclick=async()=>{if(step===1&&!state.device){error.textContent='Выберите диск';return;}if(step===5){try{await invoke('preview_config',{settings:state});}catch(e){error.textContent=String(e);return;}}if(step<6){step++;render();}};
invoke('packages').then(p=>{catalog=p;render();}).catch(e=>{error.textContent=String(e);});render();
