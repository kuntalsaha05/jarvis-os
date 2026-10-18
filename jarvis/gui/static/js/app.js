/**
 * J.A.R.V.I.S. Voice Assistant & Command Interface Controller.
 * Manages the responsive voice visualizer, Web Speech API (Voice Input & TTS Output),
 * Web Audio API synthesizer, WebSocket telemetry streaming, and tactical modules.
 */

// Global Handles
let voiceVisualizer = null;
let audioCtx = null;
let soundEnabled = true;
let speechRecognizer = null;
let isListening = false;
let currentUtterance = null;
let telemetrySocket = null;

// ========================================================
// 1. Digital Clock & Top Status Bar
// ========================================================
function updateDigitalClock() {
  const clockEl = document.getElementById("digital-clock-display");
  if (!clockEl) return;
  const now = new Date();
  const h = String(now.getHours()).padStart(2, '0');
  const m = String(now.getMinutes()).padStart(2, '0');
  const s = String(now.getSeconds()).padStart(2, '0');
  clockEl.innerText = `${h}:${m}:${s}`;
}

// ========================================================
// 2. Tactical Audio Synthesizer (SFX)
// ========================================================
function initAudio() {
  if (!audioCtx) {
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    if (AudioContext) audioCtx = new AudioContext();
  }
}

function playSound(type) {
  if (!soundEnabled) return;
  try {
    initAudio();
    if (!audioCtx) return;
    const now = audioCtx.currentTime;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);

    if (type === "click") {
      osc.type = "sine";
      osc.frequency.setValueAtTime(800, now);
      osc.frequency.exponentialRampToValueAtTime(300, now + 0.04);
      gain.gain.setValueAtTime(0.06, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.04);
      osc.start(now);
      osc.stop(now + 0.04);
    } else if (type === "transmit") {
      osc.type = "triangle";
      osc.frequency.setValueAtTime(480, now);
      osc.frequency.linearRampToValueAtTime(960, now + 0.07);
      gain.gain.setValueAtTime(0.08, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.07);
      osc.start(now);
      osc.stop(now + 0.07);
    } else if (type === "ack") {
      osc.type = "sine";
      osc.frequency.setValueAtTime(587.33, now);
      osc.frequency.setValueAtTime(880, now + 0.06);
      gain.gain.setValueAtTime(0.08, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.12);
      osc.start(now);
      osc.stop(now + 0.12);
    } else if (type === "alert") {
      osc.type = "sawtooth";
      osc.frequency.setValueAtTime(320, now);
      osc.frequency.setValueAtTime(220, now + 0.09);
      gain.gain.setValueAtTime(0.09, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.18);
      osc.start(now);
      osc.stop(now + 0.18);
    }
  } catch (e) {}
}

function toggleSound() {
  soundEnabled = !soundEnabled;
  const btn = document.getElementById("sound-toggle-btn");
  if (btn) btn.innerText = soundEnabled ? "SFX: ON" : "SFX: OFF";
  if (soundEnabled) playSound("transmit");
}


// ========================================================
// 3. Mini Audio Waveform in Right Sidebar
// ========================================================
function initMiniWaveform() {
  const canvas = document.getElementById("voice-waveform-mini");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");

  let phase = 0;
  function draw() {
    canvas.width = canvas.clientWidth || 280;
    canvas.height = canvas.clientHeight || 28;
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    const isSpeaking = voiceVisualizer && voiceVisualizer.state === "SPEAKING";
    const isListening = voiceVisualizer && voiceVisualizer.state === "LISTENING";
    phase += (isSpeaking || isListening) ? 0.2 : 0.03;

    const bars = Math.floor(w / 6);
    const mid = h / 2;

    for (let i = 0; i < bars; i++) {
      const x = i * 6;
      const normalized = i / bars;
      const envelope = Math.sin(normalized * Math.PI);
      const amp = (isSpeaking ? 0.8 : (isListening ? 0.5 : 0.15));
      const val = Math.sin(phase + i * 0.4) * Math.cos(phase * 0.7 + i * 0.2);
      const barH = Math.max(2, Math.abs(val) * (h * 0.42) * envelope * amp + 2);

      ctx.fillStyle = isListening ? "#8FBF67" : "#E8D84F";
      ctx.fillRect(x, mid - barH, 3, barH * 2);
    }
    requestAnimationFrame(draw);
  }
  requestAnimationFrame(draw);
}


