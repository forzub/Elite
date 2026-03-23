import * as THREE from 'https://unpkg.com/three@0.128.0/build/three.module.js';
import { OrbitControls } from 'https://unpkg.com/three@0.128.0/examples/jsm/controls/OrbitControls.js';

// Состояние приложения
const state = {
    scene: null,
    camera: null,
    renderer: null,
    controls: null,
    modelGroup: new THREE.Group(),
    hitVolumes: [],
    hitVolumeMeshes: [],
    modelEdges: null,
    modelWireframe: null,
    labels: [],
    showLabels: true,
    showWireframe: false,
    modelColor: '#00ff00',
    modelWidth: 1.0,
    hitColor: '#ff4444',
    hitWidth: 3.0,
    bgColor: '#111111'
};

// Показ ошибок
function showError(message) {
    const errorEl = document.getElementById('error-container');
    errorEl.style.display = 'block';
    errorEl.textContent = 'ERROR: ' + message;
    setTimeout(() => {
        errorEl.style.display = 'none';
    }, 5000);
}

// Инициализация сцены
function initScene() {
    try {
        const canvas = document.getElementById('hit-volumes-canvas');
        if (!canvas) {
            throw new Error('Canvas not found');
        }
        
        // Сцена
        state.scene = new THREE.Scene();
        state.scene.background = new THREE.Color(state.bgColor);
        
        // Камера
        state.camera = new THREE.PerspectiveCamera(45, canvas.clientWidth / canvas.clientHeight, 0.1, 1000);
        state.camera.position.set(20, 15, 30);
        state.camera.lookAt(0, 0, 0);
        
        // Рендерер
        state.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
        state.renderer.setSize(canvas.clientWidth, canvas.clientHeight);
        state.renderer.setPixelRatio(window.devicePixelRatio);
        
        // Освещение
        const ambientLight = new THREE.AmbientLight(0x404060);
        state.scene.add(ambientLight);
        
        const dirLight1 = new THREE.DirectionalLight(0xffffff, 1);
        dirLight1.position.set(1, 2, 1);
        state.scene.add(dirLight1);
        
        const dirLight2 = new THREE.DirectionalLight(0xffaa88, 0.8);
        dirLight2.position.set(-1, 1, -1);
        state.scene.add(dirLight2);
        
        // Сетка для ориентации
        const gridHelper = new THREE.GridHelper(30, 20, 0x00ff00, 0x333333);
        gridHelper.position.y = -2;
        state.scene.add(gridHelper);
        
        // Контролы
        state.controls = new OrbitControls(state.camera, state.renderer.domElement);
        state.controls.enableDamping = true;
        state.controls.dampingFactor = 0.05;
        state.controls.screenSpacePanning = true;
        state.controls.maxPolarAngle = Math.PI / 2;
        
        // Запуск анимации
        animate();
        
        // Обработка ресайза
        window.addEventListener('resize', onWindowResize);
        
        console.log('Scene initialized successfully');
    } catch (err) {
        showError('Failed to initialize 3D scene: ' + err.message);
    }
}

// Анимация
function animate() {
    requestAnimationFrame(animate);
    
    if (state.controls) {
        state.controls.update();
    }
    
    if (state.renderer && state.scene && state.camera) {
        state.renderer.render(state.scene, state.camera);
    }
}

// Ресайз окна
function onWindowResize() {
    const canvas = document.getElementById('hit-volumes-canvas');
    if (!canvas || !state.camera || !state.renderer) return;
    
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    
    state.camera.aspect = width / height;
    state.camera.updateProjectionMatrix();
    state.renderer.setSize(width, height);
}

// Создание куба для hit volume
function createHitVolumeMesh(volume, color, lineWidth) {
    const [x, y, z] = volume.position;
    const [w, h, d] = volume.size;
    
    const geometry = new THREE.BoxGeometry(w, h, d);
    const edges = new THREE.EdgesGeometry(geometry);
    const line = new THREE.LineSegments(
        edges, 
        new THREE.LineBasicMaterial({ 
            color: color || state.hitColor, 
            linewidth: lineWidth || state.hitWidth 
        })
    );
    
    line.position.set(x, y, z);
    line.userData = { type: 'hitVolume', volume };
    
    return line;
}

