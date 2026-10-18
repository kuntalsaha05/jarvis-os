/**
 * 3D Neural Intelligence Visualization for J.A.R.V.I.S.
 * Built with Three.js.
 * Renders an animated spherical cloud of luminous golden particles,
 * dynamic synaptic connecting lines, pulsing inner core, and tactical orbital rings.
 */

class NeuralSphereVisualization {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    if (!this.container) return;

    this.scene = null;
    this.camera = null;
    this.renderer = null;

    // Objects
    this.particlesGroup = null;
    this.points = null;
    this.lines = null;
    this.coreMesh = null;
    this.rings = [];

    // Interaction state
    this.isDragging = false;
    this.previousMousePosition = { x: 0, y: 0 };
    this.targetRotation = { x: 0, y: 0 };
    this.currentRotation = { x: 0, y: 0 };
    this.pulseIntensity = 1.0;
    this.targetPulse = 1.0;

    // Configuration
    this.particleCount = 1200;
    this.sphereRadius = 110;
    this.maxDistance = 24;

    this.init();
  }

  init() {
    if (typeof THREE === "undefined") {
      console.warn("Three.js not loaded, neural sphere disabled");
      return;
    }

    const width = this.container.clientWidth || 800;
    const height = this.container.clientHeight || 550;

    // Scene
    this.scene = new THREE.Scene();

    // Camera
    this.camera = new THREE.PerspectiveCamera(50, width / height, 0.1, 1000);
    this.camera.position.z = 260;

    // Renderer
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.setClearColor(0x000000, 0); // Transparent background
    this.container.appendChild(this.renderer.domElement);

    // Group for all sphere elements
    this.particlesGroup = new THREE.Group();
    this.scene.add(this.particlesGroup);

    // Build elements
    this.createParticlesAndSynapses();
    this.createInnerCore();
    this.createOrbitalRings();
    this.createLights();

    // Event listeners
    this.setupEventListeners();

    // Start animation loop
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  createParticlesAndSynapses() {
    const coords = [];
    const colors = [];
    const baseColor = new THREE.Color("#E5D65A"); // Primary gold
    const amberColor = new THREE.Color("#B8A84B"); // Secondary amber
    const oliveColor = new THREE.Color("#73805A"); // Muted green

    // Fibonacci sphere distribution for uniform distribution with natural jitter
    const phi = Math.PI * (3 - Math.sqrt(5));
    for (let i = 0; i < this.particleCount; i++) {
      const y = 1 - (i / (this.particleCount - 1)) * 2;
      const radiusAtY = Math.sqrt(1 - y * y);
      const theta = phi * i;

      // Slight radial variation to give volumetric depth
      const r = this.sphereRadius * (0.85 + Math.random() * 0.3);
      const x = Math.cos(theta) * radiusAtY * r;
      const z = Math.sin(theta) * radiusAtY * r;
      const py = y * r;

      coords.push(new THREE.Vector3(x, py, z));

      // Color variation between gold, amber, and olive
      const rand = Math.random();
      const c = rand > 0.6 ? baseColor : (rand > 0.25 ? amberColor : oliveColor);
      colors.push(c.r, c.g, c.b);
    }

    // 1. Point Cloud
    const particleGeometry = new THREE.BufferGeometry();
    const positions = new Float32Array(coords.length * 3);
    for (let i = 0; i < coords.length; i++) {
      positions[i * 3] = coords[i].x;
      positions[i * 3 + 1] = coords[i].y;
      positions[i * 3 + 2] = coords[i].z;
    }
    particleGeometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    particleGeometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));

    const particleMaterial = new THREE.PointsMaterial({
      size: 2.4,
      vertexColors: true,
      transparent: true,
      opacity: 0.85,
      blending: THREE.AdditiveBlending
    });

    this.points = new THREE.Points(particleGeometry, particleMaterial);
    this.particlesGroup.add(this.points);

    // 2. Synaptic Connecting Lines
    const linePositions = [];
    const lineColors = [];
    const maxConnectionsPerNode = 4;

    for (let i = 0; i < coords.length; i++) {
      let connections = 0;
      for (let j = i + 1; j < coords.length; j++) {
        const dist = coords[i].distanceTo(coords[j]);
        if (dist < this.maxDistance && connections < maxConnectionsPerNode) {
          linePositions.push(coords[i].x, coords[i].y, coords[i].z);
          linePositions.push(coords[j].x, coords[j].y, coords[j].z);

          // Alpha fade by distance
          const alphaFactor = 1 - dist / this.maxDistance;
          lineColors.push(0.72 * alphaFactor, 0.66 * alphaFactor, 0.29 * alphaFactor);
          lineColors.push(0.72 * alphaFactor, 0.66 * alphaFactor, 0.29 * alphaFactor);
          connections++;
        }
      }
    }

    const lineGeometry = new THREE.BufferGeometry();
    lineGeometry.setAttribute("position", new THREE.Float32BufferAttribute(linePositions, 3));
    lineGeometry.setAttribute("color", new THREE.Float32BufferAttribute(lineColors, 3));

    const lineMaterial = new THREE.LineBasicMaterial({
      vertexColors: true,
      transparent: true,
      opacity: 0.35,
      blending: THREE.AdditiveBlending
    });

    this.lines = new THREE.LineSegments(lineGeometry, lineMaterial);
    this.particlesGroup.add(this.lines);
  }

  createInnerCore() {
    // Glowing central core with wireframe icosahedron
    const coreGeo = new THREE.IcosahedronGeometry(36, 2);
    const coreMat = new THREE.MeshBasicMaterial({
      color: 0xE5D65A,
      wireframe: true,
      transparent: true,
      opacity: 0.35,
      blending: THREE.AdditiveBlending
    });
    this.coreMesh = new THREE.Mesh(coreGeo, coreMat);
    this.particlesGroup.add(this.coreMesh);

    // Dense inner nucleus
    const nucleusGeo = new THREE.SphereGeometry(18, 16, 16);
    const nucleusMat = new THREE.MeshBasicMaterial({
      color: 0xFFE873,
      wireframe: false,
      transparent: true,
      opacity: 0.15,
      blending: THREE.AdditiveBlending
    });
    const nucleus = new THREE.Mesh(nucleusGeo, nucleusMat);
    this.particlesGroup.add(nucleus);
    this.nucleus = nucleus;
  }

  createOrbitalRings() {
    const ringConfigs = [
      { radius: 125, tiltX: 0.3, tiltY: 0.2, speed: 0.003, color: 0x73805A },
      { radius: 135, tiltX: 1.1, tiltY: -0.4, speed: -0.002, color: 0xB8A84B },
      { radius: 145, tiltX: -0.8, tiltY: 0.7, speed: 0.0015, color: 0x73805A }
    ];

    ringConfigs.forEach(cfg => {
      const ringGeo = new THREE.RingGeometry(cfg.radius - 0.6, cfg.radius + 0.6, 64);
      const ringMat = new THREE.MeshBasicMaterial({
        color: cfg.color,
        side: THREE.DoubleSide,
        transparent: true,
        opacity: 0.3,
        blending: THREE.AdditiveBlending
      });
      const ring = new THREE.Mesh(ringGeo, ringMat);
      ring.rotation.x = cfg.tiltX;
      ring.rotation.y = cfg.tiltY;
      ring.userData = { speed: cfg.speed };

      this.particlesGroup.add(ring);
      this.rings.push(ring);
    });
  }

  createLights() {
    const ambientLight = new THREE.AmbientLight(0x333322);
    this.scene.add(ambientLight);

    const pointLight = new THREE.PointLight(0xE5D65A, 1.5, 300);
    this.scene.add(pointLight);
  }

  setupEventListeners() {
    window.addEventListener("resize", () => this.onWindowResize());

    const dom = this.renderer.domElement;

    dom.addEventListener("mousedown", (e) => {
      this.isDragging = true;
      this.previousMousePosition = { x: e.clientX, y: e.clientY };
    });

    window.addEventListener("mouseup", () => {
      this.isDragging = false;
    });

    window.addEventListener("mousemove", (e) => {
      if (!this.isDragging) return;
      const deltaX = e.clientX - this.previousMousePosition.x;
      const deltaY = e.clientY - this.previousMousePosition.y;

      this.targetRotation.y += deltaX * 0.005;
      this.targetRotation.x += deltaY * 0.005;

      this.previousMousePosition = { x: e.clientX, y: e.clientY };
    });

    // Touch support
    dom.addEventListener("touchstart", (e) => {
      if (e.touches.length === 1) {
        this.isDragging = true;
        this.previousMousePosition = { x: e.touches[0].clientX, y: e.touches[0].clientY };
      }
    });

    window.addEventListener("touchend", () => {
      this.isDragging = false;
    });

    window.addEventListener("touchmove", (e) => {
      if (!this.isDragging || e.touches.length !== 1) return;
      const deltaX = e.touches[0].clientX - this.previousMousePosition.x;
      const deltaY = e.touches[0].clientY - this.previousMousePosition.y;

      this.targetRotation.y += deltaX * 0.005;
      this.targetRotation.x += deltaY * 0.005;

      this.previousMousePosition = { x: e.touches[0].clientX, y: e.touches[0].clientY };
    });
  }

  onWindowResize() {
    if (!this.container || !this.renderer || !this.camera) return;
    const width = this.container.clientWidth;
    const height = this.container.clientHeight;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  triggerPulse(intensity = 1.8) {
    this.targetPulse = intensity;
  }

  animate() {
    requestAnimationFrame(this.animate);

    // Continuous idle drift rotation
    if (!this.isDragging) {
      this.targetRotation.y += 0.002;
      this.targetRotation.x = Math.sin(Date.now() * 0.0008) * 0.15;
    }

    // Smooth dampening
    this.currentRotation.x += (this.targetRotation.x - this.currentRotation.x) * 0.06;
    this.currentRotation.y += (this.targetRotation.y - this.currentRotation.y) * 0.06;

    if (this.particlesGroup) {
      this.particlesGroup.rotation.x = this.currentRotation.x;
      this.particlesGroup.rotation.y = this.currentRotation.y;
    }

    // Rotate core independently
    if (this.coreMesh) {
      this.coreMesh.rotation.x -= 0.004;
      this.coreMesh.rotation.y += 0.006;
    }

    // Pulse interpolation
    this.pulseIntensity += (this.targetPulse - this.pulseIntensity) * 0.08;
    this.targetPulse += (1.0 - this.targetPulse) * 0.04;

    if (this.coreMesh) {
      const scale = 1.0 + Math.sin(Date.now() * 0.004) * 0.05 * this.pulseIntensity;
      this.coreMesh.scale.set(scale, scale, scale);
      this.coreMesh.material.opacity = 0.3 * this.pulseIntensity;
    }

    if (this.nucleus) {
      const nScale = 1.0 + Math.sin(Date.now() * 0.006) * 0.1 * this.pulseIntensity;
      this.nucleus.scale.set(nScale, nScale, nScale);
      this.nucleus.material.opacity = 0.15 * this.pulseIntensity;
    }

    // Rotate orbital rings
    this.rings.forEach(ring => {
      ring.rotation.z += ring.userData.speed;
    });

    this.renderer.render(this.scene, this.camera);
  }
}

// Global instance handle
window.NeuralSphereVisualization = NeuralSphereVisualization;

