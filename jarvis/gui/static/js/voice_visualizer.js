/**
 * J.A.R.V.I.S. Cinematic Voice Visualizer
 * Replaces the 3D particle sphere with a responsive, cinematic voice intelligence visualizer.
 * Supports 5 states: STANDBY, LISTENING, THINKING, SPEAKING, EXECUTING.
 * Reactive to actual microphone input via Web Audio API or speech synthesis playback.
 */

class VoiceVisualizer {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    if (!this.canvas) return;
    this.ctx = this.canvas.getContext("2d");

    // Current assistant state
    this.state = "STANDBY"; // "STANDBY" | "LISTENING" | "THINKING" | "SPEAKING" | "EXECUTING"
    this.stateLabel = "STANDBY";
    this.subLabel = "AWAITING VOICE INPUT";

    // Audio Analysis State
    this.audioCtx = null;
    this.analyser = null;
    this.dataArray = null;
    this.micStream = null;
    this.isMicActive = false;

    // Animation & Waveform State
    this.time = 0;
    this.corePulse = 1.0;
    this.targetPulse = 1.0;
    this.ringExpansion = 1.0;
    this.targetExpansion = 1.0;
    this.scanAngle = 0;

    // Color definitions matching specification
    this.colors = {
      goldPrimary: "#E8D84F",
      goldSecondary: "#B9A83D",
      goldGlow: "rgba(232, 216, 79, 0.45)",
      goldFaint: "rgba(232, 216, 79, 0.12)",
      greenHealthy: "#8FBF67",
      greenGlow: "rgba(143, 191, 103, 0.45)",
      redAlert: "#C95B4B",
      darkBg: "#070805"
    };