// ========================================================
// 4. Voice Input (Microphone & Speech Recognition)
// ========================================================
function initSpeechRecognition() {
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (SpeechRecognition) {
    speechRecognizer = new SpeechRecognition();
    speechRecognizer.continuous = false;
    speechRecognizer.interimResults = false;
    speechRecognizer.lang = 'en-US';

    speechRecognizer.onstart = () => {
      isListening = true;
      updateMicButtonState(true);
      if (voiceVisualizer) {
        voiceVisualizer.setState("LISTENING", "I'M LISTENING... SPEAK NOW");
        voiceVisualizer.connectMicrophone();
      }
      setVoiceMiniStatus("● LISTENING", "#8FBF67");
      appendActivityFeed("[VOX]", "Voice capture initiated");
      playSound("transmit");
    };

    speechRecognizer.onresult = (event) => {
      const transcript = event.results[0][0].transcript;
      const input = document.getElementById("voice-command-input");
      if (input) input.value = transcript;
      appendActivityFeed("[VOX]", `Heard: "${transcript}"`);

      // Automatically transition to thinking and process
      if (voiceVisualizer) voiceVisualizer.setState("THINKING", "PROCESSING SPEECH COMMAND");
      setVoiceMiniStatus("● THINKING", "#B9A83D");
      sendVoicePrompt(transcript);
    };

    speechRecognizer.onerror = (event) => {
      console.warn("Speech recognition error:", event.error);
      stopVoiceInput();
      if (voiceVisualizer) voiceVisualizer.setState("STANDBY");
      setVoiceMiniStatus("● STANDBY", "#E8D84F");
      playSound("alert");
    };

    speechRecognizer.onend = () => {
      stopVoiceInput();
    };
  }
}

async function toggleVoiceInput() {
  if (isListening) {
    stopVoiceInput();
  } else {
    startVoiceInput();
  }
}

function startVoiceInput() {
  if (speechRecognizer) {
    try {
      speechRecognizer.start();
    } catch (e) {
      console.warn("Speech recognition already running or errored", e);
    }
  } else {
    // Fallback if browser doesn't support Web Speech API
    const userPrompt = prompt("Speech recognition unavailable in this browser. Enter voice prompt manually:");
    if (userPrompt) sendVoicePrompt(userPrompt);
  }
}

function stopVoiceInput() {
  isListening = false;
  updateMicButtonState(false);
  if (speechRecognizer) {
    try { speechRecognizer.stop(); } catch (e) {}
  }
  if (voiceVisualizer) {
    voiceVisualizer.disconnectMicrophone();
    if (voiceVisualizer.state === "LISTENING") {
      voiceVisualizer.setState("STANDBY");
      setVoiceMiniStatus("● STANDBY", "#E8D84F");
    }
  }
}

function updateMicButtonState(active) {
  const btn = document.getElementById("btn-mic-voice");
  if (!btn) return;
  if (active) {
    btn.classList.add("recording");
    btn.innerHTML = `<span>🔴</span> LISTENING...`;
  } else {
    btn.classList.remove("recording");
    btn.innerHTML = `<span>🎙</span> VOICE INPUT`;
  }
}


