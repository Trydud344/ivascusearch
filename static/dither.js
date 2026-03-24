(function () {
  'use strict';

  var canvas = document.getElementById('dither-canvas');
  if (!canvas) return;

  var gl = canvas.getContext('webgl2');
  if (!gl) {
    console.warn('WebGL2 not supported, dither effect disabled');
    return;
  }

  // --- Gruvbox-tuned config ---
  var waveSpeed = 0.05;
  var waveFrequency = 3.0;
  var waveAmplitude = 0.3;
  var waveColor = [0.843, 0.600, 0.129]; // Gruvbox yellow1 #d79921
  var colorNum = 4.0;
  var pixelSize = 2.0;
  var enableMouseInteraction = 1;
  var mouseRadius = 0.3;

  // --- Shaders ---
  var vertexShaderSrc = [
    '#version 300 es',
    'in vec2 a_position;',
    'void main() {',
    '  gl_Position = vec4(a_position, 0.0, 1.0);',
    '}'
  ].join('\n');

  var fragmentShaderSrc = [
    '#version 300 es',
    'precision highp float;',
    '',
    'uniform vec2 u_resolution;',
    'uniform float u_time;',
    'uniform float u_waveSpeed;',
    'uniform float u_waveFrequency;',
    'uniform float u_waveAmplitude;',
    'uniform vec3 u_waveColor;',
    'uniform vec2 u_mousePos;',
    'uniform int u_enableMouseInteraction;',
    'uniform float u_mouseRadius;',
    'uniform float u_colorNum;',
    'uniform float u_pixelSize;',
    '',
    'out vec4 fragColor;',
    '',
    '// --- Perlin noise helpers ---',
    'vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }',
    'vec4 permute(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }',
    'vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }',
    'vec2 fade(vec2 t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }',
    '',
    'float cnoise(vec2 P) {',
    '  vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);',
    '  vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);',
    '  Pi = mod289(Pi);',
    '  vec4 ix = Pi.xzxz;',
    '  vec4 iy = Pi.yyww;',
    '  vec4 fx = Pf.xzxz;',
    '  vec4 fy = Pf.yyww;',
    '  vec4 i = permute(permute(ix) + iy);',
    '  vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0;',
    '  vec4 gy = abs(gx) - 0.5;',
    '  vec4 tx = floor(gx + 0.5);',
    '  gx = gx - tx;',
    '  vec2 g00 = vec2(gx.x, gy.x);',
    '  vec2 g10 = vec2(gx.y, gy.y);',
    '  vec2 g01 = vec2(gx.z, gy.z);',
    '  vec2 g11 = vec2(gx.w, gy.w);',
    '  vec4 norm = taylorInvSqrt(vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11)));',
    '  g00 *= norm.x; g01 *= norm.y; g10 *= norm.z; g11 *= norm.w;',
    '  float n00 = dot(g00, vec2(fx.x, fy.x));',
    '  float n10 = dot(g10, vec2(fx.y, fy.y));',
    '  float n01 = dot(g01, vec2(fx.z, fy.z));',
    '  float n11 = dot(g11, vec2(fx.w, fy.w));',
    '  vec2 fade_xy = fade(Pf.xy);',
    '  vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);',
    '  return 2.3 * mix(n_x.x, n_x.y, fade_xy.y);',
    '}',
    '',
    '// --- FBM ---',
    'const int OCTAVES = 4;',
    'float fbm(vec2 p) {',
    '  float value = 0.0;',
    '  float amp = 1.0;',
    '  float freq = u_waveFrequency;',
    '  for (int i = 0; i < OCTAVES; i++) {',
    '    value += amp * abs(cnoise(p));',
    '    p *= freq;',
    '    amp *= u_waveAmplitude;',
    '  }',
    '  return value;',
    '}',
    '',
    'float pattern(vec2 p) {',
    '  vec2 p2 = p - u_time * u_waveSpeed;',
    '  return fbm(p + fbm(p2));',
    '}',
    '',
    '// --- Bayer 8x8 dither matrix ---',
    'const float bayerMatrix8x8[64] = float[64](',
    '   0.0/64.0, 48.0/64.0, 12.0/64.0, 60.0/64.0,  3.0/64.0, 51.0/64.0, 15.0/64.0, 63.0/64.0,',
    '  32.0/64.0, 16.0/64.0, 44.0/64.0, 28.0/64.0, 35.0/64.0, 19.0/64.0, 47.0/64.0, 31.0/64.0,',
    '   8.0/64.0, 56.0/64.0,  4.0/64.0, 52.0/64.0, 11.0/64.0, 59.0/64.0,  7.0/64.0, 55.0/64.0,',
    '  40.0/64.0, 24.0/64.0, 36.0/64.0, 20.0/64.0, 43.0/64.0, 27.0/64.0, 39.0/64.0, 23.0/64.0,',
    '   2.0/64.0, 50.0/64.0, 14.0/64.0, 62.0/64.0,  1.0/64.0, 49.0/64.0, 13.0/64.0, 61.0/64.0,',
    '  34.0/64.0, 18.0/64.0, 46.0/64.0, 30.0/64.0, 33.0/64.0, 17.0/64.0, 45.0/64.0, 29.0/64.0,',
    '  10.0/64.0, 58.0/64.0,  6.0/64.0, 54.0/64.0,  9.0/64.0, 57.0/64.0,  5.0/64.0, 53.0/64.0,',
    '  42.0/64.0, 26.0/64.0, 38.0/64.0, 22.0/64.0, 41.0/64.0, 25.0/64.0, 37.0/64.0, 21.0/64.0',
    ');',
    '',
    'void main() {',
    '  // Snap to pixel grid for the pixelation look',
    '  vec2 pixelatedCoord = floor(gl_FragCoord.xy / u_pixelSize) * u_pixelSize + u_pixelSize * 0.5;',
    '',
    '  // Compute wave UV at the snapped pixel position',
    '  vec2 uv = pixelatedCoord / u_resolution;',
    '  uv -= 0.5;',
    '  uv.x *= u_resolution.x / u_resolution.y;',
    '',
    '  float f = pattern(uv);',
    '',
    '  // Mouse interaction',
    '  if (u_enableMouseInteraction == 1) {',
    '    vec2 mouseNDC = (u_mousePos / u_resolution - 0.5) * vec2(1.0, -1.0);',
    '    mouseNDC.x *= u_resolution.x / u_resolution.y;',
    '    float dist = length(uv - mouseNDC);',
    '    float effect = 1.0 - smoothstep(0.0, u_mouseRadius, dist);',
    '    f -= 0.5 * effect;',
    '  }',
    '',
    '  vec3 col = mix(vec3(0.0), u_waveColor, f);',
    '',
    '  // Bayer dithering',
    '  vec2 scaledCoord = floor(gl_FragCoord.xy / u_pixelSize);',
    '  int x = int(mod(scaledCoord.x, 8.0));',
    '  int y = int(mod(scaledCoord.y, 8.0));',
    '  float threshold = bayerMatrix8x8[y * 8 + x] - 0.25;',
    '  float stepSize = 1.0 / (u_colorNum - 1.0);',
    '  col += threshold * stepSize;',
    '  float bias = 0.2;',
    '  col = clamp(col - bias, 0.0, 1.0);',
    '  col = floor(col * (u_colorNum - 1.0) + 0.5) / (u_colorNum - 1.0);',
    '',
    '  fragColor = vec4(col, 1.0);',
    '}'
  ].join('\n');

  // --- Compile & link ---
  function compileShader(type, source) {
    var s = gl.createShader(type);
    gl.shaderSource(s, source);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      console.error('Shader compile error:', gl.getShaderInfoLog(s));
      gl.deleteShader(s);
      return null;
    }
    return s;
  }

  function createProgram(vSrc, fSrc) {
    var vs = compileShader(gl.VERTEX_SHADER, vSrc);
    var fs = compileShader(gl.FRAGMENT_SHADER, fSrc);
    if (!vs || !fs) return null;
    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      console.error('Program link error:', gl.getProgramInfoLog(prog));
      return null;
    }
    return prog;
  }

  var program = createProgram(vertexShaderSrc, fragmentShaderSrc);
  if (!program) return;

  gl.useProgram(program);

  // --- Fullscreen quad geometry ---
  var positions = new Float32Array([
    -1, -1,  1, -1,  -1,  1,
    -1,  1,  1, -1,   1,  1
  ]);

  var vao = gl.createVertexArray();
  gl.bindVertexArray(vao);

  var buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);

  var posLoc = gl.getAttribLocation(program, 'a_position');
  gl.enableVertexAttribArray(posLoc);
  gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);

  // --- Uniform locations ---
  var uResolution = gl.getUniformLocation(program, 'u_resolution');
  var uTime = gl.getUniformLocation(program, 'u_time');
  var uWaveSpeed = gl.getUniformLocation(program, 'u_waveSpeed');
  var uWaveFrequency = gl.getUniformLocation(program, 'u_waveFrequency');
  var uWaveAmplitude = gl.getUniformLocation(program, 'u_waveAmplitude');
  var uWaveColor = gl.getUniformLocation(program, 'u_waveColor');
  var uMousePos = gl.getUniformLocation(program, 'u_mousePos');
  var uEnableMouseInteraction = gl.getUniformLocation(program, 'u_enableMouseInteraction');
  var uMouseRadius = gl.getUniformLocation(program, 'u_mouseRadius');
  var uColorNum = gl.getUniformLocation(program, 'u_colorNum');
  var uPixelSize = gl.getUniformLocation(program, 'u_pixelSize');

  // Set static uniforms
  gl.uniform1f(uWaveSpeed, waveSpeed);
  gl.uniform1f(uWaveFrequency, waveFrequency);
  gl.uniform1f(uWaveAmplitude, waveAmplitude);
  gl.uniform3fv(uWaveColor, waveColor);
  gl.uniform1i(uEnableMouseInteraction, enableMouseInteraction);
  gl.uniform1f(uMouseRadius, mouseRadius);
  gl.uniform1f(uColorNum, colorNum);
  gl.uniform1f(uPixelSize, pixelSize);

  // --- Mouse tracking (listen on document so content doesn't block it) ---
  var mouseX = 0, mouseY = 0;
  document.addEventListener('mousemove', function (e) {
    var rect = canvas.getBoundingClientRect();
    mouseX = e.clientX - rect.left;
    mouseY = e.clientY - rect.top;
  });

  // --- Resize (render at 1x DPI for the retro pixelated look) ---
  function resize() {
    var w = canvas.clientWidth;
    var h = canvas.clientHeight;
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      gl.viewport(0, 0, w, h);
    }
  }

  // --- Animation loop ---
  var startTime = performance.now();

  function render() {
    resize();

    var elapsed = (performance.now() - startTime) / 1000.0;
    gl.uniform2f(uResolution, canvas.width, canvas.height);
    gl.uniform1f(uTime, elapsed);
    gl.uniform2f(uMousePos, mouseX, mouseY);

    gl.drawArrays(gl.TRIANGLES, 0, 6);
    requestAnimationFrame(render);
  }

  render();
})();
