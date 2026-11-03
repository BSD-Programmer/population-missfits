#include "math3d.h"
#include "character_skin.h"
#include "customs_map.h"
#include "battle_gameplay.h"
#include "gles3_renderer.h"

#if defined(__ANDROID__)
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <jni.h>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <ctime>
#include <cstring>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PopMisfitsVR", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PopMisfitsVR", __VA_ARGS__)

namespace misfits {

struct SwapchainBuffer {
    XrSwapchain handle = XR_NULL_HANDLE;
    int32_t width = 0;
    int32_t height = 0;
    std::vector<XrSwapchainImageOpenGLESKHR> images;
};

struct QuestVrApp {
    android_app* app = nullptr;
    bool resumed = false;
    bool windowReady = false;

    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLConfig eglConfig = nullptr;
    EGLContext eglContext = EGL_NO_CONTEXT;
    EGLSurface eglPbufferSurface = EGL_NO_SURFACE;
    EGLSurface eglWindowSurface = EGL_NO_SURFACE;

    // OpenXR handles
    bool openXrActive = false;
    bool sessionRunning = false;
    XrInstance xrInstance = XR_NULL_HANDLE;
    XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
    XrSession xrSession = XR_NULL_HANDLE;
    XrSpace xrLocalSpace = XR_NULL_HANDLE;
    XrSpace xrHeadSpace = XR_NULL_HANDLE;
    XrSessionState xrSessionState = XR_SESSION_STATE_UNKNOWN;

    std::vector<XrViewConfigurationView> viewConfigs;
    std::vector<SwapchainBuffer> swapchains;
    std::vector<XrView> views;

    // OpenXR Actions (Oculus Touch Controllers)
    XrActionSet actionSet = XR_NULL_HANDLE;
    XrAction handPoseAction = XR_NULL_HANDLE;
    XrAction thumbstickAction = XR_NULL_HANDLE;
    XrAction triggerAction = XR_NULL_HANDLE;
    XrAction squeezeAction = XR_NULL_HANDLE;
    XrAction buttonAAction = XR_NULL_HANDLE;
    XrAction buttonBAction = XR_NULL_HANDLE;
    XrAction buttonXAction = XR_NULL_HANDLE;
    XrAction buttonYAction = XR_NULL_HANDLE;
    XrPath handSubactionPaths[2] = {XR_NULL_PATH, XR_NULL_PATH};
    XrSpace handSpaces[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};

    Gles3Renderer renderer;
    BattleEngine battleEngine;
    VRControllerInput vrInput;

    double lastTimeSec = 0.0;
    float elapsedSec = 0.0f;
};

static double getMonotonicSeconds() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

static bool initEglContext(QuestVrApp& state) {
    state.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.eglDisplay == EGL_NO_DISPLAY) return false;