// ========================================================
// 5. Speech Output (Text-To-Speech)
// ========================================================
function speakAssistantResponse(text) {
  if (!('speechSynthesis' in window)) {
    // If TTS unavailable, just show text and return to STANDBY after delay
    if (voiceVisualizer) {
      voiceVisualizer.setState("SPEAKING");
      setTimeout(() => voiceVisualizer.setState("STANDBY"), 3000);
    }
    return;
  }

  // Cancel any ongoing speech
  window.speechSynthesis.cancel();

  // Strip markdown, code blocks, or raw JSON for pleasant voice reading
  const cleanVoiceText = text
    .replace(/```[\s\S]*?```/g, "Code block output.")
    .replace(/`([^`]+)`/g, "$1")
    .replace(/[*_#]/g, "")
    .trim();

  currentUtterance = new SpeechSynthesisUtterance(cleanVoiceText);
  currentUtterance.rate = 1.05;
  currentUtterance.pitch = 0.95; // Slightly deeper, authoritative JARVIS tone

  // Select British / natural English voice if available
  const voices = window.speechSynthesis.getVoices();
  const preferredVoice = voices.find(v => v.lang.includes("en-GB") || v.name.includes("Daniel") || v.name.includes("George") || v.name.includes("Natural"));
  if (preferredVoice) currentUtterance.voice = preferredVoice;

  currentUtterance.onstart = () => {
    if (voiceVisualizer) voiceVisualizer.setState("SPEAKING", "TRANSMITTING VOICE RESPONSE");
    setVoiceMiniStatus("● SPEAKING", "#E8D84F");
    appendActivityFeed("[VOX]", "Voice transmission active");
  };

  currentUtterance.onend = () => {
    if (voiceVisualizer) voiceVisualizer.setState("STANDBY", "AWAITING VOICE INPUT");
    setVoiceMiniStatus("● STANDBY", "#E8D84F");
    appendActivityFeed("[VOX]", "Speak turn completed");
  };

  currentUtterance.onerror = () => {
    if (voiceVisualizer) voiceVisualizer.setState("STANDBY");
    setVoiceMiniStatus("● STANDBY", "#E8D84F");
  };

  window.speechSynthesis.speak(currentUtterance);
}

function stopSpeechOutput() {
  if ('speechSynthesis' in window) {
    window.speechSynthesis.cancel();
  }
  stopVoiceInput();
  if (voiceVisualizer) voiceVisualizer.setState("STANDBY");
  setVoiceMiniStatus("● STANDBY", "#E8D84F");
  appendActivityFeed("[VOX]", "Voice transmission halted");
  playSound("alert");
}

function pauseSpeechOutput() {
  if ('speechSynthesis' in window) {
    if (window.speechSynthesis.speaking) {
      if (window.speechSynthesis.paused) {
        window.speechSynthesis.resume();
        setVoiceMiniStatus("● SPEAKING", "#E8D84F");
      } else {
        window.speechSynthesis.pause();
        setVoiceMiniStatus("● PAUSED", "#B9A83D");
      }
    }
  }
}

function setVoiceMiniStatus(text, color) {
  const badge = document.getElementById("voice-mini-status");
  if (badge) {
    badge.innerText = text;
    badge.style.color = color;
  }
}


// ========================================================
// 6. Command Execution & Telemetry Bridge
// ========================================================
function submitVoiceCommand() {
  const input = document.getElementById("voice-command-input");
  if (!input) return;
  const cmd = input.value.trim();
  if (!cmd) return;

  input.value = "";
  sendVoicePrompt(cmd);
}

async function sendVoicePrompt(promptText) {
  playSound("transmit");
  if (voiceVisualizer) voiceVisualizer.setState("THINKING", "PROCESSING TELEMETRY & QUERY");
  setVoiceMiniStatus("● THINKING", "#B9A83D");

  appendActivityFeed("[SYS]", `Query: "${promptText}"`);
  showTranscriptCard(`<em>Synthesizing tactical response...</em>`, null);

  const lower = promptText.toLowerCase().trim();
  const directShellCommands = ["status", "sched", "paging", "banker", "detect", "disk", "fit", "alloc", "sync", "files", "ls", "panic", "irq", "memory", "threads", "process", "ticks", "syscall", "ipc", "clear", "help"];

  // Direct kernel command execution
  const isDirectShell = directShellCommands.some(c => lower === c || lower.startsWith(c + " "));

  if (isDirectShell) {
    if (voiceVisualizer) voiceVisualizer.setState("EXECUTING", `DISPATCHING: ${lower.split(" ")[0].toUpperCase()}`);
    try {
      const res = await fetch("/api/shell/exec", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ command: promptText })
      });
      const data = await res.json();
      playSound("ack");

      showTranscriptCard(`<pre style="font-family:var(--font-mono); font-size:11.5px; color:#D8D6C9; margin:0;">${escapeHtml(data.output)}</pre>`, "kernel.shell_dispatch");
      speakAssistantResponse(`Command ${lower.split(" ")[0]} executed successfully.`);
      appendActivityFeed("[SYS]", `Executed: ${lower.split(" ")[0].toUpperCase()}`);
    } catch (e) {
      playSound("alert");
      showTranscriptCard(`<span style="color:var(--red-alert);">[EXECUTION FAULT: ${e.message}]</span>`, null);
      if (voiceVisualizer) voiceVisualizer.setState("STANDBY");
    }
  } else {
    // Natural Language Query via AI Companion
    try {
      const res = await fetch("/api/companion/query", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ prompt: promptText })
      });
      const data = await res.json();
      playSound("ack");

      let html = `<div style="font-size:14px; margin-bottom:8px;">${escapeHtml(data.llm_response)}</div>`;
      if (data.tool_result) {
        html += `
          <div style="background:rgba(16,17,12,0.85); border:1px solid var(--border-olive); padding:6px 8px; margin-top:6px; font-family:var(--font-mono); font-size:10.5px;">
            <div style="color:var(--gold-primary); margin-bottom:3px;">// TELEMETRY PAYLOAD:</div>
            <pre style="color:var(--text-muted); margin:0;">${escapeHtml(JSON.stringify(data.tool_result, null, 2))}</pre>
          </div>
        `;
      }
      showTranscriptCard(html, data.tool_called);
      speakAssistantResponse(data.llm_response);
      appendActivityFeed("[AI]", `Delivered response`);
    } catch (e) {
      playSound("alert");
      showTranscriptCard(`<span style="color:var(--red-alert);">[COMM BRIDGE FAULT: ${e.message}]</span>`, null);
      if (voiceVisualizer) voiceVisualizer.setState("STANDBY");
    }
  }

  refreshBackgroundData();
}

function showTranscriptCard(htmlContent, toolName = null) {
  const card = document.getElementById("voice-transcript-card");
  const body = document.getElementById("transcript-body-text");
  const toolTag = document.getElementById("transcript-tool-tag");
  const toolLabel = document.getElementById("transcript-tool-name");

  if (!card || !body) return;

  body.innerHTML = htmlContent;

  if (toolName) {
    if (toolTag) toolTag.style.display = "flex";
    if (toolLabel) toolLabel.innerText = `TOOL: ${toolName}`;
  } else {
    if (toolTag) toolTag.style.display = "none";
  }

  card.classList.add("active");
}

function closeTranscriptCard() {
  playSound("click");
  const card = document.getElementById("voice-transcript-card");
  if (card) card.classList.remove("active");
}

function appendActivityFeed(tag, text) {
  const feed = document.getElementById("activity-feed-list");
  if (!feed) return;

  const item = document.createElement("div");
  item.className = "feed-item";

  let bulletClass = "";
  if (tag.includes("VOX") || tag.includes("SEC")) bulletClass = "green";
  if (tag.includes("ERR") || tag.includes("FAULT")) bulletClass = "red";

  item.innerHTML = `
    <span class="feed-bullet ${bulletClass}">•</span>
    <span class="feed-text">${escapeHtml(text)}</span>
  `;

  feed.appendChild(item);
  feed.scrollTop = feed.scrollHeight;
}

function clearActivityFeed() {
  const feed = document.getElementById("activity-feed-list");
  if (feed) feed.innerHTML = "";
  playSound("click");
}


// ========================================================
// 7. Real-Time Telemetry (WebSocket + REST Fallback)
// ========================================================
function initTelemetry() {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const wsUrl = `${protocol}//${window.location.host}/ws/telemetry`;

  try {
    telemetrySocket = new WebSocket(wsUrl);
    telemetrySocket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === "TELEMETRY_UPDATE") {
          updateVitals(data);
        }
      } catch (err) {}
    };

    telemetrySocket.onclose = () => {
      setTimeout(initTelemetry, 3000);
    };
  } catch (e) {
    setInterval(pollTelemetryRest, 2000);
  }
}

