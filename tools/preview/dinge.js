/* Dinge drawing reference: noun cards that show the thing alone (front face, variant A).
   Loaded after katze.js and shares its helpers (el, clamp, ease, f1, FONT, txt, STAR, DROP).
   Stage: 200 × 200 disc. Each OBJ entry draws one object roughly centred on (100, 105) and
   returns tick(clock) for its small idle animation. Basic shapes only.
   Cards with anim.character = "katze" are drawn by the cat instead (variant B). */

const loop = (c, period) => (c % period) / period;                  /* 0..1 sawtooth */
const window_ = (u, a, b, fade) => clamp((u - a) / fade) * clamp((b - u) / fade);   /* 1 between a and b */

const OBJ = {
  'sofa': g => {
    el('ellipse', { cx: 100, cy: 160, rx: 70, ry: 5, fill: '#000', opacity: 0.07 }, g);
    [[46, 150], [148, 150]].forEach(([x, y]) => el('rect', { x, y, width: 6, height: 10, rx: 1.5, fill: '#7A4A2E' }, g));
    el('rect', { x: 40, y: 88, width: 120, height: 44, rx: 15, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 2.2 }, g);
    el('line', { x1: 100, y1: 94, x2: 100, y2: 126, stroke: '#7C6BD6', 'stroke-width': 1.8 }, g);
    el('rect', { x: 38, y: 118, width: 124, height: 32, rx: 10, fill: '#B3A6F5', stroke: '#7C6BD6', 'stroke-width': 2.2 }, g);
    el('line', { x1: 100, y1: 120, x2: 100, y2: 148, stroke: '#7C6BD6', 'stroke-width': 1.8 }, g);
    el('rect', { x: 26, y: 104, width: 22, height: 48, rx: 10, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 2.2 }, g);
    el('rect', { x: 152, y: 104, width: 22, height: 48, rx: 10, fill: '#9D8DF1', stroke: '#7C6BD6', 'stroke-width': 2.2 }, g);
    const pil = el('g', {}, g);
    el('rect', { x: -13, y: -13, width: 26, height: 26, rx: 7, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.8 }, pil);
    el('circle', { r: 2.4, fill: '#C99227' }, pil);
    return c => { const k = Math.abs(Math.sin(c * 2.6)), b = Math.pow(k, 3);
      pil.setAttribute('transform', `translate(70 ${f1(108 - b * 10)}) rotate(${f1(-14 + b * 10)}) scale(${(1 + (1 - k) * 0.08).toFixed(3)} ${(1 - (1 - k) * 0.1).toFixed(3)})`); };
  },
  'teppich': g => {
    el('path', { d: 'M52 112 L168 112 L148 164 L32 164 Z', fill: '#E5873A', stroke: '#B8652A', 'stroke-width': 2, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M62 120 L154 120 L139 156 L47 156 Z', fill: 'none', stroke: '#FFE3C2', 'stroke-width': 2.2, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M78 138 L100 128 L122 138 L100 148 Z', fill: '#F6C356' }, g);
    for (let i = 0; i < 6; i++) { el('line', { x1: 52 - i * 3.6, y1: 112 + i * 9.5, x2: 45 - i * 3.6, y2: 112 + i * 9.5, stroke: '#B8652A', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, g);
      el('line', { x1: 168 - i * 3.6, y1: 112 + i * 9.5, x2: 175 - i * 3.6, y2: 112 + i * 9.5, stroke: '#B8652A', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, g); }
    /* a small sleeping cat */
    const cat = el('g', { transform: 'translate(98 124)' }, g);
    el('path', { d: 'M14 4 C30 2 30 -14 20 -16', fill: 'none', stroke: '#C9772F', 'stroke-width': 7, 'stroke-linecap': 'round' }, cat);
    el('path', { d: 'M14 4 C30 2 30 -14 20 -16', fill: 'none', stroke: '#F3A25A', 'stroke-width': 3.6, 'stroke-linecap': 'round' }, cat);
    const body = el('ellipse', { cx: 0, cy: -2, rx: 22, ry: 12, fill: '#F3A25A', stroke: '#C9772F', 'stroke-width': 1.8 }, cat);
    el('path', { d: 'M-20 -8 L-22 -22 L-12 -12 Z M-8 -12 L-4 -24 L2 -12 Z', fill: '#F3A25A', stroke: '#C9772F', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }, cat);
    el('path', { d: 'M-16 -3 q2.5 2 5 0 M-6 -3 q2.5 2 5 0', fill: 'none', stroke: '#2B2230', 'stroke-width': 1.5, 'stroke-linecap': 'round' }, cat);
    const zs = [8, 10, 12].map(s => { const t = el('text', { 'font-size': s, 'font-weight': 800, fill: '#7C6BD6', 'font-family': FONT, opacity: 0 }, g); t.textContent = 'z'; return t; });
    return c => { body.setAttribute('ry', f1(12 + Math.sin(c * 2) * 0.8));
      zs.forEach((t, i) => { const k = (c * 0.45 + i / 3) % 1; t.setAttribute('x', f1(92 + k * 14 + i * 3)); t.setAttribute('y', f1(104 - k * 28)); t.setAttribute('opacity', Math.sin(k * Math.PI).toFixed(2)); }); };
  },
  'lampe': g => {
    el('line', { x1: 100, y1: 0, x2: 100, y2: 58, stroke: '#3B2F4A', 'stroke-width': 2 }, g);
    const cone = el('path', { d: 'M84 80 L116 80 L150 150 L50 150 Z', fill: '#FFE58A', opacity: 0 }, g);
    el('rect', { x: 52, y: 150, width: 96, height: 7, rx: 3, fill: '#A86B48' }, g);
    el('path', { d: 'M60 157 L58 184 M140 157 L142 184', stroke: '#A86B48', 'stroke-width': 4, 'stroke-linecap': 'round' }, g);
    el('path', { d: 'M100 136 q-8 -1 -8 7 l16 0 q0 -8 -8 -7 Z', fill: '#E5484D' }, g);
    const bulb = el('circle', { cx: 100, cy: 80, r: 7, fill: '#E9E2C8' }, g);
    el('path', { d: 'M74 80 Q76 56 100 56 Q124 56 126 80 Z', fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 2, 'stroke-linejoin': 'round' }, g);
    const bolt = el('path', { d: 'M132 70 L126 80 L132 80 L126 90', fill: 'none', stroke: '#E5484D', 'stroke-width': 2.2, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', opacity: 0 }, g);
    return c => { /* broken: it flickers */
      const u = loop(c, 2.4), on = (u < 0.18 || (u > 0.26 && u < 0.3) || (u > 0.55 && u < 0.62)) ? 1 : 0;
      cone.setAttribute('opacity', (on * 0.55).toFixed(2)); bulb.setAttribute('fill', on ? '#FFF7C2' : '#E9E2C8'); bolt.setAttribute('opacity', on ? 0 : 0.9); };
  },
  'regal': g => {
    el('rect', { x: 54, y: 38, width: 92, height: 134, rx: 3, fill: '#F3E3CF', stroke: '#A86B48', 'stroke-width': 3 }, g);
    [80, 122].forEach(y => el('rect', { x: 54, y, width: 92, height: 5, fill: '#A86B48' }, g));
    const book = (x, y, w, h, c2) => el('rect', { x, y: y - h, width: w, height: h, rx: 1.5, fill: c2 }, g);
    [[60, 9, 32, '#E5484D'], [70, 7, 36, '#4BA3D9'], [78, 10, 30, '#F2C14E'], [89, 8, 34, '#5E9E45'], [98, 9, 28, '#9C7AD6']].forEach(([x, w, h, c2]) => book(x, 80, w, h, c2));
    el('path', { d: 'M118 80 L130 80 L128 66 L120 66 Z', fill: '#D9824A' }, g); el('path', { d: 'M124 66 Q116 54 120 48 M124 66 Q132 54 128 46', fill: 'none', stroke: '#5E9E45', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    [[62, 10, 34, '#2F7DB8'], [73, 8, 30, '#E5873A'], [82, 9, 36, '#7A4FD6'], [92, 11, 32, '#E5484D']].forEach(([x, w, h, c2]) => book(x, 122, w, h, c2));
    const slide = el('rect', { x: 0, y: -33, width: 9, height: 33, rx: 1.5, fill: '#3E8E2F' }, g);
    [[62, 166, 46, 14, '#C9CED6'], [110, 166, 28, 20, '#F7B9C6']].forEach(([x, y, w, h, c2]) => el('rect', { x, y: y - h, width: w, height: h, rx: 2, fill: c2 }, g));
    return c => { const u = loop(c, 4), k = ease(window_(u, 0.15, 0.75, 0.15)); slide.setAttribute('transform', `translate(${f1(134 - k * 26)} 122)`); };
  },
  'schrank': g => {
    el('rect', { x: 56, y: 28, width: 88, height: 144, rx: 5, fill: '#C99A6B', stroke: '#8C5B3A', 'stroke-width': 2.4 }, g);
    el('rect', { x: 60, y: 34, width: 80, height: 128, rx: 2, fill: '#6B4423' }, g);
    el('line', { x1: 62, y1: 46, x2: 138, y2: 46, stroke: '#9AA4B1', 'stroke-width': 2.4 }, g);
    el('path', { d: 'M108 46 L108 50 M100 58 L108 50 L116 58', fill: 'none', stroke: '#C9CED6', 'stroke-width': 1.6 }, g);
    el('path', { d: 'M96 58 L102 54 Q108 57 114 54 L120 58 L118 66 L116 64 L116 96 L100 96 L100 64 L98 66 Z', fill: '#4A6FA5', stroke: '#33507A', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M124 58 L132 54 L138 58 L136 92 L126 92 Z', fill: '#E5484D' }, g);
    el('rect', { x: 56, y: 172, width: 10, height: 6, fill: '#8C5B3A' }, g); el('rect', { x: 134, y: 172, width: 10, height: 6, fill: '#8C5B3A' }, g);
    el('rect', { x: 58, y: 30, width: 42, height: 140, rx: 3, fill: '#D9AE7E', stroke: '#8C5B3A', 'stroke-width': 2 }, g);
    el('circle', { cx: 94, cy: 100, r: 2.6, fill: '#F2C14E' }, g);
    const door = el('g', {}, g);
    el('rect', { x: -42, y: 0, width: 42, height: 140, rx: 3, fill: '#D9AE7E', stroke: '#8C5B3A', 'stroke-width': 2 }, door);
    el('circle', { cx: -36, cy: 70, r: 2.6, fill: '#F2C14E' }, door);
    el('rect', { x: -36, y: 12, width: 30, height: 50, rx: 2, fill: 'none', stroke: '#B88A5E', 'stroke-width': 1.4 }, door);
    return c => { const u = loop(c, 5), k = ease(window_(u, 0.15, 0.8, 0.15)); door.setAttribute('transform', `translate(142 30) scale(${(1 - 0.85 * k).toFixed(3)} 1)`); };
  },
  'spiegel': g => {
    for (let x = 20; x < 190; x += 20) el('line', { x1: x, y1: 0, x2: x, y2: 200, stroke: '#FFFFFF', 'stroke-width': 1.2, opacity: 0.6 }, g);
    for (let y = 20; y < 200; y += 20) el('line', { x1: 0, y1: y, x2: 200, y2: y, stroke: '#FFFFFF', 'stroke-width': 1.2, opacity: 0.6 }, g);
    el('path', { d: 'M62 150 L138 150 Q136 172 100 172 Q64 172 62 150 Z', fill: '#FFFFFF', stroke: '#B9BFCC', 'stroke-width': 2 }, g);
    el('path', { d: 'M100 150 L100 142 Q100 138 106 138', fill: 'none', stroke: '#9AA4B1', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    const clipId = 'm' + Math.random().toString(36).slice(2, 7);
    const cp = el('clipPath', { id: clipId }, el('defs', {}, g)); el('ellipse', { cx: 100, cy: 84, rx: 34, ry: 46 }, cp);
    el('ellipse', { cx: 100, cy: 84, rx: 38, ry: 50, fill: '#C99A6B' }, g);
    el('ellipse', { cx: 100, cy: 84, rx: 34, ry: 46, fill: '#DDF1FB' }, g);
    const shine = el('path', { d: 'M0 0 L12 0 L-28 90 L-40 90 Z', fill: '#FFFFFF', opacity: 0.85, 'clip-path': `url(#${clipId})` }, g);
    return c => { const u = loop(c, 3); shine.setAttribute('transform', `translate(${f1(60 + u * 110)} 36)`); };
  },
  'herd': g => {
    el('rect', { x: 52, y: 92, width: 96, height: 82, rx: 5, fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 2.2 }, g);
    el('rect', { x: 52, y: 92, width: 96, height: 12, rx: 3, fill: '#D6DCE6' }, g);
    [64, 80, 120, 136].forEach(x => el('circle', { cx: x, cy: 98, r: 3, fill: '#6B7180' }, g));
    el('rect', { x: 62, y: 112, width: 76, height: 50, rx: 5, fill: '#3B2F4A' }, g);
    const glow = el('rect', { x: 66, y: 116, width: 68, height: 42, rx: 4, fill: '#F28C28', opacity: 0.5 }, g);
    el('rect', { x: 70, y: 106, width: 60, height: 4, rx: 2, fill: '#9AA4B1' }, g);
    el('ellipse', { cx: 76, cy: 90, rx: 16, ry: 3, fill: '#3B2F4A' }, g);
    const ring = el('ellipse', { cx: 124, cy: 90, rx: 14, ry: 3, fill: 'none', stroke: '#E5484D', 'stroke-width': 2.4 }, g);
    el('path', { d: 'M62 88 L62 66 Q62 62 66 62 L86 62 Q90 62 90 66 L90 88 Z', fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 1.8 }, g);
    el('path', { d: 'M58 64 L94 64', stroke: '#2F7DB8', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    const steam = [0, 1, 2].map(() => el('path', { d: 'M0 0 q-3 -4 0 -8 q3 -4 0 -8', fill: 'none', stroke: '#C9D3DE', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0 }, g));
    const heat = [0, 1, 2].map(() => el('path', { d: 'M0 0 q-3 -3 0 -6 q3 -3 0 -6', fill: 'none', stroke: '#F28C28', 'stroke-width': 1.8, 'stroke-linecap': 'round', opacity: 0 }, g));
    return c => { const p = 0.5 + 0.5 * Math.sin(c * 3);
      glow.setAttribute('opacity', (0.35 + 0.3 * p).toFixed(2)); ring.setAttribute('stroke', p > 0.5 ? '#FF6B4A' : '#E5484D');
      steam.forEach((s, i) => { const k = (c * 0.6 + i / 3) % 1; s.setAttribute('transform', `translate(${f1(68 + i * 7)} ${f1(58 - k * 18)})`); s.setAttribute('opacity', Math.sin(Math.PI * k).toFixed(2)); });
      heat.forEach((s, i) => { const k = (c * 0.8 + i / 3) % 1; s.setAttribute('transform', `translate(${f1(116 + i * 8)} ${f1(84 - k * 16)})`); s.setAttribute('opacity', Math.sin(Math.PI * k).toFixed(2)); }); };
  },
  'heizung': g => {
    el('path', { d: 'M40 150 L40 168 M160 150 L160 168', stroke: '#9AA4B1', 'stroke-width': 4 }, g);
    for (let i = 0; i < 7; i++) el('rect', { x: 44 + i * 16, y: 98, width: 12, height: 62, rx: 6, fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 1.8 }, g);
    el('rect', { x: 40, y: 150, width: 120, height: 6, rx: 3, fill: '#D6DCE6' }, g);
    el('circle', { cx: 158, cy: 104, r: 7, fill: '#F6F8FB', stroke: '#AEB6C4', 'stroke-width': 1.8 }, g);
    txt(g, 158, 107, '5', 7, '#E5484D');
    const waves = [0, 1, 2, 3].map(() => el('path', { d: 'M0 0 q-4 -5 0 -10 q4 -5 0 -10', fill: 'none', stroke: '#F28C28', 'stroke-width': 2.2, 'stroke-linecap': 'round', opacity: 0 }, g));
    return c => waves.forEach((s, i) => { const k = (c * 0.5 + i / 4) % 1; s.setAttribute('transform', `translate(${f1(62 + i * 25)} ${f1(92 - k * 30)})`); s.setAttribute('opacity', Math.sin(Math.PI * k).toFixed(2)); });
  },
  'steckdose': g => {
    el('rect', { x: 62, y: 60, width: 76, height: 76, rx: 14, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 2.4 }, g);
    el('circle', { cx: 100, cy: 98, r: 26, fill: '#F3F5F8', stroke: '#D6DCE6', 'stroke-width': 2 }, g);
    [90, 110].forEach(x => el('circle', { cx: x, cy: 98, r: 3.6, fill: '#3B2F4A' }, g));
    const plug = el('g', {}, g);
    el('path', { d: 'M0 0 Q30 20 70 10', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 4, 'stroke-linecap': 'round' }, plug);
    el('rect', { x: -14, y: -11, width: 18, height: 22, rx: 6, fill: '#3B2F4A' }, plug);
    [-5, 5].forEach(y => el('rect', { x: -22, y: y - 1.4, width: 9, height: 2.8, rx: 1.2, fill: '#C9CED6' }, plug));
    const bolt = el('path', { d: 'M150 56 L142 70 L150 70 L142 84', fill: 'none', stroke: '#F2C14E', 'stroke-width': 3, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', opacity: 0 }, g);
    return c => { const u = loop(c, 4), k = ease(window_(u, 0.1, 0.75, 0.15));
      plug.setAttribute('transform', `translate(${f1(150 - k * 28)} ${f1(132 - k * 34)})`); bolt.setAttribute('opacity', (k > 0.95 ? 0.5 + 0.5 * Math.sin(c * 12) : 0).toFixed(2)); };
  },
  /* a2-gesundheit-nomen */
  'arzt': g => {
    el('path', { d: 'M48 176 L56 104 Q60 92 76 90 L124 90 Q140 92 144 104 L152 176 Z', fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 2 }, g);
    el('path', { d: 'M86 90 L100 122 L114 90 Z', fill: '#BFE3F2' }, g);
    el('path', { d: 'M86 90 L94 130 L100 122 M114 90 L106 130 L100 122', fill: 'none', stroke: '#C9CED6', 'stroke-width': 2 }, g);
    el('rect', { x: 120, y: 132, width: 18, height: 4, rx: 2, fill: '#4BA3D9' }, g);
    el('path', { d: 'M82 92 Q72 128 92 140 Q104 146 112 136', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    el('path', { d: 'M118 92 Q124 118 112 136', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    const chest = el('g', {}, g);
    el('circle', { r: 7, fill: '#9AA4B1', stroke: '#6B7180', 'stroke-width': 2 }, chest); el('circle', { r: 3, fill: '#D6DCE6' }, chest);
    const heart = el('path', { d: 'M0 -2 C-3 -7 -9 -5 -7 0 L0 6 L7 0 C9 -5 3 -7 0 -2 Z', fill: '#F06C8B' }, g);
    const ecg = el('path', { d: 'M40 62 L70 62 L76 50 L84 74 L90 56 L94 62 L160 62', fill: 'none', stroke: '#E5484D', 'stroke-width': 2.4, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'stroke-dasharray': '140', 'stroke-dashoffset': 140 }, g);
    return c => { const b = Math.pow(Math.max(0, Math.sin(c * 5)), 6);
      heart.setAttribute('transform', `translate(140 108) scale(${(1.1 + b * 0.4).toFixed(2)})`);
      chest.setAttribute('transform', `translate(112 ${f1(140 + Math.sin(c * 2) * 2)})`);
      ecg.setAttribute('stroke-dashoffset', f1(140 * (1 - loop(c, 1.6)))); };
  },
  'praxis': g => {
    el('rect', { x: 30, y: 30, width: 140, height: 150, fill: '#F3E9DC' }, g);
    el('rect', { x: 70, y: 70, width: 60, height: 110, rx: 3, fill: '#B07A4E', stroke: '#7A4A2E', 'stroke-width': 2 }, g);
    el('rect', { x: 78, y: 80, width: 44, height: 34, rx: 2, fill: '#BFE3F2' }, g);
    el('circle', { cx: 120, cy: 130, r: 3, fill: '#F2C14E' }, g);
    const plate = el('g', {}, g);
    el('rect', { x: 134, y: 76, width: 32, height: 24, rx: 2, fill: '#E4C27A', stroke: '#B08A3E', 'stroke-width': 1.4 }, plate);
    txt(plate, 150, 86, 'Praxis', 5.5, '#5B4214'); txt(plate, 150, 94, 'Dr. Weber', 5, '#5B4214');
    el('rect', { x: 36, y: 80, width: 28, height: 36, rx: 2, fill: '#FFFFFF', stroke: '#C9CED6' }, g);
    txt(g, 50, 89, 'Mo–Fr', 5, '#3B2F4A');
    [96, 102, 108].forEach(y => el('line', { x1: 40, y1: y, x2: 60, y2: y, stroke: '#C9CED6', 'stroke-width': 1.2 }, g));
    const mi = el('line', { x1: 40, y1: 102, x2: 60, y2: 102, stroke: '#E5484D', 'stroke-width': 2, opacity: 0 }, g);
    const glint = el('path', { d: 'M0 0 L6 0 L-4 24 L-10 24 Z', fill: '#FFFFFF', opacity: 0.7 }, plate);
    return c => { const u = loop(c, 3); glint.setAttribute('transform', `translate(${f1(132 + u * 40)} 76)`); glint.setAttribute('opacity', (0.7 * window_(u, 0, 1, 0.2)).toFixed(2));
      mi.setAttribute('opacity', Math.sin(c * 3) > 0 ? 0.9 : 0.2); };
  },
  'krankenkasse': g => {
    const card = el('g', {}, g);
    el('rect', { x: -48, y: -30, width: 96, height: 60, rx: 7, fill: '#4BA3D9', stroke: '#2F7DB8', 'stroke-width': 2 }, card);
    el('rect', { x: -48, y: 8, width: 96, height: 10, fill: '#2F7DB8' }, card);
    el('rect', { x: -38, y: -18, width: 16, height: 12, rx: 2, fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1 }, card);
    el('path', { d: 'M-38 -12 L-22 -12 M-30 -18 L-30 -6', stroke: '#C99227', 'stroke-width': 1 }, card);
    el('rect', { x: 14, y: -22, width: 24, height: 26, rx: 3, fill: '#DDF1FB' }, card);
    el('circle', { cx: 26, cy: -12, r: 5, fill: '#9AA4B1' }, card); el('path', { d: 'M17 4 Q26 -6 35 4 Z', fill: '#9AA4B1' }, card);
    el('path', { d: 'M-38 0 L4 0 M-38 -4 L0 -4', stroke: '#FFFFFF', 'stroke-width': 2, 'stroke-linecap': 'round', opacity: 0.8 }, card);
    el('path', { d: 'M-4 12 C-6 9 -10 10 -9 13 L-4 17 L1 13 C2 10 -2 9 -4 12 Z', fill: '#FFFFFF' }, card);
    return c => card.setAttribute('transform', `translate(100 104) rotate(${f1(-8 + Math.sin(c * 1.4) * 6)}) scale(${(1 + Math.sin(c * 1.4) * 0.03).toFixed(3)})`);
  },
  'rezept': g => {
    el('rect', { x: 50, y: 52, width: 100, height: 110, rx: 3, fill: '#F7C6D3', stroke: '#D9708E', 'stroke-width': 2 }, g);
    txt(g, 70, 70, 'Rp.', 11, '#B0506C', 'start');
    [86, 98, 110, 122].forEach((y, i) => el('line', { x1: 60, y1: y, x2: 140 - i * 12, y2: y, stroke: '#D9708E', 'stroke-width': 1.6 }, g));
    el('rect', { x: 60, y: 132, width: 40, height: 20, rx: 2, fill: 'none', stroke: '#D9708E', 'stroke-width': 1.4 }, g);
    const imprint = el('g', { opacity: 0 }, g);
    el('circle', { cx: 124, cy: 142, r: 12, fill: 'none', stroke: '#2F5DA8', 'stroke-width': 2 }, imprint);
    el('path', { d: 'M118 142 L130 142 M124 136 L124 148', stroke: '#2F5DA8', 'stroke-width': 1.6 }, imprint);
    const stamp = el('g', {}, g);
    el('rect', { x: -10, y: -6, width: 20, height: 8, rx: 2, fill: '#3B2F4A' }, stamp);
    el('rect', { x: -4, y: -24, width: 8, height: 18, rx: 3, fill: '#A86B48' }, stamp);
    el('circle', { cx: 0, cy: -27, r: 6, fill: '#A86B48' }, stamp);
    return c => { const u = loop(c, 3), down = window_(u, 0.3, 0.42, 0.06);
      stamp.setAttribute('transform', `translate(124 ${f1(118 - 34 * (1 - down) + 18)})`); imprint.setAttribute('opacity', u > 0.36 ? 1 : 0); };
  },
  'apotheke': g => {
    el('rect', { x: 30, y: 100, width: 140, height: 80, fill: '#F3F4F6' }, g);
    el('rect', { x: 44, y: 116, width: 50, height: 50, rx: 2, fill: '#BFE3F2', stroke: '#9AA4B1', 'stroke-width': 1.6 }, g);
    el('rect', { x: 110, y: 116, width: 40, height: 64, rx: 2, fill: '#6B7180' }, g);
    el('rect', { x: 56, y: 140, width: 10, height: 18, rx: 2, fill: '#E5873A' }, g); el('rect', { x: 72, y: 146, width: 14, height: 12, rx: 1.5, fill: '#FFFFFF' }, g);
    el('line', { x1: 100, y1: 40, x2: 132, y2: 40, stroke: '#6B7180', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    const sign = el('g', {}, g);
    el('line', { x1: -8, y1: 0, x2: -8, y2: 10, stroke: '#6B7180', 'stroke-width': 1.6 }, sign); el('line', { x1: 8, y1: 0, x2: 8, y2: 10, stroke: '#6B7180', 'stroke-width': 1.6 }, sign);
    el('rect', { x: -20, y: 10, width: 40, height: 44, rx: 6, fill: '#E5484D' }, sign);
    txt(sign, 0, 46, 'A', 32, '#FFFFFF');
    return c => sign.setAttribute('transform', `translate(116 40) rotate(${f1(Math.sin(c * 1.6) * 6)})`);
  },
  'medikament': g => {
    el('rect', { x: 104, y: 112, width: 52, height: 52, rx: 3, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 2 }, g);
    el('rect', { x: 104, y: 126, width: 52, height: 12, fill: '#4BA3D9' }, g);
    txt(g, 130, 156, '20 St.', 7, '#6B7180');
    const bottle = el('g', {}, g);
    el('rect', { x: -14, y: -54, width: 28, height: 14, rx: 3, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.6 }, bottle);
    el('path', { d: 'M-20 -38 Q-20 -42 -14 -42 L14 -42 Q20 -42 20 -38 L20 0 Q20 4 16 4 L-16 4 Q-20 4 -20 0 Z', fill: '#C97A2E', stroke: '#9B5A1E', 'stroke-width': 1.6 }, bottle);
    el('rect', { x: -16, y: -30, width: 32, height: 20, rx: 2, fill: '#FFFFFF' }, bottle);
    el('rect', { x: -9, y: -24, width: 18, height: 8, rx: 4, fill: '#F06C8B' }, bottle); el('rect', { x: 0, y: -24, width: 9, height: 8, rx: 4, fill: '#FFFFFF', stroke: '#F06C8B', 'stroke-width': 1 }, bottle);
    const sp = el('path', { d: STAR, fill: '#FFC93C', opacity: 0 }, g);
    return c => { bottle.setAttribute('transform', `translate(80 164) rotate(${f1(Math.sin(c * 2.2) * 4)})`);
      const tw = Math.abs(Math.sin(c * 2)); sp.setAttribute('transform', `translate(58 108) scale(${(0.4 + tw * 0.7).toFixed(2)})`); sp.setAttribute('opacity', tw.toFixed(2)); };
  },
  'tablette': g => {
    el('rect', { x: 46, y: 70, width: 84, height: 62, rx: 6, fill: '#D6DCE6', stroke: '#9AA4B1', 'stroke-width': 2 }, g);
    const slots = [];
    for (let r = 0; r < 2; r++) for (let k = 0; k < 4; k++) { const x = 58 + k * 20, y = 88 + r * 26;
      el('ellipse', { cx: x, cy: y, rx: 8, ry: 9, fill: '#EEF0F4', stroke: '#B9BFCC', 'stroke-width': 1.4 }, g);
      slots.push(el('ellipse', { cx: x, cy: y, rx: 5, ry: 6, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1 }, g)); }
    el('path', { d: 'M140 120 L164 120 L161 164 L143 164 Z', fill: '#DDF1FB', stroke: '#9AA4B1', 'stroke-width': 2, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M142 134 L162 134 L161 162 L143 162 Z', fill: '#9FD3EE', opacity: 0.8 }, g);
    const pill = el('ellipse', { rx: 5, ry: 6, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1 }, g);
    return c => { const u = loop(c, 3), k = clamp((u - 0.2) / 0.5);
      slots[7].setAttribute('opacity', u > 0.2 ? 0 : 1);
      pill.setAttribute('cx', f1(118 + k * 34)); pill.setAttribute('cy', f1(114 - Math.sin(Math.PI * k) * 26 + k * 16)); pill.setAttribute('opacity', u > 0.2 && u < 0.75 ? 1 : 0); };
  },
};

/* small placement corrections, applied to the whole object */
const SHIFT = { 'lampe': [0, 14], 'arzt': [0, -16], 'medikament': [0, -16], 'tablette': [0, 4] };

class DingeScene {
  constructor(svg, card) {
    svg.innerHTML = '';
    const id = 'd' + Math.random().toString(36).slice(2, 8);
    const clip = el('clipPath', { id }, el('defs', {}, svg)); el('circle', { cx: 100, cy: 100, r: 100 }, clip);
    const g = el('g', { 'clip-path': `url(#${id})` }, svg);
    el('rect', { width: 200, height: 200, fill: card.style.background }, g);
    [[34, 44, 20], [170, 156, 30], [166, 30, 12]].forEach(([cx, cy, r]) => el('circle', { cx, cy, r, fill: '#FFFFFF', opacity: 0.4 }, g));
    this.ticks = ((card.anim || {}).props || []).filter(x => OBJ[x]).map(x => OBJ[x](el('g', { transform: `translate(${(SHIFT[x] || [0, 0]).join(' ')})` }, g), card));
  }
  render(t, c) { this.ticks.forEach(f => f(c)); }
}

/* the player instantiates Scene: cards with anim.character "katze" are drawn by the cat */
var Scene = function (svg, card) {
  const who = (card.anim && card.anim.character) || (typeof DECK !== 'undefined' ? DECK.character : 'dinge');
  return who === 'katze' ? new KatzeScene(svg, card) : new DingeScene(svg, card);
};