    this.init();
  }

  init() {
    this.resize();
    window.addEventListener("resize", () => this.resize());
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  resize() {
    if (!this.canvas || !this.canvas.parentElement) return;
    const rect = this.canvas.parentElement.getBoundingClientRect();
    this.canvas.width = rect.width;
    this.canvas.height = rect.height;
  }

  setState(newState, customSub = null) {
    this.state = newState.toUpperCase();
    this.time = 0;

    if (this.state === "STANDBY") {
      this.stateLabel = "S T A N D B Y";
      this.subLabel = customSub || "AWAITING VOICE INPUT";
      this.targetExpansion = 1.0;
    } else if (this.state === "LISTENING") {
      this.stateLabel = "L I S T E N I N G";
      this.subLabel = customSub || "I'M LISTENING... SPEAK NOW";
      this.targetExpansion = 1.35;
    } else if (this.state === "THINKING") {
      this.stateLabel = "T H I N K I N G";
      this.subLabel = customSub || "PROCESSING TELEMETRY & QUERY";
      this.targetExpansion = 1.15;
    } else if (this.state === "SPEAKING") {
      this.stateLabel = "S P E A K I N G";
      this.subLabel = customSub || "TRANSMITTING VOICE RESPONSE";
      this.targetExpansion = 1.25;
    } else if (this.state === "EXECUTING") {
      this.stateLabel = "E X E C U T I N G";
      this.subLabel = customSub || "DISPATCHING KERNEL ACTION";
      this.targetExpansion = 1.2;
    }

    // Update text elements if present in DOM
    const stateEl = document.getElementById("visualizer-state-text");
    const subEl = document.getElementById("visualizer-sub-text");
    if (stateEl) stateEl.innerText = this.stateLabel;
    if (subEl) subEl.innerText = this.subLabel;
  }

  // Connect actual microphone stream for reactive LISTENING state
  async connectMicrophone() {
    try {
      if (!this.audioCtx) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        this.audioCtx = new AudioContext();
      }

      if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
        this.micStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
        const source = this.audioCtx.createMediaStreamSource(this.micStream);
        this.analyser = this.audioCtx.createAnalyser();
        this.analyser.fftSize = 64;
        source.connect(this.analyser);
        this.dataArray = new Uint8Array(this.analyser.frequencyBinCount);
        this.isMicActive = true;
        return true;
      }
    } catch (e) {
      console.warn("Microphone access unavailable or denied; using procedural audio wave.", e);
      this.isMicActive = false;
      return false;
    }
    return false;
  }

  disconnectMicrophone() {
    if (this.micStream) {
      this.micStream.getTracks().forEach(t => t.stop());
      this.micStream = null;
    }
    this.isMicActive = false;
  }

  getAudioAmplitude() {
    if (this.isMicActive && this.analyser && this.dataArray) {
      this.analyser.getByteFrequencyData(this.dataArray);
      let sum = 0;
      for (let i = 0; i < this.dataArray.length; i++) {
        sum += this.dataArray[i];
      }
      return sum / (this.dataArray.length * 255); // 0.0 to 1.0
    }
    return null;
  }

  animate() {
    requestAnimationFrame(this.animate);
    this.time += 0.03;

    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;
    if (!w || !h) return;

    ctx.clearRect(0, 0, w, h);

    const centerX = w / 2;
    const centerY = h * 0.42; // Center above the labels

    // Smooth expansion dampening
    this.ringExpansion += (this.targetExpansion - this.ringExpansion) * 0.08;

    // Determine current audio level
    let audioAmp = this.getAudioAmplitude();

    if (audioAmp === null) {
      // Procedural audio amplitude based on state
      if (this.state === "STANDBY") {
        audioAmp = 0.04 + Math.sin(this.time * 1.5) * 0.02;
      } else if (this.state === "LISTENING") {
        audioAmp = 0.2 + Math.abs(Math.sin(this.time * 4)) * 0.25;
      } else if (this.state === "THINKING") {
        audioAmp = 0.15 + Math.abs(Math.sin(this.time * 6)) * 0.15;
      } else if (this.state === "SPEAKING") {
        audioAmp = 0.35 + Math.abs(Math.sin(this.time * 5.5) * Math.cos(this.time * 3)) * 0.45;
      } else if (this.state === "EXECUTING") {
        audioAmp = 0.25 + Math.sin(this.time * 8) * 0.15;
      }
    }

    // Active color choice based on state
    let activeColor = this.colors.goldPrimary;
    let glowColor = this.colors.goldGlow;
    if (this.state === "LISTENING") {
      activeColor = this.colors.greenHealthy;
      glowColor = this.colors.greenGlow;
    } else if (this.state === "THINKING") {
      activeColor = this.colors.goldSecondary;
    }

    // 1. Draw Outer Concentric Tactical Rings
    this.drawConcentricRings(ctx, centerX, centerY, audioAmp, activeColor);

    // 2. Draw 360-Degree Radial Audio Equalizer Bars
    this.drawRadialEqualizer(ctx, centerX, centerY, audioAmp, activeColor);

    // 3. Draw Scanning Ray (for THINKING / EXECUTING state)
    if (this.state === "THINKING" || this.state === "EXECUTING") {
      this.drawScanRay(ctx, centerX, centerY, activeColor);
    }

    // 4. Draw Central Glowing Core
    this.drawCentralCore(ctx, centerX, centerY, audioAmp, activeColor, glowColor);

    // 5. Draw Tactical Reticle Accents & Crosshairs around Core
    this.drawTacticalReticle(ctx, centerX, centerY, activeColor);

    // 6. Draw Horizontal Equalizer Strip & State Typography below Core
    this.drawStateTypographyAndWave(ctx, centerX, centerY, audioAmp, activeColor);
  }

  drawConcentricRings(ctx, cx, cy, amp, color) {
    const baseRadius = 110 * this.ringExpansion;

    // Outer faint dashed ring
    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, baseRadius * 1.35, 0, Math.PI * 2);
    ctx.strokeStyle = "rgba(185, 168, 61, 0.18)";
    ctx.setLineDash([4, 8]);
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.restore();

    // Secondary solid ring with breathing amplitude
    const r2 = baseRadius * 1.15 + amp * 12;
    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, r2, 0, Math.PI * 2);
    ctx.strokeStyle = "rgba(232, 216, 79, 0.25)";
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.restore();

    // Inner orbiting segmented arc
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(this.time * 0.4);
    ctx.beginPath();
    ctx.arc(0, 0, baseRadius * 0.95, 0, Math.PI * 0.75);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(0, 0, baseRadius * 0.95, Math.PI, Math.PI * 1.75);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.restore();
  }

  drawRadialEqualizer(ctx, cx, cy, amp, color) {
    const barCount = 48;
    const baseRadius = 75 * this.ringExpansion;
    const maxBarHeight = 32 * (0.4 + amp * 1.4);

    ctx.save();
    ctx.translate(cx, cy);

    for (let i = 0; i < barCount; i++) {
      const angle = (i / barCount) * Math.PI * 2;
      const barNoise = Math.sin(this.time * 3 + i * 0.5) * Math.cos(this.time * 2 + i * 0.2);
      const barLen = Math.max(3, Math.abs(barNoise) * maxBarHeight + (this.state === "STANDBY" ? 2 : 5));

      const x1 = Math.cos(angle) * baseRadius;
      const y1 = Math.sin(angle) * baseRadius;
      const x2 = Math.cos(angle) * (baseRadius + barLen);
      const y2 = Math.sin(angle) * (baseRadius + barLen);

      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle = i % 2 === 0 ? color : "rgba(185, 168, 61, 0.45)";
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }
    ctx.restore();
  }

  drawScanRay(ctx, cx, cy, color) {
    this.scanAngle += 0.045;
    const rayLength = 140 * this.ringExpansion;

    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(this.scanAngle);

    const grad = ctx.createLinearGradient(0, 0, rayLength, 0);
    grad.addColorStop(0, "rgba(232, 216, 79, 0)");
    grad.addColorStop(0.8, color);
    grad.addColorStop(1, "rgba(232, 216, 79, 0)");

    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(rayLength, 0);
    ctx.strokeStyle = grad;
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.restore();
  }

  drawCentralCore(ctx, cx, cy, amp, color, glow) {
    const coreRadius = (32 + amp * 18) * (this.state === "STANDBY" ? 0.9 : 1.0);

    // Glowing core bloom
    const glowGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, coreRadius * 2.2);
    glowGrad.addColorStop(0, glow);
    glowGrad.addColorStop(0.5, "rgba(232, 216, 79, 0.08)");
    glowGrad.addColorStop(1, "rgba(7, 8, 5, 0)");

    ctx.save();
    ctx.fillStyle = glowGrad;
    ctx.beginPath();
    ctx.arc(cx, cy, coreRadius * 2.2, 0, Math.PI * 2);
    ctx.fill();

    // Central circular membrane
    const coreGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, coreRadius);
    coreGrad.addColorStop(0, "#FFFFFF");
    coreGrad.addColorStop(0.3, color);
    coreGrad.addColorStop(0.85, "rgba(185, 168, 61, 0.65)");
    coreGrad.addColorStop(1, "rgba(7, 8, 5, 0.9)");

    ctx.fillStyle = coreGrad;
    ctx.beginPath();
    ctx.arc(cx, cy, coreRadius, 0, Math.PI * 2);
    ctx.fill();

    // Core outline
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.restore();
  }

  drawTacticalReticle(ctx, cx, cy, color) {
    const r = 165 * this.ringExpansion;
    const len = 12;

    ctx.save();
    ctx.strokeStyle = "rgba(185, 168, 61, 0.35)";
    ctx.lineWidth = 1;

    // Corner brackets around the core
    const corners = [
      { x: cx - r, y: cy - r, dx: 1, dy: 1 },
      { x: cx + r, y: cy - r, dx: -1, dy: 1 },
      { x: cx - r, y: cy + r, dx: 1, dy: -1 },
      { x: cx + r, y: cy + r, dx: -1, dy: -1 }
    ];

    corners.forEach(c => {
      ctx.beginPath();
      ctx.moveTo(c.x, c.y + c.dy * len);
      ctx.lineTo(c.x, c.y);
      ctx.lineTo(c.x + c.dx * len, c.y);
      ctx.stroke();
    });

    // Small cardinal ticks
    ctx.beginPath();
    ctx.moveTo(cx - r - 4, cy); ctx.lineTo(cx - r + 4, cy);
    ctx.moveTo(cx + r - 4, cy); ctx.lineTo(cx + r + 4, cy);
    ctx.moveTo(cx, cy - r - 4); ctx.lineTo(cx, cy - r + 4);
    ctx.moveTo(cx, cy + r - 4); ctx.lineTo(cx, cy + r + 4);
    ctx.stroke();

    ctx.restore();
  }

  drawStateTypographyAndWave(ctx, cx, cy, amp, color) {
    const w = this.canvas.width;
    const textY = cy + 185;

    // Horizontal audio waveform strip right above the label
    const waveWidth = 140;
    const waveStartX = cx - waveWidth / 2;
    const barCount = 20;
    const barW = 4;
    const spacing = 7;

    ctx.save();
    for (let i = 0; i < barCount; i++) {
      const bx = waveStartX + i * spacing;
      const normalized = i / barCount;
      const envelope = Math.sin(normalized * Math.PI);
      const h = Math.max(3, Math.abs(Math.sin(this.time * 4 + i * 0.4)) * 18 * envelope * (0.3 + amp * 1.2));

      ctx.fillStyle = i % 2 === 0 ? color : this.colors.goldSecondary;
      ctx.fillRect(bx, textY - 32 - h / 2, barW, h);
    }

    // Main State Label: S P E A K I N G / S T A N D B Y
    ctx.font = "800 24px 'Orbitron', 'Rajdhani', sans-serif";
    ctx.textAlign = "center";
    ctx.fillStyle = color;
    ctx.shadowColor = this.colors.goldGlow;
    ctx.shadowBlur = 10;
    ctx.fillText(this.stateLabel, cx, textY);

    // Subtitle label: AWAITING VOICE INPUT
    ctx.shadowBlur = 0;
    ctx.font = "600 11px 'JetBrains Mono', monospace";
    ctx.fillStyle = this.colors.goldSecondary;
    ctx.letterSpacing = "2px";
    ctx.fillText(this.subLabel.toUpperCase(), cx, textY + 22);

    ctx.restore();
  }
}

// Global handle
window.VoiceVisualizer = VoiceVisualizer;