function updateVitals(data) {
  const elTicks = document.getElementById("vital-ticks-num");
  const elCtx = document.getElementById("vital-ctx-num");
  const elCtxBar = document.getElementById("vital-ctx-bar");
  const elTicksBar = document.getElementById("vital-ticks-bar");

  if (elTicks) elTicks.innerText = data.ticks;
  if (elCtx) elCtx.innerText = data.context_switches;

  if (elCtxBar) elCtxBar.style.width = `${Math.min(100, (data.context_switches % 100))}%`;
  if (elTicksBar) elTicksBar.style.width = `${Math.min(100, (data.ticks % 100))}%`;
}

async function pollTelemetryRest() {
  try {
    const res = await fetch("/api/status");
    if (res.ok) {
      const data = await res.json();
      updateVitals({
        ticks: data.ticks,
        context_switches: data.context_switches
      });
    }
  } catch (e) {}
}


// ========================================================
// 8. Modals (Algorithm Studio, Storage, Shell)
// ========================================================
function openStudioModal(id) {
  playSound("click");
  const modal = document.getElementById(id);
  if (modal) modal.classList.add("active");

  if (id === "algo-modal") runModalCPUSched();
  if (id === "storage-modal") refreshModalStorage();
  if (id === "term-modal") {
    const input = document.getElementById("term-input-field");
    if (input) setTimeout(() => input.focus(), 150);
  }
}

