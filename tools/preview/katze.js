/* Katze drawing reference. Shared by the web preview and used by the firmware as the
   porting reference: every motion, fx and prop is one entry keyed by its registry id.
   Stage: 200 × 200 disc (centre 100,100, radius 100). Basic shapes only: paths, ellipses,
   rects, round-capped lines. No bitmaps, gradients or filters.

   Coordinates of the cat ("body frame"): feet on y = 0, centre x = 0, ear tips at y ≈ -134.
   Paws are placed by target points (hLx, hLy, hRx, hRy) and reached with a two-bone arm
   (shoulders at ±24, -56; upper and lower arm 13.5 each; elbow bends outwards).

   Frames:
     root  = translate(x, y - jump) scale(sx, sy)      feet, floor props attached to the cat
     body  = root · rotate(rot, 0, -10)                everything else, held props
     scene = translate(STAGE)                          props standing on the stage (layer "back")
*/

const NS = 'http://www.w3.org/2000/svg';
const clamp = (v, a = 0, b = 1) => Math.max(a, Math.min(b, v));
const ease = u => -(Math.cos(Math.PI * u) - 1) / 2;
const backOut = u => { const c = 1.7; return 1 + (c + 1) * Math.pow(u - 1, 3) + c * Math.pow(u - 1, 2); };
function el(tag, attrs, parent) { const e = document.createElementNS(NS, tag); for (const k in attrs) e.setAttribute(k, attrs[k]); if (parent) parent.appendChild(e); return e; }
const hex = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
const mixC = (a, b, u) => { const A = hex(a), B = hex(b); return 'rgb(' + A.map((v, i) => Math.round(v + (B[i] - v) * u)).join(',') + ')'; };
const f1 = v => v.toFixed(1);

/* ---------- character ---------- */
const KZ = { base: '#F3A25A', edge: '#C9772F', belly: '#FFF1DE', ear: '#F7B9A6', ink: '#2B2230', mouth: '#8C3B3B', nose: '#E8807A', blush: '#F49A94' };
/* neutral pose: the registry lists these parameters once the firmware implements them */
const N0 = { x: 0, y: 0, jump: 0, rot: 0, sx: 1, sy: 1,
  open: 1, lid: 0, happy: 0, shut: 0, lookX: 0, lookY: 0, smile: 0.25, mopen: 0, blush: 0,
  earL: 0, earR: 0, tail: 0,
  hLx: -28, hLy: -31, hRx: 28, hRy: -31, fLx: -12, fLy: 0, fRx: 12, fRy: 0 };
const SHOULDER = 24, SHOULDER_Y = -56, ARM = 13.5;
const STW = { 'stroke-width': 2.2, 'stroke-linejoin': 'round', 'stroke-linecap': 'round' };
const fillStroke = (f, s, w) => Object.assign({ fill: f, stroke: s }, STW, w ? { 'stroke-width': w } : {});

/* tapered limb between two points, as one closed path */
function limb(x0, y0, x1, y1, w0, w1) {
  const dx = x1 - x0, dy = y1 - y0, L = Math.hypot(dx, dy) || 1, nx = -dy / L, ny = dx / L;
  return `M${f1(x0 + nx * w0)} ${f1(y0 + ny * w0)} L${f1(x1 + nx * w1)} ${f1(y1 + ny * w1)} A${w1} ${w1} 0 0 0 ${f1(x1 - nx * w1)} ${f1(y1 - ny * w1)} L${f1(x0 - nx * w0)} ${f1(y0 - ny * w0)} A${w0} ${w0} 0 0 0 ${f1(x0 + nx * w0)} ${f1(y0 + ny * w0)} Z`;
}
/* two-bone arm: returns [elbow, paw]; the paw is pulled in if the target is out of reach */
function reach(side, tx, ty) {
  const sx = side * SHOULDER, sy = SHOULDER_Y;
  let dx = tx - sx, dy = ty - sy, d = Math.hypot(dx, dy); const m = 2 * ARM - 0.5;
  if (d > m) { tx = sx + dx / d * m; ty = sy + dy / d * m; d = m; }
  if (d < 4) { d = 4; }
  const base = Math.atan2(ty - sy, tx - sx), a = Math.acos(clamp(d / (2 * ARM), -1, 1));
  const e1 = [sx + ARM * Math.cos(base + a), sy + ARM * Math.sin(base + a)], e2 = [sx + ARM * Math.cos(base - a), sy + ARM * Math.sin(base - a)];
  return [e1[0] * side > e2[0] * side ? e1 : e2, [tx, ty]];
}

