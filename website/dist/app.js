const preview = document.querySelector('#coverflow');
const toggle = document.querySelector('#motion');
const preference = window.matchMedia('(prefers-reduced-motion: reduce)');
let playing = false;
function setMotion(enabled) {
  playing = enabled;
  preview.src = enabled ? 'assets/coverflow.gif' : 'assets/coverflow.png';
  toggle.textContent = enabled ? 'Animation pausieren' : 'Animation starten';
  toggle.setAttribute('aria-pressed', String(enabled));
}
toggle.hidden = false;
toggle.addEventListener('click', () => setMotion(!playing));
preference.addEventListener('change', event => setMotion(!event.matches));
setMotion(!preference.matches);
