#ifndef A_NATIVE_WINDOW_CREATOR_H // !A_NATIVE_WINDOW_CREATOR_H
#define A_NATIVE_WINDOW_CREATOR_H

#include <android/native_window.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <string>
#include <vector>

#define ResolveMethod(ClassName, MethodName, Handle, MethodSignature)                                                                    \
    ClassName##__##MethodName = reinterpret_cast<decltype(ClassName##__##MethodName)>(symbolMethod.Find(Handle, MethodSignature));       \
    if (nullptr == ClassName##__##MethodName)                                                                                            \
    {                                                                                                                                    \
        __android_log_print(ANDROID_LOG_ERROR, "ImGui", "[-] Method not found: %s -> %s::%s", MethodSignature, #ClassName, #MethodName); \
    }

namespace android {
    namespace detail {
        namespace ui {
            // A LayerStack identifies a Z-ordered group of layers. A layer can only be associated to a single
            // LayerStack, but a LayerStack can be associated to multiple displays, mirroring the same content.
            struct LayerStack
            {
                uint32_t id = UINT32_MAX;
            };

            enum class Rotation
            {
                Rotation0 = 0,
                Rotation90 = 1,
                Rotation180 = 2,
                Rotation270 = 3
            };

            // A simple value type representing a two-dimensional size.
            struct Size
            {
                int32_t width = -1;
                int32_t height = -1;
            };

            // Transactional state of physical or virtual display. Note that libgui defines
            // android::DisplayState as a superset of android::ui::DisplayState.
            struct DisplayState
            {
                LayerStack layerStack;
                Rotation orientation = Rotation::Rotation0;
                Size layerStackSpaceRect;
            };

            typedef int64_t nsecs_t; // nano-seconds
            struct DisplayInfo
            {
                uint32_t w{0};
                uint32_t h{0};
                float xdpi{0};
                float ydpi{0};
                float fps{0};
                float density{0};
                uint8_t orientation{0};
                bool secure{false};
                nsecs_t appVsyncOffset{0};
                nsecs_t presentationDeadline{0};
                uint32_t viewportW{0};
                uint32_t viewportH{0};
            };

            enum class DisplayType
            {
                DisplayIdMain = 0,
                DisplayIdHdmi = 1
            };

            struct PhysicalDisplayId
            {
                uint64_t value;
            };
        }

        struct String8;

        struct LayerMetadata;

        struct Surface;

        struct SurfaceControl;

        struct SurfaceComposerClientTransaction;

        struct SurfaceComposerClient;

        template <typename any_t>
        struct StrongPointer
        {
            union
            {
                any_t *pointer;
                char padding[sizeof(std::max_align_t)];
            };

            inline any_t *operator->() const { return pointer; }
            inline any_t *get() const { return pointer; }
            inline explicit operator bool() const { return nullptr != pointer; }
        };

        struct Functionals
        {
            struct SymbolMethod
            {
                void *(*Open)(const char *filename, int flag) = nullptr;
                void *(*Find)(void *handle, const char *symbol) = nullptr;
                int (*Close)(void *handle) = nullptr;
            };

            int systemVersion = 0;
            int systemSdk = 0;
            bool libguiOpened = false;
            bool libutilsOpened = false;

            void (*RefBase__IncStrong)(void *thiz, void *id) = nullptr;
            void (*RefBase__DecStrong)(void *thiz, void *id) = nullptr;

            void (*String8__Constructor)(void *thiz, const char *const data) = nullptr;
            void (*String8__Destructor)(void *thiz) = nullptr;

            void (*LayerMetadata__Constructor)(void *thiz) = nullptr;
            void (*LayerMetadata__setInt32)(void *thiz, uint32_t key, int32_t value) = nullptr;

            void (*SurfaceComposerClient__Constructor)(void *thiz) = nullptr;
            void (*SurfaceComposerClient__Destructor)(void *thiz) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__CreateSurface)(void *thiz, void *name, uint32_t w, uint32_t h, int32_t format, int32_t flags, void *parentHandle, void *layerMetadata, uint32_t *outTransformHint) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__CreateSurface_and9)(void *thiz, void *name, uint32_t w, uint32_t h, int32_t format, uint32_t flags, void *parentHandle, int32_t windowType, int32_t ownerUid) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetInternalDisplayToken)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetBuiltInDisplay)(ui::DisplayType type) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayState)(StrongPointer<void> &display, ui::DisplayState *displayState) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayInfo)(StrongPointer<void> &display, ui::DisplayInfo *displayInfo) = nullptr;
            std::vector<ui::PhysicalDisplayId> (*SurfaceComposerClient__GetPhysicalDisplayIds)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetPhysicalDisplayToken)(ui::PhysicalDisplayId displayId) = nullptr;

            void (*SurfaceComposerClient__Transaction__Constructor)(void *thiz) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetLayer)(void *thiz, StrongPointer<void> &surfaceControl, int32_t z) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetTrustedOverlay)(void *thiz, StrongPointer<void> &surfaceControl, bool isTrustedOverlay) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__Apply)(void *thiz, bool synchronous, bool oneWay) = nullptr;

            int32_t (*SurfaceControl__Validate)(void *thiz) = nullptr;
            StrongPointer<Surface> (*SurfaceControl__GetSurface)(void *thiz) = nullptr;
            void (*SurfaceControl__DisConnect)(void *thiz) = nullptr;

            static int readPropertyInt(const char *key, int fallback = 0) {
                char value[PROP_VALUE_MAX]{0};
                __system_property_get(key, value);
                if (value[0] == '\0')
                    return fallback;

                char *end = nullptr;
                long parsed = std::strtol(value, &end, 10);
                if (end == value)
                    return fallback;
                return static_cast<int>(parsed);
            }

            static int releaseFromSdk(int sdk) {
                if (sdk >= 37) return 17;
                switch (sdk) {
                    case 36: return 16;
                    case 35: return 15;
                    case 34: return 14;
                    case 33: return 13;
                    case 32:
                    case 31: return 12;
                    case 30: return 11;
                    case 29: return 10;
                    case 28: return 9;
                    default: return 0;
                }
            }

            Functionals(const SymbolMethod &symbolMethod)
            {
                systemSdk = readPropertyInt("ro.build.version.sdk");
                systemVersion = readPropertyInt("ro.build.version.release");
                if (systemVersion <= 0)
                    systemVersion = releaseFromSdk(systemSdk);

                if (systemVersion < 9)
                {
                    __android_log_print(ANDROID_LOG_ERROR, "ImGui",
                                        "[-] Unsupported system version: release=%d sdk=%d",
                                        systemVersion, systemSdk);
                    return;
                }

                static std::unordered_map<size_t, std::unordered_map<void **, const char *>> patchesTable = {
                    {
                        // Android 17 (API 37) changed LayerMetadata from by-value to
                        // const-reference, so the createSurface mangled name differs.
                        17,
                        {
                            {reinterpret_cast<void **>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEERKNS_3gui13LayerMetadataEPj"},
                         },
                    },
                    {
                        16,
                        {
                            {reinterpret_cast<void **>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj"},
                         },
                    },
                    {
                        15,
                        {
                            {reinterpret_cast<void **>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj"},
                         },
                    },
                    {
                        14,
                        {
                            {reinterpret_cast<void **>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj"},
                        },
                    },
                    {
                        12,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__Transaction__Apply), "_ZN7android21SurfaceComposerClient11Transaction5applyEb"},
                        },
                    },
                    {
                        11,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataEPj"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                    {
                        10,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataE"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                    {
                        9,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface_and9), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlEii"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__GetBuiltInDisplay), "_ZN7android21SurfaceComposerClient17getBuiltInDisplayEi"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                };

#ifdef __LP64__
                auto libgui = symbolMethod.Open("/system/lib64/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib64/libutils.so", RTLD_LAZY);
#else
                auto libgui = symbolMethod.Open("/system/lib/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib/libutils.so", RTLD_LAZY);
#endif
                libguiOpened = libgui != nullptr;
                libutilsOpened = libutils != nullptr;
                if (!libgui || !libutils) {
                    __android_log_print(ANDROID_LOG_ERROR, "ImGui",
                                        "[-] Failed to open libgui/libutils (libgui=%p, libutils=%p)",
                                        libgui, libutils);
                    return;
                }

                ResolveMethod(RefBase, IncStrong, libutils, "_ZNK7android7RefBase9incStrongEPKv");
                ResolveMethod(RefBase, DecStrong, libutils, "_ZNK7android7RefBase9decStrongEPKv");

                ResolveMethod(String8, Constructor, libutils, "_ZN7android7String8C2EPKc");
                ResolveMethod(String8, Destructor, libutils, "_ZN7android7String8D2Ev");

                // Android 10-13 keeps LayerMetadata in android::; 14+ moved it to
                // android::gui and the version patches below override these.
                ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android13LayerMetadataC2Ev");
                ResolveMethod(LayerMetadata, setInt32, libgui, "_ZN7android13LayerMetadata8setInt32Eji");


                ResolveMethod(SurfaceComposerClient, Constructor, libgui, "_ZN7android21SurfaceComposerClientC2Ev");
                ResolveMethod(SurfaceComposerClient, CreateSurface, libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijRKNS_2spINS_7IBinderEEENS_13LayerMetadataEPj");
                ResolveMethod(SurfaceComposerClient, GetInternalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getInternalDisplayTokenEv");  //小于或者等于安卓13
                ResolveMethod(SurfaceComposerClient, GetDisplayState, libgui, "_ZN7android21SurfaceComposerClient15getDisplayStateERKNS_2spINS_7IBinderEEEPNS_2ui12DisplayStateE");
                ResolveMethod(SurfaceComposerClient, GetDisplayInfo, libgui, "_ZN7android21SurfaceComposerClient14getDisplayInfoERKNS_2spINS_7IBinderEEEPNS_11DisplayInfoE"); //安卓10及以下
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayIds, libgui, "_ZN7android21SurfaceComposerClient21getPhysicalDisplayIdsEv");
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getPhysicalDisplayTokenENS_17PhysicalDisplayIdE");

                ResolveMethod(SurfaceComposerClient__Transaction, Constructor, libgui, "_ZN7android21SurfaceComposerClient11TransactionC2Ev");
                ResolveMethod(SurfaceComposerClient__Transaction, SetLayer, libgui, "_ZN7android21SurfaceComposerClient11Transaction8setLayerERKNS_2spINS_14SurfaceControlEEEi");
                ResolveMethod(SurfaceComposerClient__Transaction, SetTrustedOverlay, libgui, "_ZN7android21SurfaceComposerClient11Transaction17setTrustedOverlayERKNS_2spINS_14SurfaceControlEEEb");
                ResolveMethod(SurfaceComposerClient__Transaction, Apply, libgui, "_ZN7android21SurfaceComposerClient11Transaction5applyEbb");

                ResolveMethod(SurfaceControl, Validate, libgui, "_ZNK7android14SurfaceControl8validateEv");
                ResolveMethod(SurfaceControl, GetSurface, libgui, "_ZN7android14SurfaceControl10getSurfaceEv");
                ResolveMethod(SurfaceControl, DisConnect, libgui, "_ZN7android14SurfaceControl10disconnectEv");
                
                auto it = patchesTable.find(systemVersion);
                if (it != patchesTable.end()) {
                    for (const auto &[patchTo, signature] : patchesTable.at(systemVersion))
                    {
                        *patchTo = symbolMethod.Find(libgui, signature);
                        if (nullptr != *patchTo)
                            continue;

                        __android_log_print(ANDROID_LOG_ERROR, "ImGui", "[-] Patch method not found: %s", signature);
                    }
                }

                // Newer releases can keep the Android 17 const-reference ABI even when the
                // release number is unknown to this table. Prefer it over the legacy symbol.
                if (nullptr == SurfaceComposerClient__CreateSurface) {
                    auto method = symbolMethod.Find(libgui,
                            "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEERKNS_3gui13LayerMetadataEPj");
                    if (nullptr != method) {
                        SurfaceComposerClient__CreateSurface =
                                reinterpret_cast<decltype(SurfaceComposerClient__CreateSurface)>(method);
                        __android_log_print(ANDROID_LOG_INFO, "ImGui",
                                            "[+] Using Android 17+ createSurface ABI");
                    }
                }
                if (nullptr == SurfaceComposerClient__CreateSurface) {
                    auto method = symbolMethod.Find(libgui,
                            "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj");
                    if (nullptr != method) {
                        SurfaceComposerClient__CreateSurface =
                                reinterpret_cast<decltype(SurfaceComposerClient__CreateSurface)>(method);
                        __android_log_print(ANDROID_LOG_INFO, "ImGui",
                                            "[+] Using Android 14-16 createSurface ABI");
                    }
                }

                // Keep both handles open: the resolved function pointers are used for the
                // lifetime of the process, and dlclose() may unload a privately loaded lib.
            }

            bool valid() const {
                if (!RefBase__IncStrong || !RefBase__DecStrong ||
                    !String8__Constructor || !String8__Destructor ||
                    !SurfaceComposerClient__Constructor ||
                    !SurfaceComposerClient__CreateSurface ||
                    !SurfaceControl__GetSurface || !SurfaceControl__DisConnect) {
                    return false;
                }
                if (systemVersion >= 10 && !LayerMetadata__Constructor)
                    return false;
                if ((systemVersion == 10 || systemVersion == 11) && !LayerMetadata__setInt32)
                    return false;
                if (systemVersion >= 11 && !SurfaceComposerClient__GetDisplayState)
                    return false;
                if (systemVersion >= 12 &&
                    (!SurfaceComposerClient__Transaction__Constructor ||
                     !SurfaceComposerClient__Transaction__SetTrustedOverlay ||
                     !SurfaceComposerClient__Transaction__Apply))
                    return false;
                if (systemVersion >= 14 &&
                    (!SurfaceComposerClient__GetPhysicalDisplayIds ||
                     !SurfaceComposerClient__GetPhysicalDisplayToken))
                    return false;
                if (systemVersion <= 10 && !SurfaceComposerClient__GetDisplayInfo)
                    return false;
                if (systemVersion == 9 && !SurfaceComposerClient__GetBuiltInDisplay)
                    return false;
                if (systemVersion >= 10 && systemVersion <= 13 &&
                    !SurfaceComposerClient__GetInternalDisplayToken)
                    return false;
                return true;
            }

            template <typename T>
            static void reportSymbol(const char *name, T fn) {
                std::printf("  %-44s %s\n", name, fn ? "ok" : "MISSING");
            }

            void logStatus() const {
                std::printf("[Surface] Android release=%d sdk=%d libgui=%s libutils=%s\n",
                            systemVersion, systemSdk,
                            libguiOpened ? "ok" : "MISSING",
                            libutilsOpened ? "ok" : "MISSING");
                reportSymbol("RefBase::incStrong", RefBase__IncStrong);
                reportSymbol("RefBase::decStrong", RefBase__DecStrong);
                reportSymbol("String8::String8", String8__Constructor);
                reportSymbol("String8::~String8", String8__Destructor);
                reportSymbol("LayerMetadata::LayerMetadata", LayerMetadata__Constructor);
                if (nullptr != LayerMetadata__setInt32) {
                    reportSymbol("LayerMetadata::setInt32", LayerMetadata__setInt32);
                } else {
                    std::printf("  %-44s %s\n", "LayerMetadata::setInt32",
                                systemVersion <= 11 ? "MISSING" : "n/a (only needed on 10-11)");
                }
                reportSymbol("SurfaceComposerClient::SurfaceComposerClient", SurfaceComposerClient__Constructor);
                reportSymbol("SurfaceComposerClient::createSurface", SurfaceComposerClient__CreateSurface);
                reportSymbol("SurfaceComposerClient::getDisplayState", SurfaceComposerClient__GetDisplayState);
                reportSymbol("SurfaceComposerClient::getPhysicalDisplayIds", SurfaceComposerClient__GetPhysicalDisplayIds);
                reportSymbol("SurfaceComposerClient::getPhysicalDisplayToken", SurfaceComposerClient__GetPhysicalDisplayToken);
                reportSymbol("Transaction::Transaction", SurfaceComposerClient__Transaction__Constructor);
                reportSymbol("Transaction::setLayer", SurfaceComposerClient__Transaction__SetLayer);
                reportSymbol("Transaction::setTrustedOverlay", SurfaceComposerClient__Transaction__SetTrustedOverlay);
                reportSymbol("Transaction::apply", SurfaceComposerClient__Transaction__Apply);
                reportSymbol("SurfaceControl::validate", SurfaceControl__Validate);
                reportSymbol("SurfaceControl::getSurface", SurfaceControl__GetSurface);
                reportSymbol("SurfaceControl::disconnect", SurfaceControl__DisConnect);
            }

            static const Functionals &GetInstance(const SymbolMethod &symbolMethod = {.Open = dlopen, .Find = dlsym, .Close = dlclose}) {
                static Functionals functionals(symbolMethod);
                return functionals;
            }
        };

        struct String8
        {
            alignas(std::max_align_t) char data[1024];

            String8(const char *const string)
            {
                Functionals::GetInstance().String8__Constructor(data, string);
            }

            ~String8()
            {
                Functionals::GetInstance().String8__Destructor(data);
            }

            operator void *()
            {
                return reinterpret_cast<void *>(data);
            }
        };

        struct LayerMetadata {
            alignas(std::max_align_t) char data[1024];

            LayerMetadata() {
                if (9 < Functionals::GetInstance().systemVersion) {
                    Functionals::GetInstance().LayerMetadata__Constructor(data);
                }
            }
            
            void setInt32(uint32_t key, int32_t value) {
                Functionals::GetInstance().LayerMetadata__setInt32(data, key, value);            
            }
            
            operator void *() {
                if (9 < Functionals::GetInstance().systemVersion)
                    return reinterpret_cast<void *>(data);
                else
                    return nullptr;
            }
        };

        struct Surface {
        };

        struct SurfaceControl {
            void *data;

            SurfaceControl() : data(nullptr) {}
            SurfaceControl(void *data) : data(data) {}

            int32_t Validate() {
                auto &f = Functionals::GetInstance();
                if (nullptr == data || nullptr == f.SurfaceControl__Validate)
                    return 0;

                return f.SurfaceControl__Validate(data);
            }

            Surface *GetSurface() {
                auto &f = Functionals::GetInstance();
                if (nullptr == data || nullptr == f.SurfaceControl__GetSurface)
                    return nullptr;

                auto result = f.SurfaceControl__GetSurface(data);
                if (nullptr == result.pointer)
                    return nullptr;

                return reinterpret_cast<Surface *>(reinterpret_cast<size_t>(result.pointer) + sizeof(std::max_align_t) / 2);
            }

            void DisConnect() {
                auto &f = Functionals::GetInstance();
                if (nullptr == data || nullptr == f.SurfaceControl__DisConnect)
                    return;

                f.SurfaceControl__DisConnect(data);
            }

            void DestroySurface(Surface *surface) {
                auto &f = Functionals::GetInstance();
                if (nullptr == data || nullptr == surface)
                    return;

                if (nullptr != f.RefBase__DecStrong)
                    f.RefBase__DecStrong(reinterpret_cast<Surface *>(reinterpret_cast<size_t>(surface) - sizeof(std::max_align_t) / 2), this);
                DisConnect();
                if (nullptr != f.RefBase__DecStrong)
                    f.RefBase__DecStrong(data, this);
            }
        };

        struct SurfaceComposerClientTransaction {
            // Transaction keeps growing between releases; 1024 bytes was already tight
            // on Android 14-16. Keep generous headroom and proper alignment.
            alignas(std::max_align_t) char data[8192];

            SurfaceComposerClientTransaction() {
                auto &f = Functionals::GetInstance();
                if (nullptr != f.SurfaceComposerClient__Transaction__Constructor)
                    f.SurfaceComposerClient__Transaction__Constructor(data);
            }

            void *SetLayer(StrongPointer<void> &surfaceControl, int32_t z) {
                auto &f = Functionals::GetInstance();
                if (nullptr == f.SurfaceComposerClient__Transaction__SetLayer)
                    return nullptr;
                return f.SurfaceComposerClient__Transaction__SetLayer(data, surfaceControl, z);
            }

            void *SetTrustedOverlay(StrongPointer<void> &surfaceControl, bool isTrustedOverlay) {
                auto &f = Functionals::GetInstance();
                if (nullptr == f.SurfaceComposerClient__Transaction__SetTrustedOverlay)
                    return nullptr;
                return f.SurfaceComposerClient__Transaction__SetTrustedOverlay(data, surfaceControl, isTrustedOverlay);
            }

            int32_t Apply(bool synchronous, bool oneWay) {
                auto &f = Functionals::GetInstance();
                if (nullptr == f.SurfaceComposerClient__Transaction__Apply)
                    return -1;
                if (12 >= f.systemVersion)
                    return reinterpret_cast<int32_t (*)(void *, bool)>(f.SurfaceComposerClient__Transaction__Apply)(data, synchronous);
                else
                    return f.SurfaceComposerClient__Transaction__Apply(data, synchronous, oneWay);
            }
        };

        struct SurfaceComposerClient {
            alignas(std::max_align_t) char data[4096];

            SurfaceComposerClient() {
                auto &f = Functionals::GetInstance();
                if (nullptr != f.SurfaceComposerClient__Constructor)
                    f.SurfaceComposerClient__Constructor(data);
                if (nullptr != f.RefBase__IncStrong)
                    f.RefBase__IncStrong(data, this);
            }

            SurfaceControl CreateSurface(const char *name, int32_t width, int32_t height, bool skipScrenshot) {
                auto &f = Functionals::GetInstance();
                if (nullptr == f.SurfaceComposerClient__CreateSurface &&
                    nullptr == f.SurfaceComposerClient__CreateSurface_and9)
                    return {};

                void *parentHandle = nullptr;
                String8 windowName(name);
                LayerMetadata layerMetadata;
                if (skipScrenshot && (f.systemVersion == 10 || f.systemVersion == 11)) {
                    layerMetadata.setInt32(2u, 441731);
                }
                int32_t flags = 0;
                if (skipScrenshot && f.systemVersion >= 12) {
                    flags |= 0x40;
                }
                
                if (12 <= f.systemVersion) {
                    static void *fakeParentHandleForBinder = nullptr;
                    parentHandle = &fakeParentHandleForBinder;
                }
                                
                StrongPointer<void> result;
                if (f.systemVersion == 9) {
                    if (nullptr == f.SurfaceComposerClient__CreateSurface_and9)
                        return {};
                    int32_t windowType = -1;
                    int32_t ownerUid = -1;
                    if (skipScrenshot) {
                        windowType = 441731;                    
                    } 
                    result = f.SurfaceComposerClient__CreateSurface_and9(data, windowName, width, height, 1, flags, parentHandle, windowType, ownerUid);                
                } else if (f.systemVersion >= 10) {
                    if (nullptr == f.SurfaceComposerClient__CreateSurface)
                        return {};
                    result = f.SurfaceComposerClient__CreateSurface(data, windowName, width, height, 1, flags, parentHandle, layerMetadata, nullptr);
                }

                if (nullptr == result.pointer)
                    return {};
                
                if (12 <= f.systemVersion) {
                    static SurfaceComposerClientTransaction transaction;
                    transaction.SetTrustedOverlay(result, true);
                    transaction.Apply(false, true);
                }
                return {result.get()};
            }

            bool GetDisplayInfo(ui::DisplayState *displayInfo) {
                auto &f = Functionals::GetInstance();
                static StrongPointer<void> defaultDisplayToken{};

                if (defaultDisplayToken.get() == nullptr) {
                    if (9 >= f.systemVersion) { //小于或者等于安卓9
                        if (nullptr == f.SurfaceComposerClient__GetBuiltInDisplay)
                            return false;
                        defaultDisplayToken = f.SurfaceComposerClient__GetBuiltInDisplay(ui::DisplayType::DisplayIdMain);
                    } else {
                        if (14 > f.systemVersion) {//小于或者等于安卓13
                            if (nullptr == f.SurfaceComposerClient__GetInternalDisplayToken)
                                return false;
                            defaultDisplayToken = f.SurfaceComposerClient__GetInternalDisplayToken();
                        } else { //安卓14及以上
                            if (nullptr == f.SurfaceComposerClient__GetPhysicalDisplayIds ||
                                nullptr == f.SurfaceComposerClient__GetPhysicalDisplayToken)
                                return false;
                            auto displayIds = f.SurfaceComposerClient__GetPhysicalDisplayIds();
                            if (displayIds.empty())
                                return false;

                            defaultDisplayToken = f.SurfaceComposerClient__GetPhysicalDisplayToken(displayIds[0]);
                        }
                    }
                }

                if (nullptr == defaultDisplayToken.get())
                    return false;

                if (11 <= f.systemVersion) { //大于或者等于安卓11
                    if (nullptr == f.SurfaceComposerClient__GetDisplayState)
                        return false;
                    return 0 == f.SurfaceComposerClient__GetDisplayState(defaultDisplayToken, displayInfo);
                } else { //安卓10及以下
                    if (nullptr == f.SurfaceComposerClient__GetDisplayInfo)
                        return false;
                    ui::DisplayInfo realDisplayInfo{};
                    if (0 != f.SurfaceComposerClient__GetDisplayInfo(defaultDisplayToken, &realDisplayInfo))
                        return false;

                    displayInfo->layerStackSpaceRect.width = realDisplayInfo.w;
                    displayInfo->layerStackSpaceRect.height = realDisplayInfo.h;
                    displayInfo->orientation = static_cast<ui::Rotation>(realDisplayInfo.orientation);

                    return true;
                }
            }
        };

    }

    class ANativeWindowCreator {
    public:
        struct DisplayInfo {
            int32_t orientation;
            int32_t width;
            int32_t height;
        };

    public:
        static detail::SurfaceComposerClient &GetComposerInstance() {
            static detail::SurfaceComposerClient surfaceComposerClient;
            return surfaceComposerClient;
        }

        static DisplayInfo GetDisplayInfo() {
            auto &surfaceComposerClient = GetComposerInstance();
            detail::ui::DisplayState displayInfo{};

            if (!surfaceComposerClient.GetDisplayInfo(&displayInfo))
                return {};
            
            DisplayInfo local_displayInfo{0};   
            int32_t local_orientation = static_cast<int32_t>(displayInfo.orientation);  
            int32_t local_abs_x = (displayInfo.layerStackSpaceRect.width > displayInfo.layerStackSpaceRect.height ? displayInfo.layerStackSpaceRect.width : displayInfo.layerStackSpaceRect.height);
            int32_t local_abs_y = (displayInfo.layerStackSpaceRect.width < displayInfo.layerStackSpaceRect.height ? displayInfo.layerStackSpaceRect.width : displayInfo.layerStackSpaceRect.height);          
            if (local_orientation == 1 || local_orientation == 3) {
                local_displayInfo.width = local_abs_x;
                local_displayInfo.height = local_abs_y;
            } else {
                local_displayInfo.width = local_abs_y;
                local_displayInfo.height = local_abs_x;
            }
            local_displayInfo.orientation = local_orientation;
            return local_displayInfo;
        }

        static ANativeWindow *Create(const char *name, int32_t width = -1, int32_t height = -1, bool skipScrenshot_ = false) {
            auto &functionals = detail::Functionals::GetInstance();
            if (!functionals.valid()) {
                __android_log_print(ANDROID_LOG_ERROR, "ImGui",
                                    "[-] SurfaceFlinger ABI is not available on this build");
                return nullptr;
            }

            auto &surfaceComposerClient = GetComposerInstance();
            while (-1 == width || -1 == height) {
                detail::ui::DisplayState displayInfo{};

                if (!surfaceComposerClient.GetDisplayInfo(&displayInfo))
                    break;

                width = displayInfo.layerStackSpaceRect.width;
                height = displayInfo.layerStackSpaceRect.height;

                break;
            }

            auto surfaceControl = surfaceComposerClient.CreateSurface(name, width, height, skipScrenshot_);
            auto nativeWindow = reinterpret_cast<ANativeWindow *>(surfaceControl.GetSurface());
            if (nullptr == nativeWindow) {
                __android_log_print(ANDROID_LOG_ERROR, "ImGui",
                                    "[-] Failed to obtain ANativeWindow from SurfaceControl");
                return nullptr;
            }

            m_cachedSurfaceControl.emplace(nativeWindow, std::move(surfaceControl));
            return nativeWindow;
        }

        static void Destroy(ANativeWindow *nativeWindow) {
            if (nullptr == nativeWindow)
                return;

            auto it = m_cachedSurfaceControl.find(nativeWindow);
            if (it == m_cachedSurfaceControl.end())
                return;

            m_cachedSurfaceControl[nativeWindow].DestroySurface(reinterpret_cast<detail::Surface *>(nativeWindow));
            m_cachedSurfaceControl.erase(nativeWindow);
        }

        // Prints the resolved SurfaceFlinger ABI and exercises createSurface/destroy.
        // Run `sk-imgui-ui --diagnose` on the target device; no Vulkan/UI is needed.
        static bool SelfCheck() {
            auto &functionals = detail::Functionals::GetInstance();
            functionals.logStatus();

            bool ok = functionals.valid();
            std::printf("[Surface] ABI: %s\n", ok ? "ok" : "INCOMPLETE");
            if (!ok) {
                std::fflush(stdout);
                return false;
            }

            auto info = GetDisplayInfo();
            std::printf("[Surface] display: %dx%d orientation=%d\n",
                        info.width, info.height, info.orientation);

            ANativeWindow *probe = Create("surface_probe", 64, 64, false);
            if (nullptr == probe) {
                std::printf("[Surface] createSurface: FAILED\n");
                std::fflush(stdout);
                return false;
            }

            std::printf("[Surface] createSurface: ok (%dx%d)\n",
                        ANativeWindow_getWidth(probe), ANativeWindow_getHeight(probe));
            Destroy(probe);
            std::printf("[Surface] destroySurface: ok\n");
            std::fflush(stdout);
            return true;
        }

    private:
        inline static std::unordered_map<ANativeWindow *, detail::SurfaceControl> m_cachedSurfaceControl;
    };
}

#undef ResolveMethod

#endif // !A_NATIVE_WINDOW_CREATOR_H
