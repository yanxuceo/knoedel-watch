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

/* ---------- new in a2-berg-nomen (absolute 200 × 200 coordinates; Knödel cameos use buddy() from katze.js) ---------- */
Object.assign(OBJ, {
  'berg': g => {
    const P = el('g', { transform: 'translate(100 158) scale(0.78)' }, g);
    el('path', { d: 'M-120 0 L-96 -60 L-84 -52 L-66 -104 L-50 -80 L-34 -118 L-18 -150 L-2 -122 L12 -136 L30 -96 L46 -112 L64 -70 L80 -84 L120 0 Z', fill: '#EEE7DD', stroke: '#C9BBA8', 'stroke-width': 2, 'stroke-linejoin': 'round' }, P);
    const glow = el('path', { d: 'M-120 0 L-96 -60 L-84 -52 L-66 -104 L-50 -80 L-34 -118 L-18 -150 L-2 -122 L12 -136 L30 -96 L46 -112 L64 -70 L80 -84 L120 0 Z', fill: '#F59AA8', opacity: 0 }, P);
    el('path', { d: 'M-18 -150 L-12 -104 L-2 -122 Z M-66 -104 L-60 -70 L-50 -80 Z M12 -136 L18 -100 L30 -96 Z M46 -112 L52 -78 L64 -70 Z', fill: '#DDD2C2', opacity: 0.9 }, P);
    el('path', { d: 'M-24 -140 L-18 -150 L-12 -138 L-18 -141 Z M8 -128 L12 -136 L17 -126 Z M-70 -96 L-66 -104 L-61 -94 Z', fill: '#FFFFFF' }, P);
    el('path', { d: 'M-120 -6 Q-60 -34 0 -16 Q60 0 120 -22 L120 30 L-120 30 Z', fill: '#A8D08D' }, P);
    el('path', { d: 'M-120 10 Q-30 -6 40 6 Q90 14 120 4 L120 30 L-120 30 Z', fill: '#8CC06E' }, P);
    fir(P, -70, -6, 0.9); fir(P, -54, -2, 0.7, '#4E9E68'); fir(P, 70, -4, 0.8);
    const cloud = el('g', {}, g);
    [[0, 0, 9], [10, 2, 7], [-9, 3, 6]].forEach(([cx, cy, r]) => el('circle', { cx, cy, r, fill: '#FFFFFF', opacity: 0.95 }, cloud));
    return c => { glow.setAttribute('opacity', (0.28 * (0.5 + 0.5 * Math.sin(c * 0.9))).toFixed(2));
      cloud.setAttribute('transform', `translate(${f1(40 + ((c * 9) % 140))} 62)`); };
  },
  'wanderschuh': g => {
    el('ellipse', { cx: 100, cy: 156, rx: 64, ry: 5, fill: '#000', opacity: 0.08 }, g);
    const boot = (x, y, col) => { const b = el('g', {}, g);
      el('path', { d: 'M0 0 L0 -54 Q0 -60 6 -60 L30 -60 Q36 -60 36 -54 L36 -28 Q60 -24 66 -12 L66 0 Z', fill: col, stroke: '#6E4528', 'stroke-width': 2, 'stroke-linejoin': 'round' }, b);
      el('path', { d: 'M-2 0 L68 0 L68 8 Q68 10 66 10 L0 10 Q-2 10 -2 8 Z', fill: '#3B2F4A' }, b);
      for (let i = 0; i < 8; i++) el('rect', { x: 2 + i * 8.4, y: 8, width: 4, height: 4, fill: '#3B2F4A' }, b);
      el('path', { d: 'M36 -26 Q44 -20 52 -18', fill: 'none', stroke: '#6E4528', 'stroke-width': 1.6 }, b);
      el('path', { d: 'M0 -12 L66 -12', stroke: '#6E4528', 'stroke-width': 1.4, opacity: 0.6 }, b);
      el('rect', { x: -1, y: -62, width: 38, height: 8, rx: 3, fill: '#E5484D' }, b);
      [-48, -40, -32].forEach(yy => el('path', { d: `M26 ${yy} L36 ${yy + 3}`, stroke: '#F2C14E', 'stroke-width': 2, 'stroke-linecap': 'round' }, b));
      const bow = el('path', { d: 'M36 -30 Q44 -38 46 -30 Q44 -24 36 -30 Q30 -40 26 -32 Q30 -24 36 -30 Z', fill: '#F2C14E' }, b);
      b.setAttribute('transform', `translate(${x} ${y})`); return { b, bow, x, y }; };
    const back = boot(76, 146, '#B07A4F'), front = boot(46, 154, '#C68B59');
    return c => { const k = Math.max(0, Math.sin(c * 3)), t = Math.pow(k, 2);
      front.b.setAttribute('transform', `translate(46 ${f1(154 - t * 8)}) rotate(${f1(-t * 6)} 0 0)`);
      back.bow.setAttribute('transform', `rotate(${f1(Math.sin(c * 4) * 6)} 36 -30)`); };
  },
  'weg': g => {
    const P = el('g', { transform: 'translate(100 124) scale(0.82 0.55)' }, g);
    peaks(P, DAY);
    el('path', { d: 'M20 200 Q30 136 96 120 Q134 110 112 102 Q98 96 108 90', fill: 'none', stroke: '#E8D3A8', 'stroke-width': 12, 'stroke-linecap': 'round' }, g);
    const steps = [[30, 176], [44, 150], [64, 132], [92, 116], [122, 102], [140, 86]].map(([x, y], i) => el('ellipse', { cx: x + (i % 2 ? 4 : -4), cy: y, rx: 3.4 - i * 0.3, ry: 2 - i * 0.15, fill: '#B0915E', opacity: 0 }, g));
    el('rect', { x: 140, y: 90, width: 6, height: 80, fill: '#A86B48' }, g);
    const sign = el('g', {}, g);
    el('path', { d: 'M108 96 L168 96 L178 106 L168 116 L108 116 Z', fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, sign);
    el('path', { d: 'M168 96 L178 106 L168 116 Z', fill: '#E5484D' }, sign);
    txt(sign, 112, 110, 'Hütte', 9, '#2B2230', 'start'); txt(sign, 164, 110, '1 h', 9, '#2B2230', 'end');
    el('path', { d: 'M30 152 Q36 140 50 142 Q58 136 64 146 Q70 154 60 160 L34 160 Q26 158 30 152 Z', fill: '#BDB7AE', stroke: '#8E8880', 'stroke-width': 1.4 }, g);
    el('rect', { x: 40, y: 146, width: 14, height: 3, fill: '#E5484D' }, g); el('rect', { x: 40, y: 149, width: 14, height: 3, fill: '#FFFFFF' }, g); el('rect', { x: 40, y: 152, width: 14, height: 3, fill: '#E5484D' }, g);
    return c => { const u = loop(c, 3.6);
      steps.forEach((s, i) => s.setAttribute('opacity', (window_(u, i * 0.1, i * 0.1 + 0.45, 0.06) * 0.85).toFixed(2)));
      sign.setAttribute('transform', `rotate(${f1(Math.sin(c * 1.5) * 1.5)} 143 106)`); };
  },
  'wald': g => {
    el('path', { d: 'M0 150 Q100 136 200 150 L200 200 L0 200 Z', fill: '#8CC06E' }, g);
    const trees = [[48, 150, 1.5, '#4E9E68'], [150, 148, 1.6, '#3E8E5A'], [100, 156, 2.2, '#3E8E5A'], [72, 162, 1.2, '#5BAF74'], [128, 164, 1.25, '#4E9E68']].map(([x, y, s, col]) => {
      const t = el('g', {}, g); fir(t, 0, 0, s, col); return [t, x, y]; });
    const shroom = el('g', { transform: 'translate(150 170)' }, g);
    el('rect', { x: -2, y: -6, width: 4, height: 7, rx: 1.5, fill: '#FFF6DA' }, shroom);
    el('path', { d: 'M-7 -5 Q0 -14 7 -5 Z', fill: '#E5484D' }, shroom);
    el('circle', { cx: -2, cy: -8, r: 1, fill: '#FFFFFF' }, shroom); el('circle', { cx: 2.5, cy: -7, r: 0.9, fill: '#FFFFFF' }, shroom);
    const bird = el('path', { d: 'M-6 0 Q-3 -4 0 0 Q3 -4 6 0', fill: 'none', stroke: '#3B2F4A', 'stroke-width': 1.8, 'stroke-linecap': 'round' }, g);
    return c => { trees.forEach(([t, x, y], i) => t.setAttribute('transform', `translate(${x} ${y}) rotate(${f1(Math.sin(c * 1.4 + i) * 1.6)})`));
      const u = loop(c, 5); bird.setAttribute('transform', `translate(${f1(20 + u * 170)} ${f1(56 - Math.sin(u * Math.PI) * 14)}) scale(1 ${(0.6 + 0.4 * Math.abs(Math.sin(c * 9))).toFixed(2)})`); };
  },
  'wiese': g => {
    const P = el('g', { transform: 'translate(100 112) scale(0.8 0.5)' }, g);
    peaks(P, DAY);
    el('path', { d: 'M0 118 Q60 100 120 108 Q170 114 200 104 L200 200 L0 200 Z', fill: '#A8D08D' }, g);
    el('path', { d: 'M0 150 Q80 136 200 146 L200 200 L0 200 Z', fill: '#8CC06E' }, g);
    const bl = el('g', { transform: 'translate(104 150) skewX(-20)' }, g);
    el('rect', { x: -26, y: -10, width: 52, height: 20, fill: '#FFFFFF' }, bl);
    for (let i = 0; i < 6; i++) for (let j = 0; j < 2; j++) if ((i + j) % 2 === 0) el('rect', { x: -26 + i * 8.66, y: -10 + j * 10, width: 8.66, height: 10, fill: '#E5484D', opacity: 0.85 }, bl);
    el('path', { d: 'M96 132 L118 132 L115 146 L99 146 Z', fill: '#C68B59', stroke: '#8A5A3B', 'stroke-width': 1.4 }, g);
    el('path', { d: 'M100 132 Q107 120 114 132', fill: 'none', stroke: '#8A5A3B', 'stroke-width': 2 }, g);
    const fl = [[34, 166, '#F28CA8'], [52, 176, '#FFFFFF'], [150, 170, '#F2C14E'], [168, 160, '#F28CA8'], [70, 160, '#9C7AD6'], [134, 180, '#FFFFFF'], [44, 140, '#F2C14E'], [160, 138, '#FFFFFF']].map(([x, y, col]) => {
      const f = el('g', {}, g);
      el('path', { d: 'M0 0 L0 10', stroke: '#5E9E45', 'stroke-width': 1.6 }, f);
      [0, 72, 144, 216, 288].forEach(a => el('ellipse', { cx: 0, cy: -3.2, rx: 2.4, ry: 3.4, fill: col, transform: `rotate(${a})` }, f));
      el('circle', { r: 1.8, fill: '#F2A93B' }, f); return [f, x, y]; });
    const bf = el('g', {}, g);
    el('path', { d: 'M0 0 C-5 -8 -12 -4 -8 1 C-10 5 -4 8 0 1 C4 8 10 5 8 1 C12 -4 5 -8 0 0 Z', fill: '#9C7AD6' }, bf);
    return c => { fl.forEach(([f, x, y], i) => f.setAttribute('transform', `translate(${x} ${y}) rotate(${f1(Math.sin(c * 2 + i) * 9)})`));
      const k = c * 0.7; bf.setAttribute('transform', `translate(${f1(100 + Math.sin(k) * 44)} ${f1(96 + Math.sin(k * 2.3) * 12)}) scale(1 ${(0.55 + 0.45 * Math.abs(Math.sin(c * 12))).toFixed(2)})`); };
  },
  'kuh': g => {
    el('path', { d: 'M0 158 Q100 146 200 158 L200 200 L0 200 Z', fill: '#A8D08D' }, g);
    const cow = el('g', {}, g);
    const tail = el('path', { d: 'M0 0 Q10 10 6 26', fill: 'none', stroke: '#F6F3EE', 'stroke-width': 3.4, 'stroke-linecap': 'round' }, cow);
    [[70, 128], [84, 132], [118, 132], [132, 128]].forEach(([x, y]) => { el('rect', { x: x - 5, y, width: 10, height: 30, rx: 3, fill: '#F6F3EE', stroke: '#B9B4AD', 'stroke-width': 1.4 }, cow); el('rect', { x: x - 5, y: y + 24, width: 10, height: 6, rx: 2, fill: '#5B4A44' }, cow); });
    el('ellipse', { cx: 100, cy: 116, rx: 44, ry: 26, fill: '#F6F3EE', stroke: '#B9B4AD', 'stroke-width': 1.8 }, cow);
    el('path', { d: 'M70 102 Q80 94 90 104 Q86 116 74 114 Z M114 112 Q128 104 136 116 Q128 128 116 124 Z M96 126 Q104 122 108 130 Q100 136 94 132 Z', fill: '#3B2F4A' }, cow);
    const head = el('g', {}, cow);
    el('path', { d: 'M84 66 Q78 56 72 58 M116 66 Q122 56 128 58', fill: 'none', stroke: '#E8D9B8', 'stroke-width': 4, 'stroke-linecap': 'round' }, head);
    el('ellipse', { cx: 76, cy: 74, rx: 9, ry: 5, fill: '#F6F3EE', stroke: '#B9B4AD', 'stroke-width': 1.4, transform: 'rotate(-20 76 74)' }, head);
    el('ellipse', { cx: 124, cy: 74, rx: 9, ry: 5, fill: '#F6F3EE', stroke: '#B9B4AD', 'stroke-width': 1.4, transform: 'rotate(20 124 74)' }, head);
    el('ellipse', { cx: 100, cy: 80, rx: 22, ry: 20, fill: '#F6F3EE', stroke: '#B9B4AD', 'stroke-width': 1.8 }, head);
    el('path', { d: 'M88 66 Q96 60 100 68 Q94 74 88 66 Z', fill: '#3B2F4A' }, head);
    el('ellipse', { cx: 100, cy: 92, rx: 16, ry: 10, fill: '#F7B9C2', stroke: '#E08C9A', 'stroke-width': 1.4 }, head);
    el('ellipse', { cx: 94, cy: 92, rx: 2, ry: 2.6, fill: '#C9707E' }, head); el('ellipse', { cx: 106, cy: 92, rx: 2, ry: 2.6, fill: '#C9707E' }, head);
    [92, 108].forEach(x => { el('ellipse', { cx: x, cy: 78, rx: 2.6, ry: 3.2, fill: '#2B2230' }, head); el('circle', { cx: x + 0.9, cy: 76.8, r: 0.9, fill: '#FFFFFF' }, head); });
    el('ellipse', { cx: 86, cy: 84, rx: 3, ry: 1.6, fill: '#F49A94', opacity: 0.6 }, head); el('ellipse', { cx: 114, cy: 84, rx: 3, ry: 1.6, fill: '#F49A94', opacity: 0.6 }, head);
    const bell = el('g', {}, cow);
    el('path', { d: 'M90 100 Q100 106 110 100', fill: 'none', stroke: '#E5484D', 'stroke-width': 3 }, bell);
    el('path', { d: 'M94 104 L106 104 L108 118 L92 118 Z', fill: '#F2C14E', stroke: '#C99227', 'stroke-width': 1.4, 'stroke-linejoin': 'round' }, bell);
    el('circle', { cx: 100, cy: 119.5, r: 2, fill: '#C99227' }, bell);
    const ding = [-1, 1].map(s => el('path', { d: `M${100 + s * 14} 108 l${s * 5} -2 M${100 + s * 15} 114 l${s * 6} 1`, stroke: '#C99227', 'stroke-width': 1.8, 'stroke-linecap': 'round', opacity: 0 }, g));
    return c => { const a = Math.sin(c * 4) * 14; bell.setAttribute('transform', `rotate(${f1(a)} 100 100)`);
      const on = Math.abs(Math.sin(c * 4)) > 0.85 ? 1 : 0; ding.forEach(d => d.setAttribute('opacity', on));
      head.setAttribute('transform', `rotate(${f1(Math.sin(c * 1.2) * 3)} 100 96)`);
      tail.setAttribute('transform', `translate(142 108) rotate(${f1(Math.sin(c * 2.5) * 18)})`); };
  },
  'see': g => {
    const P = el('g', { transform: 'translate(100 120) scale(0.82 0.55)' }, g);
    peaks(P, DAY);
    el('ellipse', { cx: 100, cy: 148, rx: 96, ry: 40, fill: '#7FC4EA' }, g);
    const refl = el('g', { transform: 'translate(100 122) scale(0.82 -0.3)', opacity: 0.25 }, g);
    peaks(refl, DAY);
    const rip = [0, 1, 2].map(i => el('path', { d: 'M-10 0 Q0 -3 10 0', fill: 'none', stroke: '#FFFFFF', 'stroke-width': 1.6, 'stroke-linecap': 'round', opacity: 0.7 }, g));
    const boat = el('g', {}, g);
    const kn = buddy(el('g', { transform: 'translate(0 -4) scale(0.78)' }, boat), 'float');
    el('path', { d: 'M-26 -6 L26 -6 Q22 8 0 8 Q-22 8 -26 -6 Z', fill: '#C68B59', stroke: '#8A5A3B', 'stroke-width': 1.8, 'stroke-linejoin': 'round' }, boat);
    el('path', { d: 'M-24 -2 L24 -2', stroke: '#8A5A3B', 'stroke-width': 1.2 }, boat);
    el('path', { d: 'M16 -8 L34 10', stroke: '#8A5A3B', 'stroke-width': 2.4, 'stroke-linecap': 'round' }, boat);
    return c => { kn(c); boat.setAttribute('transform', `translate(${f1(104 + Math.sin(c * 0.6) * 18)} ${f1(150 + Math.sin(c * 2) * 1.5)}) rotate(${f1(Math.sin(c * 1.7) * 3)})`);
      rip.forEach((r, i) => { const k = (c * 0.4 + i / 3) % 1; r.setAttribute('transform', `translate(${[50, 150, 80][i]} ${[138, 150, 172][i]}) scale(${(1 + k).toFixed(2)})`); r.setAttribute('opacity', (0.7 * (1 - k)).toFixed(2)); }); };
  },
  'huette': g => {
    el('path', { d: 'M0 156 Q100 146 200 156 L200 200 L0 200 Z', fill: '#A8D08D' }, g);
    const smoke = [0, 1, 2].map(() => el('circle', { r: 5, fill: '#FFFFFF', opacity: 0 }, g));
    el('rect', { x: 128, y: 40, width: 12, height: 26, fill: '#9AA4B1' }, g);
    el('rect', { x: 50, y: 92, width: 100, height: 64, fill: '#C68B59', stroke: '#8A5A3B', 'stroke-width': 2 }, g);
    for (let y = 100; y < 156; y += 9) el('line', { x1: 50, y1: y, x2: 150, y2: y, stroke: '#A8703F', 'stroke-width': 1.4 }, g);
    el('path', { d: 'M36 96 L100 52 L164 96 L156 100 L100 62 L44 100 Z', fill: '#8A5A3B', stroke: '#6E4528', 'stroke-width': 2, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M56 96 L100 66 L144 96 Z', fill: '#E0B07E' }, g);
    el('rect', { x: 88, y: 116, width: 24, height: 40, rx: 2, fill: '#8A5A3B', stroke: '#6E4528', 'stroke-width': 1.6 }, g);
    el('path', { d: 'M100 128 C98 124 93 126 95 130 L100 134 L105 130 C107 126 102 124 100 128 Z', fill: '#F3D2A6' }, g);
    const win = [[60, 108], [120, 108]].map(([x, y]) => { el('rect', { x, y, width: 20, height: 18, fill: '#DDF1FB', stroke: '#6E4528', 'stroke-width': 1.8 }, g);
      el('path', { d: `M${x + 10} ${y} L${x + 10} ${y + 18}`, stroke: '#6E4528', 'stroke-width': 1.4 }, g);
      el('rect', { x: x - 2, y: y + 18, width: 24, height: 5, rx: 1.5, fill: '#8A5A3B' }, g);
      return [0, 1, 2].map(i => el('circle', { cx: x + 3 + i * 7, cy: y + 17, r: 2.8, fill: '#E5484D' }, g)); });
    el('rect', { x: 84, y: 76, width: 32, height: 11, rx: 2.5, fill: '#F3D2A6', stroke: '#8A5A3B', 'stroke-width': 1.2 }, g);
    txt(g, 100, 84.5, 'Hütte', 7.5, '#6E4528');
    return c => { smoke.forEach((s, i) => { const k = (c * 0.35 + i / 3) % 1; s.setAttribute('cx', f1(134 + Math.sin(k * 5) * 4 + k * 10)); s.setAttribute('cy', f1(36 - k * 34)); s.setAttribute('r', f1(4 + k * 6)); s.setAttribute('opacity', (0.85 * Math.sin(Math.PI * k)).toFixed(2)); });
      win.flat().forEach((f, i) => f.setAttribute('r', f1(2.8 + Math.sin(c * 3 + i) * 0.35))); };
  },
  'seilbahn': g => {
    const P = el('g', { transform: 'translate(100 176) scale(0.7)' }, g);
    peaks(P, DAY);
    el('path', { d: 'M150 22 L160 22 L166 96 L144 96 Z', fill: '#9AA4B1', stroke: '#6B7180', 'stroke-width': 1.6 }, g);
    el('path', { d: 'M140 22 L170 22', stroke: '#6B7180', 'stroke-width': 3, 'stroke-linecap': 'round' }, g);
    el('line', { x1: -10, y1: 104, x2: 210, y2: 14, stroke: '#3B2F4A', 'stroke-width': 1.8 }, g);
    const cab = el('g', {}, g);
    el('path', { d: 'M0 -26 L0 -12', stroke: '#3B2F4A', 'stroke-width': 2 }, cab);
    el('circle', { cx: 0, cy: -27, r: 3, fill: '#3B2F4A' }, cab);
    el('rect', { x: -22, y: -12, width: 44, height: 36, rx: 7, fill: '#E5484D', stroke: '#B8323A', 'stroke-width': 2 }, cab);
    el('rect', { x: -17, y: -6, width: 34, height: 15, rx: 3, fill: '#DDF1FB' }, cab);
    const kn = buddy(el('g', { transform: 'translate(-1 14) scale(0.62)' }, cab), 'happy');
    el('rect', { x: -22, y: 9, width: 44, height: 15, rx: 4, fill: '#E5484D' }, cab);
    el('rect', { x: -22, y: 9, width: 44, height: 3, fill: '#FFFFFF', opacity: 0.6 }, cab);
    return c => { kn(c); const u = loop(c, 7.4), x = -20 + u * 240, y = 108 - (x + 10) * (90 / 220) + 26;
      cab.setAttribute('transform', `translate(${f1(x)} ${f1(y)}) rotate(${f1(Math.sin(c * 2) * 3)} 0 -26)`); };
  },
  'tal': g => {
    el('path', { d: 'M-10 40 L40 30 L100 132 L160 26 L210 36 L210 200 L-10 200 Z', fill: '#EEE7DD', stroke: '#C9BBA8', 'stroke-width': 2, 'stroke-linejoin': 'round' }, g);
    el('path', { d: 'M28 34 L40 30 L46 46 Z M154 36 L160 26 L170 40 Z', fill: '#FFFFFF' }, g);
    el('path', { d: 'M-10 96 Q40 92 100 150 Q160 92 210 96 L210 200 L-10 200 Z', fill: '#A8D08D' }, g);
    el('path', { d: 'M-10 150 Q60 140 100 160 Q140 140 210 150 L210 200 L-10 200 Z', fill: '#8CC06E' }, g);
    el('path', { d: 'M100 156 Q90 172 104 186 Q114 196 108 210', fill: 'none', stroke: '#7FC4EA', 'stroke-width': 6, 'stroke-linecap': 'round' }, g);
    fir(g, 34, 112, 0.8); fir(g, 166, 110, 0.8, '#4E9E68'); fir(g, 52, 124, 0.6);
    const house = (x, y, roof) => { el('rect', { x: x - 8, y: y - 10, width: 16, height: 11, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1 }, g);
      el('path', { d: `M${x - 10} ${y - 9} L${x} ${y - 17} L${x + 10} ${y - 9} Z`, fill: roof }, g); el('rect', { x: x - 2, y: y - 6, width: 4, height: 4, fill: '#7FB6E0' }, g); };
    house(80, 158, '#C2553A'); house(122, 160, '#C2553A'); house(140, 150, '#B8323A'); house(66, 150, '#B8323A');
    el('rect', { x: 96, y: 136, width: 12, height: 26, fill: '#FFFFFF', stroke: '#C9CED6', 'stroke-width': 1 }, g);
    el('path', { d: 'M96 136 Q102 120 108 136 Z', fill: '#5E9E45' }, g);
    el('ellipse', { cx: 102, cy: 128, rx: 4, ry: 5, fill: '#5E9E45' }, g);
    el('path', { d: 'M102 123 L102 115', stroke: '#C99227', 'stroke-width': 1.4 }, g);
    el('circle', { cx: 102, cy: 146, r: 2.6, fill: '#F2C14E' }, g);
    const smoke = [0, 1, 2].map(() => el('circle', { r: 2.6, fill: '#FFFFFF', opacity: 0 }, g));
    return c => smoke.forEach((s, i) => { const k = (c * 0.35 + i / 3) % 1; s.setAttribute('cx', f1(142 + Math.sin(k * 6) * 2 + k * 6)); s.setAttribute('cy', f1(134 - k * 26)); s.setAttribute('r', f1(2 + k * 4)); s.setAttribute('opacity', (0.9 * Math.sin(Math.PI * k)).toFixed(2)); });
  },
});

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