function closeStudioModal(id) {
  playSound("click");
  const modal = document.getElementById(id);
  if (modal) modal.classList.remove("active");
}

function showModalAlgo(paneId) {
  playSound("click");
  document.querySelectorAll(".modal-algo-pane").forEach(p => p.style.display = "none");
  const target = document.getElementById(`pane-${paneId}`);
  if (target) target.style.display = "block";

  if (paneId === "m-cpu") runModalCPUSched();
  if (paneId === "m-page") runModalPageReplace();
  if (paneId === "m-banker") runModalBanker();
  if (paneId === "m-mem") runModalMemFit();
  if (paneId === "m-disk") runModalDiskSched();
}

async function runModalCPUSched() {
  const algo = document.getElementById("m-cpu-algo")?.value || "rr";
  const quantum = parseInt(document.getElementById("m-cpu-quantum")?.value || "3");

  const tasks = [
    { id: 1, arrival_time: 0, burst_time: 6 },
    { id: 2, arrival_time: 1, burst_time: 8 },
    { id: 3, arrival_time: 2, burst_time: 3 },
    { id: 4, arrival_time: 3, burst_time: 4 }
  ];

  try {
    const res = await fetch("/api/algorithms/cpu-sched", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ algorithm: algo, tasks, quantum })
    });
    const data = await res.json();

    const strip = document.getElementById("m-gantt-strip");
    if (strip) {
      strip.innerHTML = "";
      const totalTime = data.gantt.length ? data.gantt[data.gantt.length - 1].end : 0;
      const colors = ["#E8D84F", "#B9A83D", "#8FBF67", "#73805A", "#C95B4B"];

      data.gantt.forEach(s => {
        const div = document.createElement("div");
        div.className = "modal-gantt-cell";
        div.style.width = `${(s.duration / totalTime) * 100}%`;
        div.style.background = typeof s.id === "number" ? colors[(s.id - 1) % colors.length] : "#444";
        div.innerText = s.label;
        strip.appendChild(div);
      });
    }

    const summary = document.getElementById("m-cpu-summary");
    if (summary) {
      summary.innerHTML = `Algorithm: <strong>${data.algorithm}</strong> | Avg Turnaround: <strong>${data.avg_turnaround}</strong> | Avg Waiting: <strong>${data.avg_waiting}</strong>`;
    }
  } catch (e) {}
}

async function runModalPageReplace() {
  const algo = document.getElementById("m-page-algo")?.value || "fifo";
  const frames = parseInt(document.getElementById("m-page-frames")?.value || "3");
  const refStr = document.getElementById("m-page-refs")?.value || "7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1";
  const refs = refStr.split(",").map(s => parseInt(s.trim())).filter(n => !isNaN(n));

  try {
    const res = await fetch("/api/algorithms/page-replace", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ algorithm: algo, frame_count: frames, refs })
    });
    const data = await res.json();
    const sum = document.getElementById("m-page-summary");
    if (sum) {
      sum.innerHTML = `Total Refs: <strong>${data.total_refs}</strong> | Faults: <strong style="color:var(--red-alert);">${data.faults}</strong> | Hits: <strong style="color:var(--green-healthy);">${data.hits}</strong> (${data.hit_rate}%)`;
    }
  } catch (e) {}
}

async function runModalBanker() {
  const alloc = [[0, 1, 0], [2, 0, 0], [3, 0, 2], [2, 1, 1], [0, 0, 2]];
  const max_m = [[7, 5, 3], [3, 2, 2], [9, 0, 2], [2, 2, 2], [4, 3, 3]];
  const avail = [3, 3, 2];

  try {
    const res = await fetch("/api/algorithms/banker/safety", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ allocation: alloc, max_matrix: max_m, available: avail })
    });
    const data = await res.json();
    const sum = document.getElementById("m-banker-summary");
    if (sum) {
      sum.innerHTML = `Status: <strong style="color:${data.is_safe ? 'var(--green-healthy)' : 'var(--red-alert)'};">${data.is_safe ? 'SAFE' : 'UNSAFE'}</strong> | Safe Sequence: <strong style="color:var(--gold-primary);">${data.safe_sequence.join(" -> ")}</strong>`;
    }
  } catch (e) {}
}

