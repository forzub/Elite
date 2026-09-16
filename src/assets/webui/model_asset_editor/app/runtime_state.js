function createEditorRuntimeState({THREE,editorViewState}){
 return {
  catalog:[],settings:null,settingsSaving:false,settingsSaveTimer:null,ignoreNextStatusNotice:false,
  asset:null,dirty:false,busy:false,noticeTimer:null,busyTimer:null,
  scene:null,camera:null,renderer:null,controls:null,
  root:new THREE.Group(),nodeGroups:[],renderNodeGroups:[],meshObjects:[],geometryCache:new Map(),
  lodPreviewGeometryCache:new Map(),variantPreviewByNode:new Map(),
  selectedCollision:null,selectedSocket:null,previewStates:new Map(),
  edgeEdit:false,edgeOverlay:null,edgeMap:[],normalOverlay:null,
  collisionGroup:new THREE.Group(),socketGroup:new THREE.Group(),structuralProxyGroup:new THREE.Group(),cameraReferenceGroup:new THREE.Group(),
  i18n:null,locale:'en',localeOrder:['en','ru','zh-Hans','es','ja'],
  wizardStage:'source',geometryScan:null,geometryReference:null,geometryCompareChecked:new Set(),geometryVariantSelected:null,
  surfaceMaterialSelection:null,surfaceApplyAllLods:true,surfaceSelectedGeometryIdsByLod:new Map(),surfaceSelectionAnchorByLod:new Map(),
  semanticApplyAllLods:true,damageApplyAllLods:true,wizardValidationReport:null,damageSelectionKind:null,damageSelectionIndex:null,
  semanticPreviewAngleDeg:0,semanticPreviewDetached:false,semanticPreviewRateDegPerSec:25,semanticMotionPlaying:false,
  semanticMotionDirection:1,semanticMotionLastTs:0,semanticSelectedNodes:new Set(),semanticSelectionAnchor:null,
  semanticBindingPickTarget:null,semanticJointPivotPickTarget:null,semanticCollapsed:new Set(),semanticGraphEnabled:true,
  semanticGraphExplode:0,semanticGraphOffsets:new Map(),semanticStructureMode:'tree',structuralGraphRoot:null,structuralNodeA:null,
  structuralNodeB:null,selectedStructuralLink:null,selectedStructuralProxy:null,
  semanticGizmoGroup:new THREE.Group(),semanticGraphGroup:new THREE.Group(),semanticJointGizmoGroup:new THREE.Group(),
  semanticGraphNodeObjects:new Map(),semanticGraphLinkObjects:new Map(),structuralGraphLinkObjects:new Map(),
  semanticGraphStructureKey:'',semanticGizmoLastUpdateTs:0,semanticGraphApplyRaf:0,
  surfaceAnalysisReady:false,surfaceAnalysisRequested:false,radialRenderNode:null,modelPreflight:null,
  lodAnalysis:null,lodGeneratorPreview:null,lodGeneratorLevel:0,lodGeneratorApplyLevels:new Set(),lodGeneratorAppliedLevels:new Set(),
  lodGeneratorPendingApplyLevels:new Set(),lodGeneratorApplying:false,lodGeneratorMeshSelection:'all',
  lodDiagnosticWireframe:false,lodDiagnosticFaceNormals:false,lodFaceNormalOverlays:[],meshViewportMode:'working',
  sourceChangeScan:null,geometryPartPreflight:null,editorView:editorViewState,
  axisModalDirectMapping:null,axisModalInitialMapping:null,axisModalRotationSteps:[]
 };
}

export {createEditorRuntimeState};