// Создание текстовой метки
function createLabel(text, position, color = '#ff4444') {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    canvas.width = 256;
    canvas.height = 128;
    
    ctx.fillStyle = 'rgba(0,0,0,0.7)';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.strokeRect(0, 0, canvas.width, canvas.height);
    
    ctx.font = 'Bold 24px Courier New';
    ctx.fillStyle = color;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, canvas.width/2, canvas.height/2);
    
    const texture = new THREE.CanvasTexture(canvas);
    const material = new THREE.SpriteMaterial({ map: texture });
    const sprite = new THREE.Sprite(material);
    
    sprite.scale.set(2, 1, 1);
    sprite.position.set(position[0], position[1] + 2, position[2]);
    
    return sprite;
}

// Загрузка hit volumes
function loadHitVolumes(volumesData) {
    try {
        // Очищаем старые
        state.hitVolumeMeshes.forEach(mesh => state.scene.remove(mesh));
        state.labels.forEach(label => state.scene.remove(label));
        state.hitVolumeMeshes = [];
        state.labels = [];
        
        // Создаем новые
        volumesData.forEach((vol, index) => {
            let type, label, hp, pos, size, critical, priority;
            
            if (Array.isArray(vol)) {
                [type, label, hp, pos, size, critical, priority] = vol;
            } else {
                type = vol.type;
                label = vol.label;
                hp = vol.hp;
                pos = vol.position;
                size = vol.size;
                critical = vol.critical;
                priority = vol.priority;
            }
            
            const volume = {
                type,
                label,
                hp,
                position: pos,
                size,
                critical,
                priority,
                index
            };
            
            const mesh = createHitVolumeMesh(volume);
            state.hitVolumeMeshes.push(mesh);
            state.scene.add(mesh);
            
            // Создаем метку
            const labelSprite = createLabel(label, pos, critical ? '#ff0000' : '#ff4444');
            state.labels.push(labelSprite);
            if (state.showLabels) {
                state.scene.add(labelSprite);
            }
        });
        
        // Обновляем UI
        document.getElementById('hit-count').textContent = volumesData.length;
        updateHitVolumesList(volumesData);
        document.getElementById('raw-hit-data').textContent = JSON.stringify(volumesData, null, 2);
        
        console.log('Hit volumes loaded:', volumesData.length);
    } catch (err) {
        showError('Failed to load hit volumes: ' + err.message);
    }
}

// Обновление списка hit volumes
function updateHitVolumesList(volumes) {
    const listEl = document.getElementById('hit-volumes-list');
    let html = '';
    
    volumes.forEach((vol, index) => {
        let type, label, hp, pos, size, critical;
        
        if (Array.isArray(vol)) {
            [type, label, hp, pos, size, critical] = vol;
        } else {
            type = vol.type;
            label = vol.label;
            hp = vol.hp;
            pos = vol.position;
            size = vol.size;
            critical = vol.critical;
        }
        
        html += `
            <div class="hit-volume-item">
                <div class="hit-volume-header" onclick="window.hitVolumesApp.toggleVolume(${index})">
                    <span class="hit-volume-name">${label}</span>
                    <span class="hit-volume-type ${critical ? 'status-critical' : ''}">HP: ${hp}</span>
                </div>
                <div class="hit-volume-controls" id="volume-${index}" style="display: grid;">
                    <div class="control-group">
                        <span class="control-label">Color</span>
                        <input type="color" value="${critical ? '#ff0000' : '#ff4444'}" onchange="window.hitVolumesApp.updateVolumeColor(${index}, this.value)">
                    </div>
                    <div class="control-group">
                        <span class="control-label">Visible</span>
                        <input type="checkbox" checked onchange="window.hitVolumesApp.toggleVolumeVisible(${index}, this.checked)">
                    </div>
                </div>
            </div>
        `;
    });
    
    listEl.innerHTML = html;
}

