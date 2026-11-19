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
  if(step===3) {if(state.mode==='clean_disk'){const i=input('Размер Windows, ГиБ','windows_size_gb','number');i.min='100';}else element('p',state.mode==='reinstall_windows'?'Используется существующий раздел Windows.':'Размер Windows: 120 ГиБ.');input('Индекс редакции в образе Windows','edition','number').min='1';}
  if(step===4) {input('Устанавливать драйверы','drivers','checkbox'); for(const p of catalog){const l=element('label',`${p.name} — ${p.description}`);const i=element('input','',l);i.type='checkbox';i.checked=state.packages.includes(p.id);i.onchange=()=>{state.packages=state.packages.filter(id=>id!==p.id);if(i.checked)state.packages.push(p.id);};}}
  if(step===5) {input('Изменить имя компьютера','change_name','checkbox');input('Имя компьютера','computer_name').maxLength=15;}
  if(step===6) {element('pre',JSON.stringify(state,null,2));try{const config=await invoke('preview_config',{settings:state});if(state.mode!=='reinstall_windows'){const plan=await invoke('preview_partition_plan',{settings:state});element('h3','План разметки (предпросмотр)');element('pre',JSON.stringify(plan,null,2));}element('h3','Конфигурация после установки');element('pre',JSON.stringify(config,null,2));element('p','Запуск установки станет доступен после переноса установочного движка.');}catch(e){error.textContent=String(e);}}
}
document.querySelector('#back').onclick=()=>{if(step>0){step--;render();}};
document.querySelector('#next').onclick=async()=>{if(step===1&&!state.device){error.textContent='Выберите диск';return;}if(step===5){try{await invoke('preview_config',{settings:state});}catch(e){error.textContent=String(e);return;}}if(step<6){step++;render();}};
invoke('packages').then(p=>{catalog=p;render();}).catch(e=>{error.textContent=String(e);});render();