    EGLint major = 0, minor = 0;
    if (!eglInitialize(state.eglDisplay, &major, &minor)) return false;

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLint numConfigs = 0;
    eglChooseConfig(state.eglDisplay, configAttribs, &state.eglConfig, 1, &numConfigs);
    if (numConfigs <= 0) return false;

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    state.eglContext = eglCreateContext(state.eglDisplay, state.eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (state.eglContext == EGL_NO_CONTEXT) return false;

    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    state.eglPbufferSurface = eglCreatePbufferSurface(state.eglDisplay, state.eglConfig, pbufferAttribs);
    eglMakeCurrent(state.eglDisplay, state.eglPbufferSurface, state.eglPbufferSurface, state.eglContext);
    return true;
}

static bool initOpenXrActions(QuestVrApp& state) {
    xrStringToPath(state.xrInstance, "/user/hand/left", &state.handSubactionPaths[0]);
    xrStringToPath(state.xrInstance, "/user/hand/right", &state.handSubactionPaths[1]);

    XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(asci.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE);
    std::strncpy(asci.localizedActionSetName, "Gameplay Actions", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    asci.priority = 0;
    if (XR_FAILED(xrCreateActionSet(state.xrInstance, &asci, &state.actionSet))) return false;

    auto createAction = [&](XrActionType type, const char* name, const char* locName, bool bothHands, XrAction& outAct) {
        XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
        aci.actionType = type;
        std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE);
        std::strncpy(aci.localizedActionName, locName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
        if (bothHands) {
            aci.countSubactionPaths = 2;
            aci.subactionPaths = state.handSubactionPaths;
        }
        xrCreateAction(state.actionSet, &aci, &outAct);
    };

    createAction(XR_ACTION_TYPE_POSE_INPUT,    "hand_pose",  "Hand Grip Pose", true,  state.handPoseAction);
    createAction(XR_ACTION_TYPE_VECTOR2F_INPUT,"thumbstick", "Thumbstick",     true,  state.thumbstickAction);
    createAction(XR_ACTION_TYPE_FLOAT_INPUT,   "trigger",    "Index Trigger",  true,  state.triggerAction);
    createAction(XR_ACTION_TYPE_FLOAT_INPUT,   "squeeze",    "Grip Squeeze",   true,  state.squeezeAction);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a",   "Button A",       false, state.buttonAAction);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b",   "Button B",       false, state.buttonBAction);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x",   "Button X",       false, state.buttonXAction);
    createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y",   "Button Y",       false, state.buttonYAction);

    auto bind = [&](XrAction act, const char* pathStr) -> XrActionSuggestedBinding {
        XrPath p = XR_NULL_PATH;
        xrStringToPath(state.xrInstance, pathStr, &p);
        return {act, p};
    };

    std::vector<XrActionSuggestedBinding> bindings = {
        bind(state.handPoseAction,   "/user/hand/left/input/grip/pose"),
        bind(state.handPoseAction,   "/user/hand/right/input/grip/pose"),
        bind(state.thumbstickAction, "/user/hand/left/input/thumbstick"),
        bind(state.thumbstickAction, "/user/hand/right/input/thumbstick"),
        bind(state.triggerAction,    "/user/hand/left/input/trigger/value"),
        bind(state.triggerAction,    "/user/hand/right/input/trigger/value"),
        bind(state.squeezeAction,    "/user/hand/left/input/squeeze/value"),
        bind(state.squeezeAction,    "/user/hand/right/input/squeeze/value"),
        bind(state.buttonAAction,    "/user/hand/right/input/a/click"),
        bind(state.buttonBAction,    "/user/hand/right/input/b/click"),
        bind(state.buttonXAction,    "/user/hand/left/input/x/click"),
        bind(state.buttonYAction,    "/user/hand/left/input/y/click")
    };

    XrPath touchProfile = XR_NULL_PATH;
    xrStringToPath(state.xrInstance, "/interaction_profiles/oculus/touch_controller", &touchProfile);
    XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested.interactionProfile = touchProfile;
    suggested.suggestedBindings = bindings.data();
    suggested.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
    xrSuggestInteractionProfileBindings(state.xrInstance, &suggested);

    for (int i = 0; i < 2; ++i) {
        XrActionSpaceCreateInfo asciSpace{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        asciSpace.action = state.handPoseAction;
        asciSpace.subactionPath = state.handSubactionPaths[i];
        asciSpace.poseInActionSpace.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
        xrCreateActionSpace(state.xrSession, &asciSpace, &state.handSpaces[i]);
    }

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &state.actionSet;
    xrAttachSessionActionSets(state.xrSession, &attachInfo);
    return true;
}

static bool initOpenXrRuntime(QuestVrApp& state) {
    PFN_xrInitializeLoaderKHR pfnInitializeLoaderKHR = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(
            XR_NULL_HANDLE, "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&pfnInitializeLoaderKHR))) &&
        pfnInitializeLoaderKHR != nullptr) {
        XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInfo.applicationVM = state.app->activity->vm;
        loaderInfo.applicationContext = state.app->activity->clazz;
        if (XR_FAILED(pfnInitializeLoaderKHR(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)))) {
            LOGE("xrInitializeLoaderKHR failed; falling back to simulator surface mode");
            return false;
        }
    }

    const char* enabledExts[] = {
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME
    };

    XrInstanceCreateInfoAndroidKHR androidCreateInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidCreateInfo.applicationVM = state.app->activity->vm;
    androidCreateInfo.applicationActivity = state.app->activity->clazz;

    XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
    ici.next = &androidCreateInfo;
    std::strncpy(ici.applicationInfo.applicationName, "Population Misfits", XR_MAX_APPLICATION_NAME_SIZE);
    ici.applicationInfo.applicationVersion = 100;
    std::strncpy(ici.applicationInfo.engineName, "MisfitHexVREngine", XR_MAX_ENGINE_NAME_SIZE);
    ici.applicationInfo.engineVersion = 100;
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ici.enabledExtensionCount = 2;
    ici.enabledExtensionNames = enabledExts;

    if (XR_FAILED(xrCreateInstance(&ici, &state.xrInstance))) {
        LOGI("OpenXR runtime not active on device; running in 2D/Simulator fallback mode");
        return false;
    }

    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(state.xrInstance, &sgi, &state.xrSystemId))) {
        return false;
    }

    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetReqs = nullptr;
    xrGetInstanceProcAddr(state.xrInstance, "xrGetOpenGLESGraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetReqs));
    if (pfnGetReqs) {
        XrGraphicsRequirementsOpenGLESKHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        pfnGetReqs(state.xrInstance, state.xrSystemId, &reqs);
    }

    XrGraphicsBindingOpenGLESAndroidKHR gfxBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    gfxBinding.display = state.eglDisplay;
    gfxBinding.config = state.eglConfig;
    gfxBinding.context = state.eglContext;

    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &gfxBinding;
    sci.systemId = state.xrSystemId;
    if (XR_FAILED(xrCreateSession(state.xrInstance, &sci, &state.xrSession))) {
        return false;
    }

    XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    xrCreateReferenceSpace(state.xrSession, &rsci, &state.xrLocalSpace);

    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    xrCreateReferenceSpace(state.xrSession, &rsci, &state.xrHeadSpace);

    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(state.xrInstance, state.xrSystemId,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    state.viewConfigs.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(state.xrInstance, state.xrSystemId,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount,
                                      state.viewConfigs.data());
    state.views.resize(viewCount, {XR_TYPE_VIEW});
    state.swapchains.resize(viewCount);

    for (uint32_t i = 0; i < viewCount; ++i) {
        SwapchainBuffer& sb = state.swapchains[i];
        sb.width = static_cast<int32_t>(state.viewConfigs[i].recommendedImageRectWidth);
        sb.height = static_cast<int32_t>(state.viewConfigs[i].recommendedImageRectHeight);

        XrSwapchainCreateInfo swci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swci.format = GL_SRGB8_ALPHA8;
        swci.sampleCount = 1;
        swci.width = sb.width;
        swci.height = sb.height;
        swci.faceCount = 1;
        swci.arraySize = 1;
        swci.mipCount = 1;
        xrCreateSwapchain(state.xrSession, &swci, &sb.handle);

        uint32_t imgCount = 0;
        xrEnumerateSwapchainImages(sb.handle, 0, &imgCount, nullptr);
        sb.images.resize(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        xrEnumerateSwapchainImages(sb.handle, imgCount, &imgCount,
                                   reinterpret_cast<XrSwapchainImageBaseHeader*>(sb.images.data()));
    }

    initOpenXrActions(state);
    state.openXrActive = true;
    LOGI("Population Misfits OpenXR 6DoF Stereo Runtime Initialized (%u views)", viewCount);
    return true;
}

static void pollOpenXrEventsAndActions(QuestVrApp& state, XrTime predictedDisplayTime) {
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(state.xrInstance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            state.xrSessionState = ssc->state;
            if (state.xrSessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
                sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(xrBeginSession(state.xrSession, &sbi))) {
                    state.sessionRunning = true;
                }
            } else if (state.xrSessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(state.xrSession);
                state.sessionRunning = false;
            }
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    if (!state.sessionRunning || predictedDisplayTime == 0) return;

    XrActiveActionSet activeSet{state.actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;
    xrSyncActions(state.xrSession, &syncInfo);

    // Head pose
    XrSpaceLocation headLoc{XR_TYPE_SPACE_LOCATION};
    if (XR_SUCCEEDED(xrLocateSpace(state.xrHeadSpace, state.xrLocalSpace, predictedDisplayTime, &headLoc))) {
        if (headLoc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            state.vrInput.headPos = {headLoc.pose.position.x, headLoc.pose.position.y, headLoc.pose.position.z};
        }
        if (headLoc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
            state.vrInput.headRot = {
                headLoc.pose.orientation.x,
                headLoc.pose.orientation.y,
                headLoc.pose.orientation.z,
                headLoc.pose.orientation.w
            };
        }
    }

    // Left & Right Touch Controllers
    for (int h = 0; h < 2; ++h) {
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(state.handSpaces[h], state.xrLocalSpace, predictedDisplayTime, &loc))) {
            Vec3 pos{loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
            Quat rot{loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z, loc.pose.orientation.w};
            if (h == 0) { state.vrInput.leftHandPos = pos; state.vrInput.leftHandRot = rot; }
            else        { state.vrInput.rightHandPos = pos; state.vrInput.rightHandRot = rot; }
        }

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.subactionPath = state.handSubactionPaths[h];

        getInfo.action = state.thumbstickAction;
        XrActionStateVector2f stickState{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(state.xrSession, &getInfo, &stickState))) {
            if (h == 0) state.vrInput.leftThumbstick = {stickState.currentState.x, stickState.currentState.y};
            else        state.vrInput.rightThumbstick = {stickState.currentState.x, stickState.currentState.y};
        }

        getInfo.action = state.triggerAction;
        XrActionStateFloat trigState{XR_TYPE_ACTION_STATE_FLOAT};
        if (XR_SUCCEEDED(xrGetActionStateFloat(state.xrSession, &getInfo, &trigState))) {
            if (h == 0) state.vrInput.leftTrigger = trigState.currentState;
            else        state.vrInput.rightTrigger = trigState.currentState;
        }

        getInfo.action = state.squeezeAction;
        XrActionStateFloat gripState{XR_TYPE_ACTION_STATE_FLOAT};
        if (XR_SUCCEEDED(xrGetActionStateFloat(state.xrSession, &getInfo, &gripState))) {
            if (h == 0) state.vrInput.leftGrip = gripState.currentState;
            else        state.vrInput.rightGrip = gripState.currentState;
        }
    }

    auto readBool = [&](XrAction act) -> bool {
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        gi.action = act;
        XrActionStateBoolean bs{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(state.xrSession, &gi, &bs))) {
            return bs.currentState == XR_TRUE;
        }
        return false;
    };

    state.vrInput.buttonAPressed = readBool(state.buttonAAction);
    state.vrInput.buttonBPressed = readBool(state.buttonBAction);
    state.vrInput.buttonXPressed = readBool(state.buttonXAction);
    state.vrInput.buttonYPressed = readBool(state.buttonYAction);
}