// Загрузка OBJ модели
function loadObjModel(objData) {
    try {
        // Очищаем старую модель
        if (state.modelEdges) {
            state.scene.remove(state.modelEdges);
            state.modelEdges = null;
        }
        if (state.modelWireframe) {
            state.scene.remove(state.modelWireframe);
            state.modelWireframe = null;
        }
        
        if (!objData) return;
        
        // Парсим OBJ
        const lines = objData.split('\n');
        const vertices = [];
        const faces = [];
        
        lines.forEach(line => {
            line = line.trim();
            if (!line || line.startsWith('#')) return;
            
            const parts = line.split(/\s+/);
            if (parts[0] === 'v') {
                vertices.push(new THREE.Vector3(
                    parseFloat(parts[1]) || 0,
                    parseFloat(parts[2]) || 0,
                    parseFloat(parts[3]) || 0
                ));
            } else if (parts[0] === 'f') {
                const face = [];
                for (let i = 1; i < parts.length; i++) {
                    const vIdx = parseInt(parts[i].split('/')[0]) - 1;
                    if (vIdx >= 0 && vIdx < vertices.length) {
                        face.push(vIdx);
                    }
                }
                if (face.length >= 3) {
                    faces.push(face);
                }
            }
        });
        
        if (vertices.length === 0) {
            throw new Error('No vertices found in OBJ file');
        }
        
        // Нормируем вершины
        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;
        let minZ = Infinity, maxZ = -Infinity;
        
        vertices.forEach(v => {
            minX = Math.min(minX, v.x);
            maxX = Math.max(maxX, v.x);
            minY = Math.min(minY, v.y);
            maxY = Math.max(maxY, v.y);
            minZ = Math.min(minZ, v.z);
            maxZ = Math.max(maxZ, v.z);
        });
        
        const centerX = (minX + maxX) / 2;
        const centerY = (minY + maxY) / 2;
        const centerZ = (minZ + maxZ) / 2;
        const maxDim = Math.max(maxX - minX, maxY - minY, maxZ - minZ);
        const scale = 10 / maxDim;
        
        // Создаем геометрию
        const geometry = new THREE.BufferGeometry();
        const positionArray = [];
        
        vertices.forEach(v => {
            positionArray.push(
                (v.x - centerX) * scale,
                (v.y - centerY) * scale,
                (v.z - centerZ) * scale
            );
        });
        
        geometry.setAttribute('position', new THREE.Float32BufferAttribute(positionArray, 3));
        
        // Создаем индексы для ребер
        const edges = [];
        const edgeSet = new Set();
        
        faces.forEach(face => {
            for (let i = 0; i < face.length; i++) {
                const a = face[i];
                const b = face[(i + 1) % face.length];
                if (a !== undefined && b !== undefined) {
                    const edge = a < b ? `${a}-${b}` : `${b}-${a}`;
                    if (!edgeSet.has(edge)) {
                        edgeSet.add(edge);
                        edges.push(a, b);
                    }
                }
            }
        });
        
        if (edges.length > 0) {
            geometry.setIndex(edges);
            
            // Создаем ребра модели
            state.modelEdges = new THREE.LineSegments(
                geometry,
                new THREE.LineBasicMaterial({ color: state.modelColor, linewidth: state.modelWidth })
            );
            state.scene.add(state.modelEdges);
        }
        
        // Создаем вайрфрейм
        if (faces.length > 0) {
            const wireframeIndices = [];
            faces.forEach(face => {
                for (let i = 1; i < face.length - 1; i++) {
                    if (face[0] !== undefined && face[i] !== undefined && face[i + 1] !== undefined) {
                        wireframeIndices.push(face[0], face[i], face[i + 1]);
                    }
                }
            });
            
            if (wireframeIndices.length > 0) {
                const wireframeGeometry = new THREE.BufferGeometry();
                wireframeGeometry.setAttribute('position', new THREE.Float32BufferAttribute(positionArray, 3));
                wireframeGeometry.setIndex(wireframeIndices);
                
                state.modelWireframe = new THREE.Mesh(
                    wireframeGeometry,
                    new THREE.MeshBasicMaterial({ 
                        color: 0x888888, 
                        wireframe: true,
                        transparent: true,
                        opacity: 0.3
                    })
                );
                
                if (state.showWireframe) {
                    state.scene.add(state.modelWireframe);
                }
            }
        }
        
        document.getElementById('model-info').textContent = `loaded (${vertices.length} verts)`;
        console.log('Model loaded successfully');
    } catch (err) {
        showError('Failed to load OBJ model: ' + err.message);
    }
}

// Обновление стиля модели
function updateModelStyle() {
    const color = document.getElementById('model-color').value;
    const width = parseFloat(document.getElementById('model-width').value);
    
    state.modelColor = color;
    state.modelWidth = width;
    
    if (state.modelEdges) {
        state.modelEdges.material.color.set(color);
        state.modelEdges.material.linewidth = width;
    }
    
    document.getElementById('model-width-value').textContent = width.toFixed(1);
}

// Обновление всех hit volumes
function updateAllHitVolumes() {
    const color = document.getElementById('hit-color').value;
    const width = parseFloat(document.getElementById('hit-width').value);
    
    state.hitColor = color;
    state.hitWidth = width;
    
    state.hitVolumeMeshes.forEach(mesh => {
        mesh.material.color.set(color);
        mesh.material.linewidth = width;
    });
    
    document.getElementById('hit-width-value').textContent = width.toFixed(1);
}

// Обновление цвета конкретного volume
function updateVolumeColor(index, color) {
    if (state.hitVolumeMeshes[index]) {
        state.hitVolumeMeshes[index].material.color.set(color);
    }
}