async function runModalMemFit() {
  const blocks = [100, 500, 200, 300, 600];
  const requests = [212, 417, 112, 426];

  try {
    const res = await fetch("/api/algorithms/mem-fit", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ strategy: "best", blocks, requests })
    });
    const data = await res.json();

    const bar = document.getElementById("m-mem-bar");
    if (bar) {
      bar.innerHTML = "";
      data.block_states.forEach(b => {
        const div = document.createElement("div");
        div.style.width = `${(b.size / data.total_memory) * 100}%`;
        div.style.height = "100%";
        div.style.display = "flex";
        div.style.alignItems = "center";
        div.style.justifyContent = "center";
        div.style.borderRight = "1px solid #000";
        div.style.fontFamily = "var(--font-mono)";
        div.style.fontSize = "9.5px";

        if (b.allocated) {
          div.style.background = "var(--green-healthy)";
          div.style.color = "#070805";
          div.innerText = `${b.process} (${b.allocated_size}K)`;
        } else {
          div.style.background = "#141510";
          div.style.color = "var(--text-muted)";
          div.innerText = `${b.size}K`;
        }
        bar.appendChild(div);
      });
    }

    const sum = document.getElementById("m-mem-summary");
    if (sum) {
      sum.innerHTML = `Allocated: <strong>${data.successful_allocations}/${requests.length}</strong> | Ext Fragmentation: <strong>${data.external_fragmentation} KB</strong>`;
    }
  } catch (e) {}
}

async function runModalDiskSched() {
  const reqs = [98, 183, 37, 122, 14, 124, 65, 67];

  try {
    const res = await fetch("/api/algorithms/disk-sched", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ algorithm: "scan", initial_head: 53, requests: reqs, disk_size: 200 })
    });
    const data = await res.json();
    const sum = document.getElementById("m-disk-summary");
    if (sum) {
      sum.innerHTML = `SCAN Total Movement: <strong style="color:var(--gold-primary);">${data.total_head_movement} cylinders</strong> | Trajectory: ${data.trajectory.join(" -> ")}`;
    }
  } catch (e) {}
}

async function runDirectBelady() {
  playSound("alert");
  if (voiceVisualizer) voiceVisualizer.setState("THINKING", "VERIFYING BELADY'S ANOMALY");
  try {
    const res = await fetch("/api/algorithms/belady");
    const data = await res.json();
    const html = `
      <div style="color:var(--red-alert); font-weight:700; margin-bottom:4px;">⚠️ BELADY'S ANOMALY CONFIRMED</div>
      <div style="font-family:var(--font-mono); font-size:11.5px;">
        FIFO with 3 Frames: <strong style="color:var(--gold-primary);">${data.frames_3_faults} faults</strong><br>
        FIFO with 4 Frames: <strong style="color:var(--red-alert);">${data.frames_4_faults} faults</strong> (Increased with more frames!)
      </div>
      <div style="margin-top:6px; font-size:12px; color:var(--text-muted);">${data.explanation}</div>
    `;
    showTranscriptCard(html, "memory.page_replace_belady");
    speakAssistantResponse(`Belady's anomaly confirmed. Page faults increased from 9 to 10 when frames expanded from 3 to 4.`);
  } catch (e) {}
}