static void renderOpenXrStereoFrame(QuestVrApp& state, float dt) {
    pollOpenXrEventsAndActions(state, 0);
    if (!state.sessionRunning) return;

    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(state.xrSession, &waitInfo, &frameState))) return;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(state.xrSession, &beginInfo))) return;

    std::vector<XrCompositionLayerProjectionView> projViews;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<const XrCompositionLayerBaseHeader*> layers;

    if (frameState.shouldRender) {
        pollOpenXrEventsAndActions(state, frameState.predictedDisplayTime);
        state.battleEngine.tick(dt, state.vrInput);

        MeshData worldMesh, hudMesh;
        state.battleEngine.buildFrameGeometry(state.vrInput, worldMesh, hudMesh);

        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = state.xrLocalSpace;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCountOutput = 0;
        xrLocateViews(state.xrSession, &locateInfo, &viewState,
                      static_cast<uint32_t>(state.views.size()), &viewCountOutput, state.views.data());

        projViews.resize(viewCountOutput, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

        const CombatantState& player = state.battleEngine.getLocalPlayer();
        Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, player.yawRad);

        for (uint32_t i = 0; i < viewCountOutput; ++i) {
            SwapchainBuffer& sb = state.swapchains[i];
            uint32_t imgIdx = 0;
            XrSwapchainImageAcquireInfo acqInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrAcquireSwapchainImage(sb.handle, &acqInfo, &imgIdx);

            XrSwapchainImageWaitInfo swWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            swWaitInfo.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(sb.handle, &swWaitInfo);

            const XrView& v = state.views[i];
            Vec3 eyeLocalPos{v.pose.position.x, v.pose.position.y, v.pose.position.z};
            Quat eyeLocalRot{v.pose.orientation.x, v.pose.orientation.y, v.pose.orientation.z, v.pose.orientation.w};

            Vec3 eyeWorldPos = player.footPosition + playerYawQ.rotate(eyeLocalPos);
            Quat eyeWorldRot = playerYawQ * eyeLocalRot;

            Mat4 viewMat = Mat4::viewFromPose(eyeWorldPos, eyeWorldRot);
            Mat4 projMat = Mat4::perspectiveAsymmetric(
                v.fov.angleLeft, v.fov.angleRight, v.fov.angleUp, v.fov.angleDown, 0.08f, 150.0f
            );
            Mat4 viewProj = projMat * viewMat;

            Color4 skyCol = state.battleEngine.getEditorState().editorActive
                ? Color4{0.06f, 0.14f, 0.24f, 1.0f}
                : Color4{0.05f, 0.09f, 0.18f, 1.0f};

            state.renderer.beginEyeFrame(sb.width, sb.height, sb.images[imgIdx].image, skyCol);
            state.renderer.renderMesh(worldMesh, viewProj, eyeWorldPos, state.elapsedSec);
            state.renderer.renderMesh(hudMesh, viewProj, eyeWorldPos, state.elapsedSec);
            state.renderer.endEyeFrame();

            XrSwapchainImageReleaseInfo relInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(sb.handle, &relInfo);

            projViews[i].pose = v.pose;
            projViews[i].fov = v.fov;
            projViews[i].subImage.swapchain = sb.handle;
            projViews[i].subImage.imageRect.offset = {0, 0};
            projViews[i].subImage.imageRect.extent = {sb.width, sb.height};
        }

        layer.space = state.xrLocalSpace;
        layer.viewCount = viewCountOutput;
        layer.views = projViews.data();
        layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer));
    }

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = static_cast<uint32_t>(layers.size());
    endInfo.layers = layers.data();
    xrEndFrame(state.xrSession, &endInfo);
}