// Видимость volume
function toggleVolumeVisible(index, visible) {
    if (state.hitVolumeMeshes[index]) {
        state.hitVolumeMeshes[index].visible = visible;
    }
    if (state.labels[index]) {
        state.labels[index].visible = visible && state.showLabels;
    }
}

// Скрыть/показать панель управления volume
function toggleVolume(index) {
    const controls = document.getElementById(`volume-${index}`);
    if (controls) {
        controls.style.display = controls.style.display === 'none' ? 'grid' : 'none';
    }
}

// Обновление фона
function updateBackground() {
    const color = document.getElementById('bg-color').value;
    state.bgColor = color;
    if (state.scene) {
        state.scene.background = new THREE.Color(color);
    }
}

// Включение/выключение меток
function toggleLabels() {
    state.showLabels = document.getElementById('show-labels').checked;
    state.labels.forEach(label => {
        label.visible = state.showLabels;
    });
}

// Включение/выключение вайрфрейма
function toggleWireframe() {
    state.showWireframe = document.getElementById('show-wireframe').checked;
    
    if (state.showWireframe && state.modelWireframe && !state.modelWireframe.parent) {
        state.scene.add(state.modelWireframe);
    } else if (!state.showWireframe && state.modelWireframe && state.modelWireframe.parent) {
        state.scene.remove(state.modelWireframe);
    }
}

// Сброс вида
function resetView() {
    if (state.camera && state.controls) {
        state.camera.position.set(20, 15, 30);
        state.camera.lookAt(0, 0, 0);
        state.controls.target.set(0, 0, 0);
        state.controls.update();
    }
}

// Загрузка тестовой модели
function loadSampleModel() {
    const sampleObj = `# Simple cube model
v -5 -5 -5
v 5 -5 -5
v 5 5 -5
v -5 5 -5
v -5 -5 5
v 5 -5 5
v 5 5 5
v -5 5 5

f 1 2 3 4
f 5 6 7 8
f 1 2 6 5
f 2 3 7 6
f 3 4 8 7
f 4 1 5 8`;
    
    loadObjModel(sampleObj);
}

// Инициализация
document.addEventListener('DOMContentLoaded', () => {
    // Создаем глобальный объект приложения
    window.hitVolumesApp = {
        loadSampleModel,
        resetView,
        toggleWireframe,
        updateModelStyle,
        updateAllHitVolumes,
        updateBackground,
        toggleLabels,
        updateVolumeColor,
        toggleVolumeVisible,
        toggleVolume,
        loadHitVolumes
    };
    
    // Инициализируем сцену
    initScene();
    
    // Загружаем тестовые hit volumes
    const sampleHitVolumes = [
        ['Cockpit', 'COCKPIT', 100, [0, 1.5, 6], [4, 2.5, 4], false, -1],
        ['Reactor', 'REACTOR', 250, [0, 0, -3], [6, 4, 5], true, 1],
        ['Engine', 'ENGINE', 150, [0, 0, -7], [7, 3, 4], true, 2],
        ['Cargo', 'CARGO', 80, [0, -1, 2], [10, 3, 8], false, -1],
        ['Hull', 'HULL', 0, [0, 0, 0], [14, 5.8, 16.5], false, -1]
    ];
    
    loadHitVolumes(sampleHitVolumes);
    
    // Загружаем тестовую модель
    setTimeout(loadSampleModel, 500);
    
    // Назначаем обработчики событий
    document.getElementById('model-color').addEventListener('input', updateModelStyle);
    document.getElementById('model-width').addEventListener('input', updateModelStyle);
    document.getElementById('hit-color').addEventListener('input', updateAllHitVolumes);
    document.getElementById('hit-width').addEventListener('input', updateAllHitVolumes);
    document.getElementById('bg-color').addEventListener('input', updateBackground);
    document.getElementById('show-labels').addEventListener('change', toggleLabels);
    document.getElementById('show-wireframe').addEventListener('change', toggleWireframe);
    
    document.getElementById('obj-file').addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = (e) => loadObjModel(e.target.result);
        reader.onerror = () => showError('Failed to read OBJ file');
        reader.readAsText(file);
    });
    
    document.getElementById('hit-json').addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const volumes = JSON.parse(e.target.result);
                loadHitVolumes(volumes);
            } catch (err) {
                showError('Failed to parse JSON: ' + err.message);
            }
        };
        reader.onerror = () => showError('Failed to read JSON file');
        reader.readAsText(file);
    });
});