function makeCat(parent) {
  const R = {};
  R.root = el('g', {}, parent);
  R.legs = [0, 1].map(() => el('path', fillStroke(KZ.base, KZ.edge), R.root));
  R.feet = [0, 1].map(() => el('ellipse', Object.assign({ rx: 9, ry: 5.5 }, fillStroke(KZ.belly, KZ.edge)), R.root));
  const b = R.body = el('g', {}, R.root);
  R.tail = el('g', {}, b);
  el('path', { d: 'M0 0 C26 -2 30 -32 18 -46', fill: 'none', stroke: KZ.edge, 'stroke-width': 10, 'stroke-linecap': 'round' }, R.tail);
  el('path', { d: 'M0 0 C26 -2 30 -32 18 -46', fill: 'none', stroke: KZ.base, 'stroke-width': 5.6, 'stroke-linecap': 'round' }, R.tail);
  R.ears = [-1, 1].map(s => {
    const g = el('g', {}, b);
    el('path', Object.assign({ d: `M${s * 30} -100 L${s * 27} -134 L${s * 6} -116 Z` }, fillStroke(KZ.base, KZ.edge)), g);
    el('path', { d: `M${s * 25} -106 L${s * 24} -124 L${s * 13} -114 Z`, fill: KZ.ear }, g);
    return g;
  });
  R.torso = el('path', Object.assign({ d: 'M-31 -94 C-35 -60 -32 -22 -20 -9 C-8 0 8 0 20 -9 C32 -22 35 -60 31 -94 C27 -122 -27 -122 -31 -94 Z' }, fillStroke(KZ.base, KZ.edge)), b);
  el('ellipse', { cx: 0, cy: -34, rx: 18, ry: 20, fill: KZ.belly }, b);
  [[-7, -109], [0, -111], [7, -109]].forEach(([x, y]) => el('path', { d: `M${x} ${y} l0 7`, stroke: KZ.edge, 'stroke-width': 3, 'stroke-linecap': 'round' }, b));
  R.blush = [-20, 20].map(x => el('ellipse', { cx: x, cy: -77, rx: 5, ry: 2.8, fill: KZ.blush }, b));
  R.eyes = [-11, 11].map(x => {
    const e = { g: el('g', { transform: `translate(${x} -86)` }, b) };
    e.ell = el('ellipse', { rx: 4, ry: 4.9, fill: KZ.ink }, e.g);
    e.hl = el('circle', { r: 1.5, fill: '#fff' }, e.g);
    e.lid = el('rect', { x: -5.5, y: -7, width: 11, height: 0, fill: KZ.base }, e.g);
    e.happy = el('path', { d: 'M-4.5 1 Q0 -5 4.5 1', fill: 'none', stroke: KZ.ink, 'stroke-width': 2.2, 'stroke-linecap': 'round', opacity: 0 }, e.g);
    e.shut = el('path', { d: 'M-4.5 -1 Q0 3.5 4.5 -1', fill: 'none', stroke: KZ.ink, 'stroke-width': 2.2, 'stroke-linecap': 'round', opacity: 0 }, e.g);
    return e;
  });
  el('path', Object.assign({ d: 'M-2.6 -79 L2.6 -79 L0 -76 Z' }, fillStroke(KZ.nose, KZ.nose, 1.4)), b);
  R.open = el('path', { fill: KZ.mouth }, b);
  R.mouth = el('path', { fill: 'none', stroke: KZ.ink, 'stroke-width': 1.7, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, b);
  [[-16, -79, -29, -81], [-16, -75, -29, -73], [16, -79, 29, -81], [16, -75, 29, -73]].forEach(([a, c, d, e]) =>
    el('path', { d: `M${a} ${c} L${d} ${e}`, stroke: KZ.edge, 'stroke-width': 1.3, 'stroke-linecap': 'round' }, b));
  R.wear = el('g', {}, b);              /* worn props (scarf, jacket, thermometer): over the body, under the arms */
  R.arms = [0, 1, 2, 3].map(() => el('path', fillStroke(KZ.base, KZ.edge), b));
  R.hand = el('g', {}, b);              /* held props: between the arms and the paws */
  R.paws = [0, 1].map(() => el('ellipse', Object.assign({ rx: 6, ry: 5.4 }, fillStroke(KZ.base, KZ.edge)), b));
  return R;
}

function drawCat(R, p) {
  R.root.setAttribute('transform', `translate(${f1(p.x)} ${f1(p.y - p.jump)}) scale(${p.sx.toFixed(3)} ${p.sy.toFixed(3)})`);
  R.body.setAttribute('transform', `rotate(${p.rot.toFixed(2)} 0 -10)`);
  [[p.fLx, p.fLy, -11], [p.fRx, p.fRy, 11]].forEach(([x, y, hx], i) => {
    const lifted = y < -3 || Math.abs(x - hx) > 6;
    R.legs[i].setAttribute('d', lifted ? limb(hx, -12, x, y, 6.5, 6) : '');
    R.feet[i].setAttribute('cx', f1(x)); R.feet[i].setAttribute('cy', f1(y - 4));
  });
  R.tail.setAttribute('transform', `translate(20 -16) rotate(${p.tail.toFixed(1)})`);
  R.ears[0].setAttribute('transform', `rotate(${p.earL.toFixed(1)} -18 -104)`);
  R.ears[1].setAttribute('transform', `rotate(${(-p.earR).toFixed(1)} 18 -104)`);
  const vis = 1 - Math.max(p.happy, p.shut), ry = 4.9 * p.open;
  R.eyes.forEach(e => {
    e.ell.setAttribute('ry', ry.toFixed(2)); e.ell.setAttribute('cx', p.lookX.toFixed(2)); e.ell.setAttribute('cy', p.lookY.toFixed(2)); e.ell.setAttribute('opacity', vis.toFixed(2));
    e.hl.setAttribute('cx', (p.lookX + 1.4).toFixed(2)); e.hl.setAttribute('cy', (p.lookY - ry * 0.4).toFixed(2)); e.hl.setAttribute('opacity', vis.toFixed(2));
    e.lid.setAttribute('y', (p.lookY - ry - 0.8).toFixed(2)); e.lid.setAttribute('height', (p.lid * ry * 2 + 0.8).toFixed(2)); e.lid.setAttribute('opacity', (p.lid > 0.02 ? vis : 0).toFixed(2));
    e.happy.setAttribute('opacity', p.happy.toFixed(2)); e.shut.setAttribute('opacity', p.shut.toFixed(2));
  });
  const s = p.smile * 3.6;
  R.mouth.setAttribute('d', `M-5.5 -73 Q-2.7 ${f1(-73 + s)} 0 -73.5 Q2.7 ${f1(-73 + s)} 5.5 -73`);
  R.open.setAttribute('d', p.mopen > 0.02 ? `M-4.2 -72.6 Q0 ${f1(-72.6 + 11 * p.mopen)} 4.2 -72.6 Z` : '');
  R.blush.forEach(b => b.setAttribute('opacity', (0.45 + 0.45 * p.blush).toFixed(2)));
  const L = reach(-1, p.hLx, p.hLy), Rr = reach(1, p.hRx, p.hRy);
  [[L, -1, 0], [Rr, 1, 2]].forEach(([[e, w], side, k]) => {
    R.arms[k].setAttribute('d', limb(side * SHOULDER, SHOULDER_Y, e[0], e[1], 6, 5.4));
    R.arms[k + 1].setAttribute('d', limb(e[0], e[1], w[0], w[1], 5.4, 5));
  });
  R.paws[0].setAttribute('cx', f1(L[1][0])); R.paws[0].setAttribute('cy', f1(L[1][1]));
  R.paws[1].setAttribute('cx', f1(Rr[1][0])); R.paws[1].setAttribute('cy', f1(Rr[1][1]));
  return { L: L[1], R: Rr[1] };
}

/* frame conversions (see the header) */
function bodyToRoot(p, x, y) {
  const a = p.rot * Math.PI / 180, dy = y + 10;
  return [x * Math.cos(a) - dy * Math.sin(a), x * Math.sin(a) + dy * Math.cos(a) - 10];
}
function rootToBody(p, x, y) {
  const a = -p.rot * Math.PI / 180, dy = y + 10;
  return [x * Math.cos(a) - dy * Math.sin(a), x * Math.sin(a) + dy * Math.cos(a) - 10];
}
function rootToStage(p, x, y) { return [p.x + x * p.sx, p.y - p.jump + y * p.sy]; }
const bodyToStage = (p, x, y) => rootToStage(p, ...bodyToRoot(p, x, y));

/* ---------- shapes reused by effects and props ---------- */
const STAR = 'M0 -6 L1.6 -1.6 L6 0 L1.6 1.6 L0 6 L-1.6 1.6 L-6 0 L-1.6 -1.6 Z';
const DROP = 'M0 -5 C2 -1 3 1 3 2.5 A3 3 0 1 1 -3 2.5 C-3 1 -2 -1 0 -5 Z';
const FONT = 'Bricolage Grotesque, system-ui, sans-serif';

/* ---------- MOTION: (pose, clock, weight) -> mutates pose ---------- */
const MOTION = {
  'tail-wag': (p, c, w) => { p.tail += Math.sin(c * 3) * 14 * w; },
  'breathe':  (p, c, w) => { const k = 1 + Math.sin(c * 1.8) * 0.015 * w; p.sx *= k; p.sy *= k; },
  'bounce':   (p, c, w) => { const s = Math.abs(Math.sin(c * 5)); p.jump += w * s * 9; p.sy *= 1 - 0.06 * w * (1 - s); },
  'hop':      (p, c, w) => { p.jump += w * Math.max(0, Math.sin(c * 2.4)) * 3; },
  /* walking on the spot: feet step, body bobs */
  'walk': (p, c, w) => {
    const s = Math.sin(c * 6);
    p.jump += Math.abs(s) * 2.2 * w; p.rot += s * 2 * w;
    p.fLy -= Math.max(0, s) * 5 * w; p.fRy -= Math.max(0, -s) * 5 * w;
    p.hLy += Math.sin(c * 12) * 1.2 * w; p.hRy += Math.sin(c * 12) * 1.2 * w;
  },
  /* small circles of the right paw (sponge, towel) */
  'scrub': (p, c, w) => { p.hRx += Math.cos(c * 9) * 5 * w; p.hRy += Math.sin(c * 9) * 4 * w; p.rot += Math.sin(c * 9) * 0.8 * w; },
  /* both paws forwards and back along a slight diagonal (vacuum, iron) */
  'push': (p, c, w) => { const d = Math.sin(c * 3.2); p.hRx += d * 9 * w; p.hRy += d * 2.5 * w; p.hLx += d * 9 * w; p.hLy += d * 2.5 * w; p.rot += d * 2 * w; },
  /* wide side-to-side swing of both paws (mop) */
  'sweep': (p, c, w) => { const d = Math.sin(c * 2.6); p.hRx += d * 12 * w; p.hLx += d * 12 * w; p.rot += d * 3 * w; p.lookX += d * 0.8 * w; },
  /* gentle smoothing over a surface (bed cover) */
  'smooth': (p, c, w) => { const d = Math.sin(c * 2.2); p.hLx += d * 8 * w; p.hRx += d * 8 * w; p.rot += d * 1.5 * w; },
  /* right paw comes up to the target, holds, goes down again (hanging washing) */
  'reach-up': (p, c, w) => { const u = ease(clamp(Math.sin(c * 1.8) * 0.8 + 0.5)); p.hRy += (1 - u) * 22 * w; p.hRx -= (1 - u) * 6 * w; p.jump += u * 1.5 * w; p.lookY -= u * 0.8 * w; },
  /* tilts the held watering can; the prop reads p._tilt */
  'pour': (p, c, w) => { const u = ease(clamp(Math.sin(c * 1.6) * 0.9 + 0.55)); p._tilt = (p._tilt || 0) + u * 34 * w; p.hRy -= u * 2 * w; },
  /* eyes follow a spinning drum; head tilts along */
  'watch-spin': (p, c, w) => { p.lookX += Math.cos(c * 4) * 1.6 * w + 0.8 * w; p.lookY += Math.sin(c * 4) * 1.3 * w; p.rot += Math.sin(c * 4) * 1.6 * w; },
  /* new in a2-unterwegs-verben */
  /* feet circle round the bike's crank (stage 0, -14); use with pose jump ≈ 20 */
  'pedal': (p, c, w) => {
    const a = c * 7, cy = -14 + p.jump, mix = (v, t) => v + (t - v) * w;
    p.fLx = mix(p.fLx, Math.cos(a) * 7); p.fLy = mix(p.fLy, cy + Math.sin(a) * 7);
    p.fRx = mix(p.fRx, -Math.cos(a) * 7); p.fRy = mix(p.fRy, cy - Math.sin(a) * 7);
    p.jump += Math.abs(Math.sin(a)) * 1 * w;
  },
  /* running on the spot: bigger steps, arms swing, tail streams back */
  'run': (p, c, w) => {
    const s = Math.sin(c * 11);
    p.jump += Math.abs(s) * 4 * w; p.rot += s * 2 * w;
    p.fLx += s * 6 * w; p.fRx -= s * 6 * w; p.fLy -= Math.max(0, s) * 7 * w; p.fRy -= Math.max(0, -s) * 7 * w;
    p.hLx += s * 7 * w; p.hRx -= s * 7 * w; p.hLy -= Math.abs(s) * 3 * w; p.hRy -= Math.abs(s) * 3 * w;
    p.tail += (-20 + s * 10) * w;
  },
  /* impatient foot tapping */
  'tap': (p, c, w) => { p.fRy -= Math.max(0, Math.sin(c * 7)) * 4 * w; p.tail += Math.sin(c * 1.5) * 8 * w; },
  /* looking left and right, head tilts, ears turn */
  'look-around': (p, c, w) => { const s = Math.sin(c * 1.7); p.lookX += s * 2.2 * w; p.rot += s * 3 * w; p.earL += s * 6 * w; p.earR -= s * 6 * w; },
  'tremble': (p, c, w) => { p.x += Math.sin(c * 30) * 0.7 * w; },
  /* new in a2-einkaufen-verben and a2-termine-verben */
  'type':  (p, c, w) => { p.hRy += Math.max(0, Math.sin(c * 14)) * 2 * w; p.hRx += Math.sin(c * 7) * 1.5 * w; },
  'drop':  (p, c, w) => { const u = ease(clamp(Math.sin(c * 2) * 0.9 + 0.5)); p.hRy += u * 14 * w; p.hRx += u * 3 * w; p.lookY += u * 1.2 * w; },
  'knock': (p, c, w) => { const on = Math.sin(c * 1.6) > -0.2 ? 1 : 0; p.hRx += Math.max(0, Math.sin(c * 11)) * 4 * w * on; p.rot += Math.max(0, Math.sin(c * 11)) * 0.6 * w * on; },
  'write': (p, c, w) => { p.hRx += Math.sin(c * 12) * 2.5 * w; p.hRy += Math.sin(c * 6) * 1 * w; },
  /* new in the noun decks */
  'cough': (p, c, w) => { const u = (c % 1.8) / 1.8, k = u < 0.12 ? u / 0.12 : u < 0.3 ? 1 - (u - 0.12) / 0.18 : 0;
    p.rot += 4 * k * w; p.shut = Math.max(p.shut, k * w); p.mopen = Math.max(p.mopen, 0.5 * k * w); p.jump -= 1.5 * k * w; },
};

const txt = (g, x, y, t, size, fill, anchor) => { const e = el('text', { x, y, 'font-size': size, 'font-weight': 800, fill, 'font-family': FONT, 'text-anchor': anchor || 'middle' }, g); e.textContent = t; return e; };

/* wall calendar shared by 'calendar', 'calendar-move' and 'calendar-x' (scene frame) */
function calendarSheet(g, mode) {
  el('rect', { x: 40, y: -136, width: 60, height: 54, rx: 3, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.6 }, g);
  el('rect', { x: 40, y: -136, width: 60, height: 12, rx: 3, fill: '#E5484D' }, g);
  [52, 88].forEach(x => el('line', { x1: x, y1: -140, x2: x, y2: -131, stroke: '#6B7180', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g));
  const cell = (r, k) => [46 + k * 13, -118 + r * 11];
  for (let r = 0; r < 3; r++) for (let k = 0; k < 4; k++) { const [x, y] = cell(r, k); el('rect', { x, y, width: 9, height: 7, rx: 1, fill: '#EEF0F4' }, g); }
  const s = { mode };
  if (mode === 'circle') { const [x, y] = cell(1, 2); s.mark = el('ellipse', { cx: x + 4.5, cy: y + 3.5, rx: 8, ry: 6.5, fill: 'none', stroke: '#E5484D', 'stroke-width': 2, 'stroke-dasharray': '46', 'stroke-dashoffset': 46 }, g); }
  if (mode === 'move') { s.from = cell(0, 1); s.to = cell(2, 3);
    el('path', { d: `M${s.from[0] + 9} ${s.from[1] + 6} Q${s.to[0] - 2} ${s.from[1] + 4} ${s.to[0] + 2} ${s.to[1] - 2}`, fill: 'none', stroke: '#9AA4B1', 'stroke-width': 1.4, 'stroke-dasharray': '2 2' }, g);
    s.mark = el('rect', { width: 9, height: 7, rx: 1.5, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1 }, g); }
  if (mode === 'x') { const [x, y] = cell(1, 1); el('rect', { x, y, width: 9, height: 7, rx: 1, fill: '#FADBD8' }, g);
    s.mark = el('path', { d: `M${x - 1} ${y - 2} L${x + 10} ${y + 9} M${x + 10} ${y - 2} L${x - 1} ${y + 9}`, stroke: '#E5484D', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g); }
  return s;
}
function calendarTick(s, P) {
  const u = (P.c % 3) / 3;
  if (s.mode === 'circle') s.mark.setAttribute('stroke-dashoffset', f1(46 * (1 - clamp(u * 2))));
  if (s.mode === 'move') { const k = ease(clamp(u * 1.6 - 0.2)); s.mark.setAttribute('x', f1(s.from[0] + (s.to[0] - s.from[0]) * k)); s.mark.setAttribute('y', f1(s.from[1] + (s.to[1] - s.from[1]) * k)); }
  if (s.mode === 'x') s.mark.setAttribute('opacity', u < 0.15 ? (u / 0.15).toFixed(2) : 1);
}

/* ---------- PROP: { layer: 'back' | 'wear' | 'hand', make(group) -> state, tick(state, P) } ----------
   'wear' props are fixed to the body (scarf, jacket) and drawn under the arms.
   'back' props stand on the stage behind the cat (scene frame, feet at 0,0).
   'hand' props are drawn in the body frame between the arms and the paws.
   P = { p, c, L:[x,y], R:[x,y] (paws, body frame), pub } ; pub collects anchor points (stage coords) for fx. */
const PROP = {
  'bed': { layer: 'back', make: g => {
    el('rect', { x: 92, y: -64, width: 8, height: 64, rx: 3, fill: '#A86B48' }, g);
    el('rect', { x: 32, y: -8, width: 5, height: 8, fill: '#A86B48' }, g);
    el('rect', { x: 30, y: -30, width: 66, height: 22, rx: 4, fill: '#FFFFFF', stroke: '#D6D2E0', 'stroke-width': 1.6 }, g);
    el('ellipse', { cx: 83, cy: -36, rx: 10, ry: 6, fill: '#FFFFFF', stroke: '#D6D2E0', 'stroke-width': 1.6 }, g);
    el('path', { d: 'M26 -38 L72 -38 Q76 -24 72 -10 L26 -10 Q22 -24 26 -38 Z', fill: '#C9C2F2', stroke: '#9D8DF1', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M26 -38 L72 -38 L72 -32 L26 -32 Z', fill: '#FFFFFF', opacity: 0.85 }, g);
    return null; }, tick: () => {} },
  'window': { layer: 'back', make: g => {
    el('rect', { x: 40, y: -116, width: 52, height: 58, rx: 3, fill: '#DDF1FB', stroke: '#8FA3B8', 'stroke-width': 3 }, g);
    el('path', { d: 'M66 -116 L66 -58 M40 -87 L92 -87', stroke: '#8FA3B8', 'stroke-width': 2.4 }, g);
    el('path', { d: 'M46 -108 L54 -100 M48 -96 L60 -108', stroke: '#FFFFFF', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    el('rect', { x: 36, y: -58, width: 60, height: 5, rx: 2, fill: '#8FA3B8' }, g);
    return null; }, tick: () => {} },
  'washing-machine': { layer: 'back', make: g => {
    el('rect', { x: 44, y: -64, width: 50, height: 64, rx: 6, fill: '#FFFFFF', stroke: '#B9BFCC', 'stroke-width': 2 }, g);
    el('line', { x1: 44, y1: -52, x2: 94, y2: -52, stroke: '#B9BFCC', 'stroke-width': 1.6 }, g);
    el('circle', { cx: 84, cy: -58, r: 2.6, fill: '#9D8DF1' }, g); el('rect', { x: 50, y: -60, width: 14, height: 4, rx: 2, fill: '#D6DCE6' }, g);
    el('circle', { cx: 69, cy: -27, r: 18, fill: '#DCE3EC', stroke: '#8FA3B8', 'stroke-width': 3 }, g);
    const drum = el('g', {}, g);
    [['#E5484D', 0], ['#F2C14E', 2.1], ['#4BA3D9', 4.2]].forEach(([col, a]) => el('ellipse', { cx: 9 * Math.cos(a), cy: 9 * Math.sin(a), rx: 6, ry: 4, fill: col, transform: `rotate(${a * 57} ${9 * Math.cos(a)} ${9 * Math.sin(a)})` }, drum));
    el('path', { d: 'M54 -34 Q69 -24 84 -34', fill: 'none', stroke: '#9FC4E8', 'stroke-width': 2, opacity: 0.8 }, g);
    return { drum }; },
    tick: (s, P) => { s.drum.setAttribute('transform', `translate(69 -27) rotate(${f1((P.c * 260) % 360)})`); P.pub.drum = [P.stageX + 69, P.stageY - 27]; } },
  'drying-rack': { layer: 'back', make: g => {
    el('path', { d: 'M40 0 L82 -64 M96 0 L54 -64', stroke: '#9AA4B1', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    [-64, -56].forEach(y => el('line', { x1: 36, y1: y, x2: 98, y2: y, stroke: '#9AA4B1', 'stroke-width': 2, 'stroke-linecap': 'round' }, g));
    el('path', { d: 'M58 -64 L80 -64 L84 -54 L78 -52 L78 -30 L60 -30 L60 -52 L54 -54 Z', fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, g);
    el('rect', { x: 86, y: -56, width: 10, height: 22, rx: 1.5, fill: '#F2C14E' }, g);
    return null; }, tick: () => {} },
  'ironing-board': { layer: 'back', make: g => {
    el('path', { d: 'M46 0 L80 -40 M86 0 L52 -40', stroke: '#9AA4B1', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    el('path', { d: 'M28 -40 Q24 -46 34 -46 L94 -46 Q98 -46 98 -42 L98 -40 Z', fill: '#BFE3F2', stroke: '#7FA6C4', 'stroke-width': 1.8, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M60 -46 L78 -46 L80 -50 L72 -52 L66 -52 L58 -50 Z', fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.2 }, g);
    return null; }, tick: () => {} },
  'plant': { layer: 'back', make: g => {
    el('path', { d: 'M62 -22 L90 -22 L86 0 L66 0 Z', fill: '#D9824A', stroke: '#B0653A', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M76 -22 L76 -46', stroke: '#3E8E2F', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    el('path', { d: 'M76 -30 Q64 -34 62 -44 Q72 -42 76 -32 Z M76 -34 Q88 -38 90 -48 Q80 -46 76 -36 Z', fill: '#5E9E45' }, g);
    const fl = el('g', { transform: 'translate(76 -50)' }, g);
    [0, 72, 144, 216, 288].forEach(a => el('ellipse', { cx: 0, cy: -4.5, rx: 3.2, ry: 4.5, fill: '#F28CA8', transform: `rotate(${a})` }, fl));
    el('circle', { r: 2.8, fill: '#F2C14E' }, fl);
    return null; }, tick: () => {} },

  /* held props: drawn at the paw, in the body frame */
  'box': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('circle', { cx: -8, cy: -15, r: 6, fill: '#E5484D' }, s);
    el('rect', { x: 2, y: -24, width: 8, height: 14, rx: 1, fill: '#4BA3D9', transform: 'rotate(12 6 -17)' }, s);
    el('rect', { x: -20, y: -12, width: 40, height: 24, rx: 2, fill: '#D9A86C', stroke: '#B0803F', 'stroke-width': 1.6 }, s);
    el('path', { d: 'M-20 -12 L-26 -18 M20 -12 L26 -18', stroke: '#B0803F', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y + 4)})`); } },
  'sponge': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('rect', { x: -7, y: -5, width: 14, height: 10, rx: 2.5, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.2 }, s);
    el('rect', { x: -7, y: -5, width: 14, height: 3.5, rx: 1.5, fill: '#5E9E45' }, s);
    return s; },
    tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0] + 3)} ${f1(P.R[1] - 5)}) rotate(-20)`); P.pub.tool = bodyToStage(P.p, P.R[0] + 4, P.R[1] - 8); } },
  'towel': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-8 -6 L8 -6 L9 10 L-7 12 Z', fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.2, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-8 -1 L8 -1 M-7.5 4 L8.5 4', stroke: '#E5484D', 'stroke-width': 1.8 }, s);
    return s; },
    tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0] + 1)} ${f1(P.R[1] + 2)}) rotate(-10)`); P.pub.tool = bodyToStage(P.p, P.R[0], P.R[1]); } },
  'plate': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('circle', { r: 13, fill: '#FFFFFF', stroke: '#B9BFCC', 'stroke-width': 1.6 }, s);
    el('circle', { r: 8, fill: 'none', stroke: '#DCE3EC', 'stroke-width': 1.4 }, s);
    return s; },
    tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.L[0] + 10)} ${f1(P.L[1] - 4)})`); P.pub.plate = bodyToStage(P.p, P.L[0] + 10, P.L[1] - 4); } },
  'sock': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-4 0 L4 0 L4 14 Q4 18 10 18 L12 18 Q16 18 16 22 Q16 26 12 26 L2 26 Q-4 26 -4 20 Z', fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 1.2, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-4 5 L4 5 M-4 9 L4 9', stroke: '#FFFFFF', 'stroke-width': 1.6 }, s);
    el('rect', { x: -2, y: -6, width: 4, height: 8, rx: 1, fill: '#F2C14E' }, s);
    return s; },
    tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0])} ${f1(P.R[1] + 2)})`); } },
  'iron': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-12 4 L12 4 Q14 -4 6 -8 L-6 -8 Q-12 -6 -12 4 Z', fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-12 4 L12 4 L13 6 L-12 6 Z', fill: '#C9CED6' }, s);
    el('path', { d: 'M-5 -8 Q-5 -14 0 -14 L5 -14 Q8 -14 8 -8', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => { const [x, y] = [P.R[0] + 2, P.R[1] + 12]; s.setAttribute('transform', `translate(${f1(x)} ${f1(y)})`); P.pub.iron = bodyToStage(P.p, x + 8, y - 6); } },
  'trash-bag': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-3 2 Q-14 8 -14 20 Q-14 32 0 32 Q14 32 14 20 Q14 8 3 2 Z', fill: '#4A4F5C', stroke: '#2E323B', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-4 2 L-7 -4 M4 2 L7 -4', stroke: '#2E323B', 'stroke-width': 2.2, 'stroke-linecap': 'round' }, s);
    el('path', { d: 'M-8 16 Q-6 24 -2 26', fill: 'none', stroke: '#6B7180', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => { const sw = Math.sin(P.c * 6) * 6; s.setAttribute('transform', `translate(${f1(P.R[0])} ${f1(P.R[1] + 2)}) rotate(${f1(sw)})`); P.pub.bag = bodyToStage(P.p, P.R[0], P.R[1] + 4); } },
  'watering-can': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M10 -4 L30 -16 L32 -13 L14 2 Z', fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('ellipse', { cx: 31, cy: -15, rx: 2.6, ry: 4, fill: '#2F7DB8', transform: 'rotate(-30 31 -15)' }, s);
    el('path', { d: 'M-12 -10 L12 -10 L10 12 L-10 12 Z', fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-8 -10 Q0 -22 8 -10', fill: 'none', stroke: '#2F7DB8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => {
      const t = P.p._tilt || 0, x = P.R[0] - 2, y = P.R[1] + 18;
      s.setAttribute('transform', `translate(${f1(x)} ${f1(y)}) rotate(${f1(t)})`);
      const a = t * Math.PI / 180, sx = 31 * Math.cos(a) + 15 * Math.sin(a), sy = 31 * Math.sin(a) - 15 * Math.cos(a);
      P.pub.spout = bodyToStage(P.p, x + sx, y + sy); P.pub.pour = clamp((t - 12) / 18);
    } },
  /* held, but touching the floor: the head is placed on the floor in root coordinates */
  'vacuum': { layer: 'hand', make: g => {
    const s = { stick: el('path', { stroke: '#8A8FA0', 'stroke-width': 3.6, 'stroke-linecap': 'round' }, g),
      can: el('rect', { width: 11, height: 20, rx: 4, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 1.6 }, g),
      head: el('rect', { width: 28, height: 9, rx: 4, fill: '#3B2F4A', stroke: '#231C2C', 'stroke-width': 1.4 }, g) };
    return s; },
    tick: (s, P) => {
      const [rx] = bodyToRoot(P.p, ...P.R), [hx, hy] = rootToBody(P.p, rx + 40, -6);
      const [px, py] = P.R, mx = px + (hx - px) * 0.38, my = py + (hy - py) * 0.38, ang = Math.atan2(hy - py, hx - px) * 180 / Math.PI - 90;
      s.stick.setAttribute('d', `M${f1(px)} ${f1(py)} L${f1(hx)} ${f1(hy)}`);
      s.can.setAttribute('transform', `translate(${f1(mx)} ${f1(my)}) rotate(${f1(ang)}) translate(-5.5 -10)`);
      s.head.setAttribute('transform', `rotate(${f1(-P.p.rot)} ${f1(hx)} ${f1(hy)}) translate(${f1(hx - 8)} ${f1(hy - 4.5)})`);
      P.pub.vac = rootToStage(P.p, rx + 40 + 22, -4);
    } },
  'mop': { layer: 'hand', make: g => {
    const s = { stick: el('path', { stroke: '#B07A45', 'stroke-width': 3.4, 'stroke-linecap': 'round' }, g), head: el('g', {}, g) };
    el('rect', { x: -10, y: -3, width: 20, height: 5, rx: 2, fill: '#4BA3D9' }, s.head);
    [-9, -5, -1, 3, 7].forEach((x, i) => el('path', { d: `M${x} 2 L${x - 2 + i} 9`, stroke: '#9FC4E8', 'stroke-width': 3, 'stroke-linecap': 'round' }, s.head));
    return s; },
    tick: (s, P) => {
      const [rx] = bodyToRoot(P.p, ...P.R), [hx, hy] = rootToBody(P.p, rx + 30, -10);
      s.stick.setAttribute('d', `M${f1(P.R[0])} ${f1(P.R[1] - 4)} L${f1(hx)} ${f1(hy)}`);
      s.head.setAttribute('transform', `translate(${f1(hx)} ${f1(hy)}) rotate(${f1(-P.p.rot)})`);
      P.pub.mop = rootToStage(P.p, rx + 30, -2);
    } },

  /* new in a2-unterwegs-verben: stage props */
  'bike': { layer: 'back', make: g => {
    const wheels = [-40, 44].map(x => { const w = el('g', { transform: `translate(${x} -16)` }, g);
      el('circle', { r: 16, fill: 'none', stroke: '#3B2F4A', 'stroke-width': 3.2 }, w);
      const sp = el('g', {}, w); [0, 60, 120].forEach(a => el('line', { x1: -14, y1: 0, x2: 14, y2: 0, stroke: '#9AA4B1', 'stroke-width': 1.2, transform: `rotate(${a})` }, sp));
      return sp; });
    el('path', { d: 'M-40 -16 L0 -14 L-8 -44 Z M0 -14 L34 -48 M-8 -44 L34 -48 M34 -48 L44 -16', fill: 'none', stroke: '#E5484D', 'stroke-width': 3.4, 'stroke-linejoin': 'round', 'stroke-linecap': 'round' }, g);
    el('rect', { x: -17, y: -50, width: 17, height: 5, rx: 2.5, fill: '#3B2F4A' }, g);
    el('path', { d: 'M34 -48 L38 -60 L50 -60', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 3.2, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, g);
    el('circle', { cx: 0, cy: -14, r: 3.2, fill: '#3B2F4A' }, g);
    return { wheels }; },
    tick: (s, P) => s.wheels.forEach(w => w.setAttribute('transform', `rotate(${f1((P.c * 420) % 360)})`)) },
  'bus': { layer: 'back', make: g => {
    const b = el('g', {}, g);
    el('rect', { x: 0, y: -74, width: 84, height: 62, rx: 9, fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 2 }, b);
    [6, 28, 50].forEach(x => el('rect', { x, y: -66, width: 18, height: 18, rx: 3, fill: '#DDF1FB' }, b));
    el('rect', { x: 72, y: -66, width: 8, height: 44, rx: 2, fill: '#DDF1FB' }, b);
    el('rect', { x: 0, y: -40, width: 84, height: 5, fill: '#FFFFFF' }, b);
    [16, 66].forEach(x => el('circle', { cx: x, cy: -12, r: 8, fill: '#3B2F4A', stroke: '#9AA4B1', 'stroke-width': 2 }, b));
    const puffs = [0, 1, 2].map(() => el('circle', { r: 4, fill: '#C9CED6', opacity: 0 }, b));
    return { b, puffs }; },
    tick: (s, P) => {
      const off = (P.c * 16) % 70;
      s.b.setAttribute('transform', `translate(${f1(44 + off)} 0)`); s.b.setAttribute('opacity', (1 - clamp((off - 42) / 26)).toFixed(2));
      s.puffs.forEach((q, i) => { const k = (P.c * 1.2 + i / 3) % 1; q.setAttribute('cx', f1(-4 - k * 16)); q.setAttribute('cy', f1(-16 - k * 6)); q.setAttribute('r', f1(2.5 + k * 4)); q.setAttribute('opacity', (0.8 * (1 - k)).toFixed(2)); });
    } },
  'bus-stop': { layer: 'back', make: g => {
    el('line', { x1: 56, y1: 0, x2: 56, y2: -104, stroke: '#9AA4B1', 'stroke-width': 3.2, 'stroke-linecap': 'round' }, g);
    el('circle', { cx: 56, cy: -112, r: 13, fill: '#F2C14E', stroke: '#3E8E2F', 'stroke-width': 2.6 }, g);
    const t = el('text', { x: 56, y: -106.5, 'text-anchor': 'middle', 'font-size': 16, 'font-weight': 800, fill: '#3E8E2F', 'font-family': FONT }, g); t.textContent = 'H';
    el('rect', { x: 48, y: -88, width: 16, height: 22, rx: 2, fill: '#FFFFFF', stroke: '#9AA4B1', 'stroke-width': 1.4 }, g);
    [-83, -79, -75, -71].forEach(y => el('line', { x1: 51, y1: y, x2: 61, y2: y, stroke: '#C9CED6', 'stroke-width': 1.2 }, g));
    return null; }, tick: () => {} },
  'train-door': { layer: 'back', make: g => {
    el('rect', { x: 34, y: -128, width: 96, height: 122, rx: 9, fill: '#F3F4F6', stroke: '#B9BFCC', 'stroke-width': 2 }, g);
    el('rect', { x: 34, y: -42, width: 96, height: 6, fill: '#E5484D' }, g);
    el('rect', { x: 78, y: -106, width: 34, height: 26, rx: 4, fill: '#BFE3F2' }, g);
    el('rect', { x: 40, y: -112, width: 28, height: 102, rx: 3, fill: '#4A4F5C' }, g);
    el('rect', { x: 44, y: -108, width: 20, height: 40, rx: 2, fill: '#6B7180' }, g);
    el('line', { x1: 52, y1: -100, x2: 52, y2: -30, stroke: '#D6DCE6', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    el('rect', { x: 36, y: -12, width: 36, height: 4, rx: 1.5, fill: '#9AA4B1' }, g);
    return null; }, tick: () => {} },
  'bus-door': { layer: 'back', make: g => {
    el('rect', { x: 30, y: -120, width: 100, height: 108, rx: 10, fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 2 }, g);
    el('rect', { x: 36, y: -106, width: 30, height: 92, rx: 3, fill: '#3B4A5C' }, g);
    el('rect', { x: 74, y: -106, width: 34, height: 28, rx: 4, fill: '#DDF1FB' }, g);
    el('rect', { x: 30, y: -60, width: 100, height: 5, fill: '#FFFFFF' }, g);
    el('circle', { cx: 100, cy: -12, r: 9, fill: '#3B2F4A', stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    return null; }, tick: () => {} },
  'departure-board': { layer: 'back', make: g => {
    el('path', { d: 'M46 -138 L46 -150 M92 -138 L92 -150', stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    el('rect', { x: 34, y: -138, width: 72, height: 40, rx: 4, fill: '#1F3A6E' }, g);
    const txt = (x, y, t, fill, size) => { const e = el('text', { x, y, 'font-size': size || 8, 'font-weight': 700, fill, 'font-family': FONT }, g); e.textContent = t; return e; };
    txt(39, -125, '08:15', '#FFFFFF'); txt(64, -125, 'München', '#FFFFFF');
    const gl = txt(39, -113, 'Gleis 3', '#FFC93C');
    txt(39, -103, '08:32', '#8EA3C7', 7); txt(64, -103, 'Augsburg', '#8EA3C7', 7);
    return { gl }; },
    tick: (s, P) => s.gl.setAttribute('opacity', Math.sin(P.c * 5) > -0.3 ? 1 : 0.25) },
  'transfer-sign': { layer: 'back', make: g => {
    el('line', { x1: 70, y1: -96, x2: 70, y2: -150, stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    el('rect', { x: 50, y: -132, width: 40, height: 36, rx: 5, fill: '#2F5DA8' }, g);
    el('path', { d: 'M58 -120 L80 -120 M74 -126 L80 -120 L74 -114 M82 -108 L60 -108 M66 -114 L60 -108 L66 -102', fill: 'none', stroke: '#FFFFFF', 'stroke-width': 2.6, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, g);
    el('rect', { x: 54, y: -92, width: 32, height: 13, rx: 2, fill: '#FFFFFF' }, g);
    const t = el('text', { x: 70, y: -82, 'text-anchor': 'middle', 'font-size': 9, 'font-weight': 800, fill: '#E5484D', 'font-family': FONT }, g); t.textContent = 'ICE';
    return null; }, tick: () => {} },
  'station-sign': { layer: 'back', make: g => {
    el('path', { d: 'M44 -132 L44 -146 M96 -132 L96 -146', stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    el('rect', { x: 32, y: -132, width: 76, height: 22, rx: 3, fill: '#2F5DA8' }, g);
    const t = el('text', { x: 70, y: -116.5, 'text-anchor': 'middle', 'font-size': 12, 'font-weight': 800, fill: '#FFFFFF', 'font-family': FONT }, g); t.textContent = 'Hamburg';
    return null; }, tick: () => {} },

  /* new in a2-unterwegs-verben: held props */
  'suitcase': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-5 2 L-5 -3 L5 -3 L5 2', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 2.2, 'stroke-linejoin': 'round' }, s);
    el('rect', { x: -12, y: 2, width: 24, height: 19, rx: 3.5, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 1.6 }, s);
    el('line', { x1: -12, y1: 11, x2: 12, y2: 11, stroke: '#F2C14E', 'stroke-width': 2.2 }, s);
    return s; },
    tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.L[0])} ${f1(P.L[1] + 2)})`) },
  'map': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M-22 -14 L-8 -11 L6 -14 L22 -11 L22 13 L6 10 L-8 13 L-22 10 Z', fill: '#FFF6DA', stroke: '#C9B98A', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-8 -11 L-8 13 M6 -14 L6 10', stroke: '#E4D6A8', 'stroke-width': 1.2 }, s);
    el('path', { d: 'M-22 2 Q-10 -6 0 2 T22 -2', fill: 'none', stroke: '#6FB8E6', 'stroke-width': 2.4 }, s);
    el('path', { d: 'M-16 8 L-6 0 L4 4 L12 -6', fill: 'none', stroke: '#E5484D', 'stroke-width': 1.6, 'stroke-dasharray': '2.5 2' }, s);
    el('path', { d: 'M10 -9 L15 -4 M15 -9 L10 -4', stroke: '#E5484D', 'stroke-width': 1.8, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y - 2)}) rotate(-5)`); } },
  'welcome-sign': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('rect', { x: -22, y: -14, width: 44, height: 28, rx: 3, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.6 }, s);
    el('path', { d: 'M0 -2 C-3 -7 -9 -5 -7 0 L0 6 L7 0 C9 -5 3 -7 0 -2 Z', fill: '#F06C8B', transform: 'translate(0 -3)' }, s);
    const t = el('text', { x: 0, y: 11, 'text-anchor': 'middle', 'font-size': 7.5, 'font-weight': 800, fill: '#C0567A', 'font-family': FONT }, s); t.textContent = 'Hallo!';
    return s; },
    tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y + 2)})`); } },
  'umbrella': { layer: 'hand', make: g => {
    const s = el('g', {}, g);
    el('path', { d: 'M0 4 Q0 9 -5 9', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, s);
    el('line', { x1: 0, y1: 4, x2: -16, y2: -80, stroke: '#3B2F4A', 'stroke-width': 2.2, 'stroke-linecap': 'round' }, s);
    el('path', { d: 'M-62 -78 C-58 -116 26 -116 30 -78 Q22 -84 14 -78 Q7 -84 -1 -78 Q-8 -84 -16 -78 Q-24 -84 -31 -78 Q-39 -84 -46 -78 Q-54 -84 -62 -78 Z', fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-16 -106 L-16 -112', stroke: '#3B2F4A', 'stroke-width': 2.2, 'stroke-linecap': 'round' }, s);
    return s; },
    tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0])} ${f1(P.R[1])})`) },

  /* new in a2-einkaufen-verben: stage props */
  'clothes-rack': { layer: 'back', make: g => {
    el('path', { d: 'M32 0 L32 -98 M104 0 L104 -98 M28 -98 L108 -98', fill: 'none', stroke: '#9AA4B1', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    [[46, '#E5484D', '#B8323A'], [68, '#4BA3D9', '#2F7DB8'], [90, '#F2C14E', '#C99227']].forEach(([x, f, s2]) => {
      el('path', { d: `M${x} -98 L${x} -94`, stroke: '#6B7180', 'stroke-width': 1.6 }, g);
      el('path', { d: `M${x - 11} -86 L${x - 4} -92 Q${x} -89 ${x + 4} -92 L${x + 11} -86 L${x + 8} -81 L${x + 6} -82 L${x + 6} -62 L${x - 6} -62 L${x - 6} -82 L${x - 8} -81 Z`, fill: f, stroke: s2, 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, g);
    });
    return null; }, tick: () => {} },
  'mirror': { layer: 'back', make: g => {
    el('path', { d: 'M56 0 L64 -38 M80 0 L72 -38', stroke: '#A86B48', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    el('ellipse', { cx: 68, cy: -76, rx: 21, ry: 33, fill: '#DDF1FB', stroke: '#C99A6B', 'stroke-width': 4 }, g);
    el('path', { d: 'M56 -92 L64 -100 M57 -82 L70 -95', stroke: '#FFFFFF', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, g);
    return null; }, tick: () => {} },
  'shoe-shelf': { layer: 'back', make: g => {
    el('rect', { x: 40, y: -108, width: 64, height: 108, rx: 3, fill: '#F3E3CF', stroke: '#C99A6B', 'stroke-width': 2 }, g);
    [-74, -40].forEach(y => el('rect', { x: 40, y, width: 64, height: 4, fill: '#C99A6B' }, g));
    const shoe = (x, y, f, s2) => el('path', { d: `M${x} ${y} L${x} ${y - 9} Q${x + 4} ${y - 11} ${x + 8} ${y - 8} L${x + 18} ${y - 4} Q${x + 22} ${y - 3} ${x + 22} ${y} Z`, fill: f, stroke: s2, 'stroke-width': 1.3, 'stroke-linejoin': 'round' }, g);
    shoe(46, -74, '#4BA3D9', '#2F7DB8'); shoe(70, -74, '#4BA3D9', '#2F7DB8');
    el('rect', { x: 46, y: -76, width: 46, height: 2, fill: '#FFFFFF', opacity: 0.8 }, g);
    shoe(48, -40, '#B9BFCC', '#8A8FA0'); shoe(72, -40, '#B9BFCC', '#8A8FA0');
    el('path', { d: 'M94 -94 L102 -94 L102 -84 L94 -84 Z', fill: '#F06C8B' }, g);
    return null; }, tick: () => {} },
  'card-terminal': { layer: 'back', make: g => {
    el('rect', { x: 60, y: -24, width: 52, height: 24, rx: 2, fill: '#E4DFD6' }, g);
    el('rect', { x: 74, y: -78, width: 26, height: 38, rx: 4, fill: '#3B2F4A', stroke: '#231C2C', 'stroke-width': 1.6 }, g);
    el('rect', { x: 78, y: -74, width: 18, height: 13, rx: 2, fill: '#BFF0D4' }, g);
    for (let r = 0; r < 3; r++) for (let k = 0; k < 3; k++) el('rect', { x: 79 + k * 6, y: -57 + r * 5, width: 4, height: 3, rx: 1, fill: '#6B7180' }, g);
    el('path', { d: 'M82 -40 L92 -40 L94 -24 L80 -24 Z', fill: '#3B2F4A' }, g);
    return null; }, tick: () => {} },
  'counter': { layer: 'back', make: g => {
    el('rect', { x: 40, y: -50, width: 74, height: 50, fill: '#D9B48A' }, g);
    el('rect', { x: 36, y: -54, width: 82, height: 7, rx: 2, fill: '#A86B48' }, g);
    el('rect', { x: 48, y: -40, width: 58, height: 32, rx: 2, fill: 'none', stroke: '#C49A6E', 'stroke-width': 1.6 }, g);
    return null; }, tick: () => {} },
  'piggy-bank': { layer: 'back', make: g => {
    el('rect', { x: 52, y: -26, width: 34, height: 6, rx: 2, fill: '#A86B48' }, g);
    el('path', { d: 'M56 -20 L56 0 M82 -20 L82 0', stroke: '#A86B48', 'stroke-width': 3 }, g);
    el('ellipse', { cx: 69, cy: -40, rx: 19, ry: 14, fill: '#F7A8BE', stroke: '#D9708E', 'stroke-width': 1.6 }, g);
    el('ellipse', { cx: 87, cy: -40, rx: 5, ry: 6, fill: '#F7A8BE', stroke: '#D9708E', 'stroke-width': 1.4 }, g);
    el('circle', { cx: 86, cy: -41, r: 0.9, fill: '#B0506C' }, g); el('circle', { cx: 88.5, cy: -39, r: 0.9, fill: '#B0506C' }, g);
    el('path', { d: 'M74 -53 L78 -60 L80 -51 Z', fill: '#F7A8BE', stroke: '#D9708E', 'stroke-width': 1.2, 'stroke-linejoin': 'round' }, g);
    el('circle', { cx: 78, cy: -45, r: 1.6, fill: '#2B2230' }, g);
    el('rect', { x: 62, y: -55, width: 10, height: 2.4, rx: 1.2, fill: '#B0506C' }, g);
    [60, 76].forEach(x => el('rect', { x, y: -29, width: 5, height: 4, rx: 1, fill: '#F7A8BE', stroke: '#D9708E', 'stroke-width': 1 }, g));
    el('path', { d: 'M50 -42 q-5 -2 -3 -6', fill: 'none', stroke: '#D9708E', 'stroke-width': 1.4, 'stroke-linecap': 'round' }, g);
    return null; }, tick: () => {} },

  /* new in a2-einkaufen-verben: worn and held props */
  'jacket': { layer: 'wear', make: g => {
    el('path', { d: 'M-31 -64 Q-36 -36 -26 -12 L-5 -10 L-3 -64 Q-18 -70 -31 -64 Z', fill: '#4A6FA5', stroke: '#33507A', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M31 -64 Q36 -36 26 -12 L5 -10 L3 -64 Q18 -70 31 -64 Z', fill: '#4A6FA5', stroke: '#33507A', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M-3 -64 L-12 -50 L-16 -64 Z M3 -64 L12 -50 L16 -64 Z', fill: '#6F8FC2' }, g);
    [-40, -28].forEach(y => el('circle', { cx: -8, cy: y, r: 1.4, fill: '#F2C14E' }, g));
    return null; }, tick: () => {} },
  'scarf': { layer: 'wear', make: g => {
    el('path', { d: 'M-27 -72 Q0 -60 27 -72 L28 -63 Q0 -51 -28 -63 Z', fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M10 -60 L19 -62 L23 -36 L14 -35 Z', fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M15 -35 l0 4 M18 -35.5 l0 4 M21 -36 l0 4', stroke: '#B8323A', 'stroke-width': 1.4, 'stroke-linecap': 'round' }, g);
    return null; }, tick: () => {} },
  'shoe-small': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('path', { d: 'M-6 6 L-6 -2 Q-3 -4 0 -2 L6 1 Q9 2 9 6 Z', fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 1.2, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-2 -1 L1 1 M0 -2 L3 0', stroke: '#FFFFFF', 'stroke-width': 1 }, s);
    return s; }, tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0] + 6)} ${f1(P.R[1] - 12)}) scale(1.6)`) },
  'card': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -9, y: -6, width: 18, height: 12, rx: 2, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 1.4 }, s);
    el('rect', { x: -6, y: -3, width: 5, height: 4, rx: 1, fill: '#F2C14E' }, s);
    return s; }, tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0] + 6)} ${f1(P.R[1] - 6)}) rotate(-12)`); P.pub.card = bodyToStage(P.p, P.R[0] + 14, P.R[1] - 8); } },
  'gift': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -16, y: -12, width: 32, height: 22, rx: 2, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.4 }, s);
    el('path', { d: 'M0 -12 L0 10 M-16 -2 L16 -2', stroke: '#E5484D', 'stroke-width': 3 }, s);
    el('path', { d: 'M0 -12 C-8 -22 -12 -12 0 -12 C12 -12 8 -22 0 -12 Z', fill: '#E5484D' }, s);
    return s; }, tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y + 2)})`); } },
  'phone': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -8, y: -14, width: 16, height: 26, rx: 3, fill: '#3B2F4A', stroke: '#231C2C', 'stroke-width': 1.2 }, s);
    el('rect', { x: -6, y: -11, width: 12, height: 19, rx: 1.5, fill: '#DDF1FB' }, s);
    el('path', { d: 'M-3 -6 L3 -6 M-3 -2 L3 -2 M-3 2 L1 2', stroke: '#9AA4B1', 'stroke-width': 1.2, 'stroke-linecap': 'round' }, s);
    return s; }, tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = Math.min(P.L[1], P.R[1]) - 6; s.setAttribute('transform', `translate(${f1(x)} ${f1(y)})`); P.pub.phone = bodyToStage(P.p, x, y - 16); } },
  'sweater-small': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('path', { d: 'M-12 -8 L-5 -11 Q0 -8 5 -11 L12 -8 L15 0 L10 1 L9 -3 L9 10 L-9 10 L-9 -3 L-10 1 L-15 0 Z', fill: '#5E9E45', stroke: '#3E7A2F', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-9 2 L9 2 M-9 6 L9 6', stroke: '#8CC56E', 'stroke-width': 1.4 }, s);
    return s; }, tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y + 4)})`); } },
  'toaster': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -14, y: -10, width: 28, height: 20, rx: 6, fill: '#D6DCE6', stroke: '#8A8FA0', 'stroke-width': 1.4 }, s);
    el('path', { d: 'M-8 -10 L-8 -6 M0 -10 L0 -6', stroke: '#6B7180', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, s);
    el('rect', { x: 10, y: -2, width: 6, height: 3, rx: 1, fill: '#3B2F4A' }, s);
    el('path', { d: 'M-4 -16 q-3 -3 0 -6 q3 -3 0 -6', fill: 'none', stroke: '#8A8FA0', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, s);
    return s; }, tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y - 4)})`); } },
  'wallet': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('path', { d: 'M-12 -2 L12 -2 L12 12 L-12 12 Z', fill: '#A86B48', stroke: '#7A4A2E', 'stroke-width': 1.4 }, s);
    el('path', { d: 'M-12 -2 L-8 -12 L16 -12 L12 -2 Z', fill: '#C08559', stroke: '#7A4A2E', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-8 4 L8 4', stroke: '#7A4A2E', 'stroke-width': 1.2 }, s);
    return s; }, tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0] + 6)} ${f1(P.R[1] - 6)}) scale(1.3)`); P.pub.wallet = bodyToStage(P.p, P.R[0] + 6, P.R[1] - 2); } },
  'coin': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('circle', { r: 5, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.3 }, s);
    txt(s, 0, 2.6, '€', 7, '#B07A10');
    return s; }, tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0] + 2)} ${f1(P.R[1] + 7)})`) },

  /* new in a2-termine-verben */
  'phone-ear': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -5, y: -13, width: 10, height: 24, rx: 3, fill: '#3B2F4A', stroke: '#231C2C', 'stroke-width': 1.2 }, s);
    return s; }, tick: (s, P) => { s.setAttribute('transform', `translate(${f1(P.R[0] + 2)} ${f1(P.R[1] - 6)}) rotate(-18)`); P.pub.ear = bodyToStage(P.p, P.R[0] + 6, P.R[1] - 16); } },
  'calendar': { layer: 'back', make: g => calendarSheet(g, 'circle'), tick: (s, P) => calendarTick(s, P) },
  'calendar-move': { layer: 'back', make: g => calendarSheet(g, 'move'), tick: (s, P) => calendarTick(s, P) },
  'calendar-x': { layer: 'back', make: g => calendarSheet(g, 'x'), tick: (s, P) => calendarTick(s, P) },
  'thermometer': { layer: 'wear', make: g => {
    el('line', { x1: 2, y1: -72, x2: 24, y2: -79, stroke: '#9AA4B1', 'stroke-width': 4.4, 'stroke-linecap': 'round' }, g);
    el('line', { x1: 2, y1: -72, x2: 24, y2: -79, stroke: '#FFFFFF', 'stroke-width': 2.8, 'stroke-linecap': 'round' }, g);
    el('line', { x1: 3, y1: -72.3, x2: 15, y2: -76.2, stroke: '#E5484D', 'stroke-width': 1.4, 'stroke-linecap': 'round' }, g);
    txt(g, 30, -84, '38°', 8, '#E5484D');
    return null; }, tick: () => {} },
  'big-clock': { layer: 'back', make: g => {
    el('circle', { cx: 74, cy: -108, r: 21, fill: '#FFFFFF', stroke: '#3B2F4A', 'stroke-width': 3 }, g);
    [0, 90, 180, 270].forEach(a => el('line', { x1: 74, y1: -126, x2: 74, y2: -122, stroke: '#3B2F4A', 'stroke-width': 2, transform: `rotate(${a} 74 -108)` }, g));
    const hm = el('line', { x1: 74, y1: -108, x2: 74, y2: -124, stroke: '#3B2F4A', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, g);
    const hh = el('line', { x1: 74, y1: -108, x2: 74, y2: -118, stroke: '#E5484D', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    return { hm, hh }; },
    tick: (s, P) => { s.hm.setAttribute('transform', `rotate(${f1((P.c * 240) % 360)} 74 -108)`); s.hh.setAttribute('transform', `rotate(${f1((P.c * 20) % 360)} 74 -108)`); } },
  'door': { layer: 'back', make: g => {
    el('rect', { x: 50, y: -128, width: 54, height: 128, rx: 3, fill: '#B07A4E', stroke: '#7A4A2E', 'stroke-width': 2 }, g);
    el('rect', { x: 58, y: -118, width: 38, height: 44, rx: 2, fill: 'none', stroke: '#8C5B3A', 'stroke-width': 1.8 }, g);
    el('rect', { x: 58, y: -64, width: 38, height: 52, rx: 2, fill: 'none', stroke: '#8C5B3A', 'stroke-width': 1.8 }, g);
    el('circle', { cx: 60, cy: -66, r: 3, fill: '#F2C14E' }, g);
    el('rect', { x: 64, y: -108, width: 26, height: 11, rx: 2, fill: '#FFFFFF' }, g);
    txt(g, 77, -100, 'Praxis', 6.5, '#3B2F4A');
    return null; }, tick: () => {} },
  'number-display': { layer: 'back', make: g => {
    el('path', { d: 'M70 -132 L70 -142 M96 -132 L96 -142', stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    el('rect', { x: 58, y: -132, width: 50, height: 20, rx: 3, fill: '#2B2230' }, g);
    const t = txt(g, 83, -117.5, 'B 042', 11, '#FF6B5A');
    return { t }; }, tick: (s, P) => s.t.setAttribute('opacity', Math.sin(P.c * 4) > -0.4 ? 1 : 0.35) },
  'ticket': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -9, y: -14, width: 18, height: 12, rx: 1.5, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.2 }, s);
    txt(s, 0, -5.5, 'B 042', 5.5, '#2B2230');
    return s; }, tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0] + 2)} ${f1(P.R[1])}) rotate(-8)`) },
  'clipboard': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -13, y: -17, width: 26, height: 33, rx: 2.5, fill: '#C99A6B', stroke: '#A86B48', 'stroke-width': 1.4 }, s);
    el('rect', { x: -10, y: -13, width: 20, height: 26, rx: 1, fill: '#FFFFFF' }, s);
    el('rect', { x: -5, y: -19, width: 10, height: 5, rx: 1.5, fill: '#9AA4B1' }, s);
    [-9, -5, -1].forEach(y => el('path', { d: `M-7 ${y} L7 ${y}`, stroke: '#C9D3DE', 'stroke-width': 1.4 }, s));
    el('path', { d: 'M-7 9 L7 9', stroke: '#9AA4B1', 'stroke-width': 1 }, s);
    return s; }, tick: (s, P) => { const x = P.L[0] + 12, y = P.L[1] - 4; s.setAttribute('transform', `translate(${f1(x)} ${f1(y)})`); P.pub.clip = bodyToStage(P.p, x, y); } },
  'pen': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('line', { x1: 0, y1: 0, x2: -8, y2: 9, stroke: '#2F5DA8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
    el('line', { x1: -8, y1: 9, x2: -9.5, y2: 11, stroke: '#2B2230', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, s);
    return s; }, tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0])} ${f1(P.R[1] - 2)})`) },
  'folder': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -14, y: -14, width: 26, height: 20, rx: 1, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1, transform: 'rotate(-6)' }, s);
    el('rect', { x: -16, y: -10, width: 32, height: 22, rx: 2, fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 1.4 }, s);
    el('rect', { x: -6, y: -2, width: 12, height: 5, rx: 1, fill: '#FFFFFF' }, s);
    return s; }, tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y)})`); } },
  /* new in the noun decks (cards drawn by the cat) */
  'fridge': { layer: 'back', make: g => {
    el('rect', { x: 44, y: -122, width: 58, height: 122, rx: 8, fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 2.2 }, g);
    el('rect', { x: 48, y: -78, width: 50, height: 74, rx: 3, fill: '#FFF4C2' }, g);
    [-56, -32].forEach(y => el('line', { x1: 49, y1: y, x2: 97, y2: y, stroke: '#E9D58A', 'stroke-width': 2 }, g));
    el('rect', { x: 54, y: -74, width: 10, height: 18, rx: 1.5, fill: '#FFFFFF', stroke: '#C9CED6' }, g); el('rect', { x: 54, y: -68, width: 10, height: 4, fill: '#4BA3D9' }, g);
    el('circle', { cx: 86, cy: -61, r: 5, fill: '#E5484D' }, g); el('path', { d: 'M70 -32 L86 -32 L86 -42 Z', fill: '#F2C14E' }, g);
    el('rect', { x: 44, y: -122, width: 58, height: 42, rx: 8, fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 2.2 }, g);
    el('path', { d: 'M102 -80 L118 -86 L118 -4 L102 -2 Z', fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 2.2, 'stroke-linejoin': 'round' }, g);
    return null; }, tick: () => {} },
  'sofa-back': { layer: 'back', make: g => {
    el('rect', { x: -58, y: -66, width: 170, height: 40, rx: 14, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 2 }, g);
    el('rect', { x: -62, y: -36, width: 178, height: 28, rx: 10, fill: '#B3A6F5', stroke: '#7C6BD6', 'stroke-width': 2 }, g);
    el('rect', { x: 96, y: -50, width: 22, height: 44, rx: 10, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 2 }, g);
    [[-50, -8], [104, -8]].forEach(([x, y]) => el('rect', { x, y, width: 5, height: 8, fill: '#7A4A2E' }, g));
    return null; }, tick: () => {} },
  'pillow': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('rect', { x: -14, y: -14, width: 28, height: 28, rx: 8, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.6 }, s); el('circle', { r: 2.4, fill: '#C99227' }, s); return s; },
    tick: (s, P) => { const x = (P.L[0] + P.R[0]) / 2, y = (P.L[1] + P.R[1]) / 2; s.setAttribute('transform', `translate(${f1(x)} ${f1(y)}) rotate(-8)`); } },
  'red-nose': { layer: 'wear', make: g => { el('ellipse', { cx: 0, cy: -77.5, rx: 4.6, ry: 3.6, fill: '#E5484D' }, g); el('circle', { cx: 1.4, cy: -78.8, r: 1.1, fill: '#FFB3B3' }, g); return null; }, tick: () => {} },
  'tissue': { layer: 'hand', make: g => { const s = el('g', {}, g);
    el('path', { d: 'M-9 -8 Q0 -12 9 -8 L8 8 Q0 11 -8 8 Z', fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.2, 'stroke-linejoin': 'round' }, s);
    el('path', { d: 'M-4 -6 Q-2 0 -5 6 M3 -7 Q5 0 2 7', fill: 'none', stroke: '#E4E7EE', 'stroke-width': 1 }, s);
    return s; }, tick: (s, P) => s.setAttribute('transform', `translate(${f1(P.R[0] - 2)} ${f1(P.R[1] - 4)}) rotate(-10)`) },
  'plaster-cheek': { layer: 'wear', make: g => {
    el('rect', { x: 12, y: -84, width: 15, height: 7, rx: 3, fill: '#F2D2A6', stroke: '#D9AE7E', 'stroke-width': 1, transform: 'rotate(-20 19 -80)' }, g);
    el('rect', { x: 16.5, y: -83, width: 6, height: 5, rx: 1, fill: '#E9BF8E', transform: 'rotate(-20 19 -80)' }, g);
    return null; }, tick: () => {} },
};

/* ---------- FX: { make(group, card) -> state, tick(state, clock, weight, anchors) } ----------
   anchors: x, y (head top), my (mouth), by (body centre), floor, plus points published by props (tool, vac, mop, iron, bag, spout, drum, plate) */
const FX = {
  'sparkles': {
    make: g => Array.from({ length: 4 }, () => el('path', { d: STAR, fill: '#FFC93C', opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.plate || h.tool || [h.x + 40, h.by - 10];
      s.forEach((e, i) => {
        const pos = [[-16, -16], [16, -10], [-12, 14], [18, 16]][i], tw = Math.abs(Math.sin(c * 3 + i * 1.3));
        e.setAttribute('transform', `translate(${f1(o[0] + pos[0])} ${f1(o[1] + pos[1])}) scale(${(0.35 + tw * 0.65).toFixed(2)})`);
        e.setAttribute('opacity', (w * tw).toFixed(2));
      });
    },
  },
  'bubbles': {
    make: g => Array.from({ length: 6 }, (_, i) => el('circle', { r: 2.5 + (i % 3), fill: '#FFFFFF', stroke: '#9FC4E8', 'stroke-width': 1, opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.tool || h.drum || [h.x + 30, h.by];
      s.forEach((b, i) => {
        const k = (c * 0.45 + i / s.length) % 1;
        b.setAttribute('cx', f1(o[0] + Math.sin(k * 6 + i) * 6 + (i % 2 ? 6 : -6)));
        b.setAttribute('cy', f1(o[1] - k * 34));
        b.setAttribute('opacity', (w * Math.sin(Math.PI * k) * 0.95).toFixed(2));
      });
    },
  },
  'dust': {
    make: g => Array.from({ length: 5 }, () => el('circle', { r: 2.2, fill: '#E9A766', opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.vac || [h.x + 50, h.floor - 4];
      s.forEach((d, i) => {
        const k = (c * 0.8 + i / s.length) % 1;
        d.setAttribute('cx', f1(o[0] + 2 + (1 - k) * 18)); d.setAttribute('cy', f1(o[1] - 2 - Math.sin(k * 9 + i) * 4 * (1 - k)));
        d.setAttribute('r', (3.2 * (1 - k * 0.6)).toFixed(2)); d.setAttribute('opacity', (w * (k < 0.85 ? 1 : (1 - k) / 0.15)).toFixed(2));
      });
    },
  },
  'shine': { layer: 'back',
    make: g => ({ pool: el('ellipse', { rx: 26, ry: 3.4, fill: '#9FD3EE', opacity: 0 }, g), stars: [0, 1, 2].map(() => el('path', { d: STAR, fill: '#FFFFFF', opacity: 0 }, g)) }),
    tick: (s, c, w, h) => {
      const o = h.mop || [h.x + 40, h.floor];
      s.pool.setAttribute('cx', f1(o[0])); s.pool.setAttribute('cy', f1(h.floor + 1)); s.pool.setAttribute('opacity', (w * 0.7).toFixed(2));
      s.stars.forEach((e, i) => { const tw = Math.abs(Math.sin(c * 2.4 + i * 2));
        e.setAttribute('transform', `translate(${f1(o[0] - 16 + i * 16)} ${f1(h.floor - 2)}) scale(${(0.3 + tw * 0.4).toFixed(2)})`); e.setAttribute('opacity', (w * tw).toFixed(2)); });
    },
  },
  'steam': {
    make: g => [0, 1, 2].map(() => el('path', { d: 'M0 0 q-3 -4 0 -8 q3 -4 0 -8', fill: 'none', stroke: '#C9D3DE', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.iron || [h.x + 40, h.by];
      s.forEach((e, i) => { const k = (c * 0.6 + i / 3) % 1;
        e.setAttribute('transform', `translate(${f1(o[0] - 6 + i * 6)} ${f1(o[1] - k * 14)})`); e.setAttribute('opacity', (w * Math.sin(Math.PI * k)).toFixed(2)); });
    },
  },
  'stink': {
    make: g => [0, 1, 2].map(() => el('path', { d: 'M0 0 q-3 -4 0 -8 q3 -4 0 -8', fill: 'none', stroke: '#7DB35A', 'stroke-width': 2.2, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.bag || [h.x + 40, h.by];
      s.forEach((e, i) => { const k = (c * 0.5 + i / 3) % 1;
        e.setAttribute('transform', `translate(${f1(o[0] - 10 + i * 10)} ${f1(o[1] + 6 - k * 18)})`); e.setAttribute('opacity', (w * Math.sin(Math.PI * k)).toFixed(2)); });
    },
  },
  'drops': {
    make: g => Array.from({ length: 4 }, () => el('path', { d: DROP, fill: '#6FB8E6', opacity: 0 }, g)),
    tick: (s, c, w, h) => {
      const o = h.spout || [h.x + 50, h.by], on = h.pour == null ? 1 : h.pour;
      s.forEach((d, i) => { const k = (c * 1.4 + i / s.length) % 1;
        d.setAttribute('transform', `translate(${f1(o[0] + 2 + k * 3)} ${f1(o[1] + 3 + k * 22)}) scale(0.7)`); d.setAttribute('opacity', (w * on * (1 - k * 0.6)).toFixed(2)); });
    },
  },
  'sun': { layer: 'back',
    make: g => { const s = el('g', { opacity: 0 }, g); el('circle', { r: 9, fill: '#FFC93C' }, s);
      const rays = el('g', {}, s); for (let i = 0; i < 8; i++) el('line', { x1: 0, y1: -12.5, x2: 0, y2: -17, stroke: '#FFC93C', 'stroke-width': 2.4, 'stroke-linecap': 'round', transform: `rotate(${i * 45})` }, rays);
      return { s, rays }; },
    tick: (st, c, w, h) => { st.s.setAttribute('transform', `translate(${f1(h.stageX - 56)} ${f1(h.stageY - 118)})`); st.s.setAttribute('opacity', w.toFixed(2)); st.rays.setAttribute('transform', `rotate(${f1(c * 20)})`); },
  },
  /* new in a2-unterwegs-verben */
  'speed-lines': {
    make: g => [0, 1, 2].map(() => el('path', { stroke: '#FFFFFF', 'stroke-width': 3, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => s.forEach((l, i) => { const k = (c * 2 + i / 3) % 1, x = h.x - 46 - k * 22, y = h.by - 34 + i * 20;
      l.setAttribute('d', `M${f1(x)} ${f1(y)} l-16 0`); l.setAttribute('opacity', (w * (1 - k) * 0.95).toFixed(2)); }),
  },
  'sweat': {
    make: g => [0, 1].map(() => el('path', { d: DROP, fill: '#6FB8E6', opacity: 0 }, g)),
    tick: (s, c, w, h) => s.forEach((d, i) => { const k = (c * 0.9 + i * 0.5) % 1, side = i ? 1 : -1;
      d.setAttribute('transform', `translate(${f1(h.x + side * (34 + k * 8))} ${f1(h.y + 26 + k * 10)}) rotate(${side * 30})`); d.setAttribute('opacity', (w * (1 - k)).toFixed(2)); }),
  },
  'sigh-puffs': {
    make: g => [0, 1, 2].map(() => el('circle', { r: 4, fill: '#FFFFFF', opacity: 0 }, g)),
    tick: (s, c, w, h) => { const k = (c * 0.45) % 1; s.forEach((q, i) => {
      q.setAttribute('cx', f1(h.x + 10 + k * 26 + i * 7)); q.setAttribute('cy', f1(h.my - k * 10 - i * 3)); q.setAttribute('r', f1(2.5 + i * 1.3 + k * 2));
      q.setAttribute('opacity', (w * Math.sin(Math.PI * k) * 0.9).toFixed(2)); }); },
  },
  'question': {
    make: g => [0, 1].map(() => { const t = el('text', { 'text-anchor': 'middle', 'font-size': 20, 'font-weight': 800, fill: '#7A4FD6', 'font-family': FONT, opacity: 0 }, g); t.textContent = '?'; return t; }),
    tick: (s, c, w, h) => s.forEach((t, i) => { const k = (c * 0.7 + i * 0.5) % 1, sc = k < 0.25 ? backOut(k / 0.25) : 1;
      t.setAttribute('transform', `translate(${f1(h.x + (i ? -32 : 34))} ${f1(h.y + (i ? 6 : -2))}) scale(${(Math.max(0.01, sc) * (i ? 0.8 : 1)).toFixed(2)})`);
      t.setAttribute('opacity', (w * (k < 0.85 ? 1 : (1 - k) / 0.15)).toFixed(2)); }),
  },
  'hearts': {
    make: g => [0, 1, 2].map(() => el('path', { d: 'M0 -2 C-3 -7 -9 -5 -7 0 L0 6 L7 0 C9 -5 3 -7 0 -2 Z', fill: '#F06C8B', opacity: 0 }, g)),
    tick: (s, c, w, h) => s.forEach((e, i) => { const k = (c * 0.5 + i / 3) % 1;
      e.setAttribute('transform', `translate(${f1(h.x + [-34, 36, 26][i] + Math.sin(k * 7) * 3)} ${f1(h.y + 40 - k * 34)}) scale(${(0.6 + k * 0.5).toFixed(2)})`);
      e.setAttribute('opacity', (w * Math.sin(Math.PI * k)).toFixed(2)); }),
  },
  'rain': { layer: 'back',
    make: g => Array.from({ length: 12 }, () => el('line', { stroke: '#7FA6C4', 'stroke-width': 1.8, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => s.forEach((l, i) => { const k = (c * 1.1 + i * 0.37) % 1, x = h.x - 70 + (i * 53) % 190, y = h.y - 40 + k * 190;
      l.setAttribute('x1', f1(x)); l.setAttribute('y1', f1(y)); l.setAttribute('x2', f1(x - 3)); l.setAttribute('y2', f1(y + 9)); l.setAttribute('opacity', (w * 0.8).toFixed(2)); }),
  },
  /* new in a2-einkaufen-verben and a2-termine-verben */
  'contactless': {
    make: g => [6, 11, 16].map(r => el('path', { d: `M0 ${-r} A${r} ${r} 0 0 1 0 ${r}`, fill: 'none', stroke: '#7C6BD6', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => { const o = h.card || [h.x + 50, h.by]; s.forEach((a, i) => { const k = (c * 1.2 - i * 0.2) % 1;
      a.setAttribute('transform', `translate(${f1(o[0])} ${f1(o[1])})`); a.setAttribute('opacity', (w * Math.max(0, Math.sin(Math.PI * ((k + 1) % 1)))).toFixed(2)); }); },
  },
  'parcel': {
    make: g => { const s = el('g', { opacity: 0 }, g); el('rect', { x: -7, y: -6, width: 14, height: 12, rx: 1.5, fill: '#D9A86C', stroke: '#B0803F', 'stroke-width': 1.2 }, s);
      el('path', { d: 'M-7 -1 L7 -1 M0 -6 L0 6', stroke: '#B0803F', 'stroke-width': 1.2 }, s); return s; },
    tick: (s, c, w, h) => { const o = h.phone || [h.x, h.by - 30], k = (c * 0.5) % 1;
      s.setAttribute('transform', `translate(${f1(o[0] + 22 + k * 40)} ${f1(o[1] - 6 - Math.sin(Math.PI * k) * 24 - k * 8)}) scale(${(0.6 + k * 0.5).toFixed(2)})`);
      s.setAttribute('opacity', (w * (k < 0.8 ? 1 : (1 - k) / 0.2)).toFixed(2)); },
  },
  'swap': {
    make: g => { const s = el('g', { opacity: 0 }, g);
      el('path', { d: 'M-10 -4 A11 11 0 0 1 9 -6', fill: 'none', stroke: '#1F8FA8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
      el('path', { d: 'M9 -6 l-1 -5 M9 -6 l-5 1', stroke: '#1F8FA8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
      el('path', { d: 'M10 4 A11 11 0 0 1 -9 6', fill: 'none', stroke: '#1F8FA8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
      el('path', { d: 'M-9 6 l1 5 M-9 6 l5 -1', stroke: '#1F8FA8', 'stroke-width': 2.6, 'stroke-linecap': 'round' }, s);
      return s; },
    tick: (s, c, w, h) => { s.setAttribute('transform', `translate(${f1(h.x + 40)} ${f1(h.y + 16)}) rotate(${f1((c * 90) % 360)})`); s.setAttribute('opacity', w.toFixed(2)); },
  },
  'coins': {
    make: g => Array.from({ length: 4 }, () => el('circle', { r: 3.4, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1, opacity: 0 }, g)),
    tick: (s, c, w, h) => { const o = h.wallet || [h.x + 34, h.by - 20]; s.forEach((d, i) => { const k = (c * 0.9 + i / s.length) % 1;
      d.setAttribute('cx', f1(o[0] + (i - 1.5) * 5 + k * 6)); d.setAttribute('cy', f1(o[1] + 6 + k * k * 50)); d.setAttribute('opacity', (w * (1 - k)).toFixed(2)); }); },
  },
  'ring': {
    make: g => [0, 1, 2].map(() => el('path', { fill: 'none', stroke: '#2E9A7A', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => { const o = h.ear || [h.x + 34, h.y + 40]; s.forEach((a, i) => { const k = (c * 1.5 + i / 3) % 1, r = 6 + k * 10;
      a.setAttribute('d', `M${f1(o[0] + r * 0.7)} ${f1(o[1] - r * 0.7)} A${f1(r)} ${f1(r)} 0 0 1 ${f1(o[0] + r * 0.7)} ${f1(o[1] + r * 0.7)}`); a.setAttribute('opacity', (w * (1 - k)).toFixed(2)); }); },
  },
  'callback': {
    make: g => { const s = el('g', { opacity: 0 }, g); el('circle', { r: 11, fill: '#FFFFFF', stroke: '#2F7DB8', 'stroke-width': 1.6 }, s);
      el('path', { d: 'M5 -3 A6 6 0 1 0 5 4', fill: 'none', stroke: '#2F7DB8', 'stroke-width': 2.2, 'stroke-linecap': 'round' }, s);
      el('path', { d: 'M5 -3 l1 -4 M5 -3 l-4 -1', stroke: '#2F7DB8', 'stroke-width': 2.2, 'stroke-linecap': 'round' }, s); return s; },
    tick: (s, c, w, h) => { const o = h.phone || [h.x + 30, h.by - 40], k = (c * 0.8) % 1, sc = k < 0.25 ? backOut(k / 0.25) : 1;
      s.setAttribute('transform', `translate(${f1(o[0] + 34)} ${f1(o[1] - 16)}) scale(${Math.max(0.01, sc).toFixed(2)})`); s.setAttribute('opacity', (w * (k < 0.85 ? 1 : (1 - k) / 0.15)).toFixed(2)); },
  },
  'checkmark': {
    make: g => { const s = el('g', { opacity: 0 }, g); el('circle', { r: 12, fill: '#3E8E2F' }, s);
      el('path', { d: 'M-6 0 L-2 5 L7 -5', fill: 'none', stroke: '#FFFFFF', 'stroke-width': 3, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, s); return s; },
    tick: (s, c, w, h) => { const o = h.phone || [h.x + 30, h.by - 40], k = (c * 0.6) % 1, sc = k < 0.2 ? backOut(k / 0.2) : 1;
      s.setAttribute('transform', `translate(${f1(o[0] + 36)} ${f1(o[1] - 16)}) scale(${Math.max(0.01, sc).toFixed(2)})`); s.setAttribute('opacity', (w * (k < 0.85 ? 1 : (1 - k) / 0.15)).toFixed(2)); },
  },
  'knock-lines': {
    make: g => [0, 1, 2].map(() => el('path', { stroke: '#7A4A2E', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => { const o = h.pawR || [h.x + 40, h.by - 20], on = Math.sin(c * 1.6) > -0.2 && Math.sin(c * 11) > 0.3 ? 1 : 0;
      [[-6, -10, 1], [0, -12, 1], [6, -10, 1]].forEach(([dx, dy], i) => { s[i].setAttribute('d', `M${f1(o[0] + 8 + dx)} ${f1(o[1] + dy)} l${dx * 0.5} -5`); s[i].setAttribute('opacity', (w * on).toFixed(2)); }); },
  },
  'signature': {
    make: g => el('path', { d: 'M-8 0 q2 -6 4 0 t4 0 q2 -5 3 1 q1 3 5 -3', fill: 'none', stroke: '#2F5DA8', 'stroke-width': 1.6, 'stroke-linecap': 'round', 'stroke-dasharray': '40', 'stroke-dashoffset': 40, opacity: 0 }, g),
    tick: (e, c, w, h) => { const o = h.clip || [h.x + 10, h.by]; const k = (c * 0.45) % 1;
      e.setAttribute('transform', `translate(${f1(o[0])} ${f1(o[1] + 4)})`); e.setAttribute('stroke-dashoffset', f1(40 * (1 - clamp(k * 1.4)))); e.setAttribute('opacity', w.toFixed(2)); },
  },
  /* new in the noun decks */
  'cough-puffs': {
    make: g => [0, 1, 2].map(() => el('circle', { r: 3, fill: '#D6DCE6', opacity: 0 }, g)),
    tick: (s, c, w, h) => { const u = (c % 1.8) / 1.8; s.forEach((q, i) => { const k = clamp((u - 0.08 - i * 0.04) / 0.4);
      q.setAttribute('cx', f1(h.x + 8 + k * 26 + i * 4)); q.setAttribute('cy', f1(h.my - 2 - k * 6 + i * 3)); q.setAttribute('r', f1(2 + k * 4));
      q.setAttribute('opacity', (w * (k > 0 && k < 1 ? 1 - k : 0)).toFixed(2)); }); },
  },
  'pain': {
    make: g => [0, 1, 2].map(() => el('path', { d: 'M0 0 l4 -4 l4 4 l4 -4', fill: 'none', stroke: '#E5484D', 'stroke-width': 2.2, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', opacity: 0 }, g)),
    tick: (s, c, w, h) => s.forEach((z, i) => { const k = (c * 1.2 + i / 3) % 1;
      z.setAttribute('transform', `translate(${f1(h.x + 34 + k * 6)} ${f1(h.by - 14 + i * 12)})`); z.setAttribute('opacity', (w * Math.sin(Math.PI * k)).toFixed(2)); }),
  },
};

const BUBBLE = {};

/* ---------- one card's animation, as the device plays it ---------- */
const DUR = 7.4, B0 = 2.8;            /* loop length, bubble appears at B0 */
const STAGE = { x: 76, y: 168 };       /* feet position on the 200 × 200 disc */
const SCALE = 0.9;                     /* cat, props and effects, scaled around the feet */

class KatzeScene {
  constructor(svg, card) {
    svg.innerHTML = '';
    this.card = card; this.anim = card.anim || {};
    const id = 'c' + Math.random().toString(36).slice(2, 8);
    const clip = el('clipPath', { id }, el('defs', {}, svg)); el('circle', { cx: 100, cy: 100, r: 100 }, clip);
    const root = el('g', { 'clip-path': `url(#${id})` }, svg);
    el('rect', { x: 0, y: 0, width: 200, height: 200, fill: card.style.background }, root);
    [[30, 40, 18], [176, 150, 30], [168, 26, 12]].forEach(([cx, cy, r]) => el('circle', { cx, cy, r, fill: '#FFFFFF', opacity: 0.35 }, root));
    const charG = el('g', { transform: `translate(${STAGE.x} ${STAGE.y}) scale(${SCALE}) translate(${-STAGE.x} ${-STAGE.y})` }, root);
    const props = (this.anim.props || []).filter(x => PROP[x]);
    const sceneG = el('g', { transform: `translate(${STAGE.x} ${STAGE.y})` }, charG);
    this.shadow = el('ellipse', { cx: STAGE.x, cy: STAGE.y + 1, rx: 32, ry: 4.5, fill: '#000', opacity: 0.08 }, charG);
    const fxBack = el('g', {}, charG);
    this.cat = makeCat(charG);
    this.props = props.map(x => ({ def: PROP[x], s: PROP[x].make(PROP[x].layer === 'back' ? sceneG : PROP[x].layer === 'wear' ? this.cat.wear : this.cat.hand, card) }));
    const fxg = el('g', {}, charG);
    this.fx = (this.anim.fx || []).map(f => FX[f] ? { def: FX[f], s: FX[f].make(FX[f].layer === 'back' ? fxBack : fxg, card) } : null).filter(Boolean);
    this.bub = el('g', { opacity: 0 }, root);
    const target = Object.assign({}, N0, STAGE, this.anim.pose || {});
    const neutral = Object.assign({}, N0, STAGE);
    this.kf = [[0, neutral], [0.5, neutral], [1.1, target], [DUR - 0.6, target], [DUR, neutral]];
  }
  pose(t) {
    const kf = this.kf;
    for (let i = 0; i < kf.length - 1; i++) {
      const [t0, p0] = kf[i], [t1, p1] = kf[i + 1];
      if (t < t1) { const u = ease(clamp((t - t0) / (t1 - t0))), o = {}; for (const k in p0) o[k] = p0[k] + (p1[k] - p0[k]) * u; return o; }
    }
    return Object.assign({}, kf[kf.length - 1][1]);
  }
  render(t, c) {
    const p = this.pose(t);
    const w = clamp((t - 0.9) / 0.4) * clamp((DUR - 0.35 - t) / 0.45);
    (this.anim.motion || []).forEach(m => MOTION[m] && MOTION[m](p, c, w));
    const paws = drawCat(this.cat, p);
    this.shadow.setAttribute('cx', f1(p.x)); this.shadow.setAttribute('rx', f1(32 * p.sx - p.jump * 0.4));
    const top = p.y - p.jump - 134 * p.sy;
    const h = { x: p.x, y: top, my: p.y - p.jump - 73 * p.sy, by: p.y - p.jump - 50 * p.sy, floor: p.y, stageX: STAGE.x, stageY: STAGE.y };
    h.pawL = bodyToStage(p, ...paws.L); h.pawR = bodyToStage(p, ...paws.R);
    const P = { p, c, L: paws.L, R: paws.R, pub: h, stageX: STAGE.x, stageY: STAGE.y };
    this.props.forEach(x => x.def.tick(x.s, P));
    this.fx.forEach(f => f.def.tick(f.s, c, w, h));
  }
}
var Scene = KatzeScene; /* the player instantiates Scene */