static void renderFallbackSimulatorFrame(QuestVrApp& state, float dt) {
    if (!state.windowReady || state.eglWindowSurface == EGL_NO_SURFACE) return;

    // Smooth demo patrol & combat input when running in 2D Android window fallback
    state.vrInput.leftThumbstick = {std::sin(state.elapsedSec * 0.5f) * 0.4f, 0.6f};
    state.vrInput.rightTrigger = (std::fmod(state.elapsedSec, 1.8f) < 0.35f) ? 0.9f : 0.0f;
    state.battleEngine.tick(dt, state.vrInput);

    MeshData worldMesh, hudMesh;
    state.battleEngine.buildFrameGeometry(state.vrInput, worldMesh, hudMesh);

    EGLint w = 1280, h = 720;
    eglQuerySurface(state.eglDisplay, state.eglWindowSurface, EGL_WIDTH, &w);
    eglQuerySurface(state.eglDisplay, state.eglWindowSurface, EGL_HEIGHT, &h);

    const CombatantState& player = state.battleEngine.getLocalPlayer();
    Quat playerYawQ = Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, player.yawRad);
    Vec3 camPos = player.footPosition + playerYawQ.rotate(state.vrInput.headPos);
    Mat4 viewMat = Mat4::viewFromPose(camPos, playerYawQ * state.vrInput.headRot);
    Mat4 projMat = Mat4::perspectiveAsymmetric(-0.8f, 0.8f, 0.5f, -0.5f, 0.1f, 150.0f);
    Mat4 vp = projMat * viewMat;

    state.renderer.beginEyeFrame(w, h, 0, {0.05f, 0.09f, 0.18f, 1.0f});
    state.renderer.renderMesh(worldMesh, vp, camPos, state.elapsedSec);
    state.renderer.renderMesh(hudMesh, vp, camPos, state.elapsedSec);
    state.renderer.endEyeFrame();

    eglSwapBuffers(state.eglDisplay, state.eglWindowSurface);
}