async function refreshModalStorage() {
  try {
    const res = await fetch("/api/storage/files");
    const data = await res.json();

    const grid = document.getElementById("storage-blocks-grid");
    if (grid) {
      grid.innerHTML = "";
      for (let i = 0; i < data.disk.total_blocks; i++) {
        const d = document.createElement("div");
        d.style.aspectRatio = "1";
        d.style.fontSize = "8px";
        d.style.display = "flex";
        d.style.alignItems = "center";
        d.style.justifyContent = "center";
        d.style.border = "1px solid rgba(133, 135, 118, 0.2)";
        d.innerText = i;

        if (i === 0) d.style.background = "#B9A83D";
        else if (i === 1 || i === 2) d.style.background = "#73805A";
        else if (i === 3) d.style.background = "#E8D84F";
        else {
          const isUsed = data.disk.bitmap[i - 4] === 1;
          d.style.background = isUsed ? "var(--green-healthy)" : "#0c0d09";
          d.style.color = isUsed ? "#070805" : "#444";
        }
        grid.appendChild(d);
      }
    }

    const tbody = document.getElementById("storage-files-tbody");
    if (tbody) {
      tbody.innerHTML = "";
      data.files.forEach(f => {
        const tr = document.createElement("tr");
        tr.style.borderBottom = "1px solid rgba(255,255,255,0.04)";
        tr.innerHTML = `
          <td><strong>${escapeHtml(f.name)}</strong></td>
          <td>${f.size} B</td>
          <td>#${f.inode}</td>
          <td>[${f.blocks.join(", ")}]</td>
          <td>
            <button class="voice-btn" style="padding:1px 5px;" onclick="viewModalDoc('${escapeHtml(f.name)}')">READ</button>
          </td>
        `;
        tbody.appendChild(tr);
      });
    }
  } catch (e) {}
}

async function viewModalDoc(filename) {
  try {
    const res = await fetch(`/api/storage/file/${filename}`);
    const data = await res.json();
    showTranscriptCard(`
      <div style="color:var(--gold-primary); font-weight:700; margin-bottom:4px;">// DOCUMENT: ${escapeHtml(filename)}</div>
      <pre style="font-family:var(--font-mono); font-size:11.5px; color:#D8D6C9; margin:0;">${escapeHtml(data.content)}</pre>
    `, "storage.read_file");
    speakAssistantResponse(`Opened document ${filename}.`);
  } catch (e) {}
}

async function execTermCommand() {
  const input = document.getElementById("term-input-field");
  if (!input) return;
  const cmd = input.value.trim();
  if (!cmd) return;
  input.value = "";

  const out = document.getElementById("term-output-box");
  if (cmd.toLowerCase() === "clear") {
    if (out) out.innerHTML = "";
    return;
  }

  printTermLine(`jarvis> ${escapeHtml(cmd)}`);
  try {
    const res = await fetch("/api/shell/exec", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command: cmd })
    });
    const data = await res.json();
    if (data.output === "__CLEAR__") {
      if (out) out.innerHTML = "";
    } else {
      printTermLine(escapeHtml(data.output));
    }
  } catch (e) {
    printTermLine(`[ERROR: ${e.message}]`);
  }
}

function printTermLine(text) {
  const out = document.getElementById("term-output-box");
  if (!out) return;
  out.innerHTML += `\n${text}`;
  out.scrollTop = out.scrollHeight;
}

function spawnTaskPrompt() {
  const name = prompt("Enter mission / task name:", "alpha_routine");
  if (!name) return;
  fetch("/api/processes/create", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name, priority: 1, is_thread: false })
  }).then(() => {
    appendActivityFeed("[SYS]", `Task dispatched: ${name}`);
    speakAssistantResponse(`Mission ${name} initiated.`);
  });
}

async function switchBackend() {
  const sel = document.getElementById("llm-backend-select");
  if (!sel) return;
  const backend = sel.value;
  playSound("click");

  try {
    await fetch("/api/companion/backend", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ backend })
    });
    appendActivityFeed("[SYS]", `Backend switched: ${backend.toUpperCase()}`);
    speakAssistantResponse(`Inference backend set to ${backend}.`);
  } catch (e) {}
}

function refreshBackgroundData() {
  // Can poll status or refresh file tables if needed
}

function escapeHtml(str) {
  if (typeof str !== "string") return str;
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}


// ========================================================
// 9. Initialization
// ========================================================
window.addEventListener("DOMContentLoaded", () => {
  // Initialize Voice Visualizer on main stage
  voiceVisualizer = new VoiceVisualizer("jarvis-voice-canvas");

  // Initialize Speech Recognition
  initSpeechRecognition();

  // Initialize Mini Waveform
  initMiniWaveform();

  // Start Real-Time Digital Clock
  setInterval(updateDigitalClock, 1000);
  updateDigitalClock();

  // Initialize WebSocket Telemetry
  initTelemetry();

  // Prompt input listener
  const input = document.getElementById("voice-command-input");
  if (input) {
    input.addEventListener("keydown", (e) => {
      if (e.key === "Enter") submitVoiceCommand();
    });
  }

  const termInput = document.getElementById("term-input-field");
  if (termInput) {
    termInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") execTermCommand();
    });
  }
});
