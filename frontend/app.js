const invoke = window.__TAURI__.core.invoke;
const error = document.querySelector('#error');
async function refresh() {
  try { document.querySelector('#disks').textContent = JSON.stringify(await invoke('disks'), null, 2); }
  catch (e) { error.textContent = String(e); }
}
document.querySelector('#refresh').onclick = refresh;
invoke('packages').then(items => {
  for (const item of items) {
    const label = document.createElement('label');
    const input = document.createElement('input'); input.type = 'checkbox'; input.value = item.id;
    label.append(input, document.createTextNode(`${item.name} — ${item.description}`));
    document.querySelector('#packages').append(label);
  }
}).catch(e => { error.textContent = String(e); });
refresh();