static void handleAppCmd(android_app* app, int32_t cmd) {
    auto* state = static_cast<QuestVrApp*>(app->userData);
    switch (cmd) {
        case APP_CMD_RESUME:
            state->resumed = true;
            break;
        case APP_CMD_PAUSE:
            state->resumed = false;
            break;
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr && state->eglDisplay != EGL_NO_DISPLAY) {
                state->eglWindowSurface = eglCreateWindowSurface(state->eglDisplay, state->eglConfig, app->window, nullptr);
                if (!state->openXrActive && state->eglWindowSurface != EGL_NO_SURFACE) {
                    eglMakeCurrent(state->eglDisplay, state->eglWindowSurface, state->eglWindowSurface, state->eglContext);
                }
                state->windowReady = true;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            state->windowReady = false;
            if (state->eglWindowSurface != EGL_NO_SURFACE) {
                eglDestroySurface(state->eglDisplay, state->eglWindowSurface);
                state->eglWindowSurface = EGL_NO_SURFACE;
            }
            break;
        default:
            break;
    }
}

} // namespace misfits

extern "C" JNIEXPORT void JNICALL android_main(struct android_app* app) {
    misfits::QuestVrApp state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = misfits::handleAppCmd;

    std::string internalDir = (app->activity && app->activity->internalDataPath)
        ? app->activity->internalDataPath
        : "/data/local/tmp";

    misfits::initEglContext(state);
    state.renderer.initialize();
    state.battleEngine.initialize(internalDir);
    misfits::initOpenXrRuntime(state);

    state.lastTimeSec = misfits::getMonotonicSeconds();

    while (app->destroyRequested == 0) {
        int events = 0;
        android_poll_source* source = nullptr;
        int timeoutMs = (state.resumed || state.sessionRunning) ? 0 : 250;

        while (ALooper_pollAll(timeoutMs, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0) break;
            timeoutMs = 0;
        }

        double now = misfits::getMonotonicSeconds();
        float dt = static_cast<float>(now - state.lastTimeSec);
        state.lastTimeSec = now;
        state.elapsedSec += dt;

        if (state.openXrActive) {
            misfits::renderOpenXrStereoFrame(state, dt);
        } else if (state.resumed && state.windowReady) {
            misfits::renderFallbackSimulatorFrame(state, dt);
        }
    }

    state.renderer.shutdown();
}
#endif // defined(__ANDROID__)
