// SPDX-License-Identifier: MIT
// Copyright (c) 2026 shohei-s.com

#pragma once

// Abyssograph add-on SDK. This header is the only one an add-on requires.
// See docs/api-reference.en.md for the full specification.

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
  #define ABYSSO_ADDON_API extern "C" __declspec(dllexport)
#else
  #define ABYSSO_ADDON_API extern "C" __attribute__((visibility("default")))
#endif

// ABI version. The host does not load an add-on that reports a different value.
#define ABYSSO_ADDON_ABI_VERSION 2

typedef struct { uint8_t bytes[16]; } AbyssoGuid;

// Item types. The host builds its UI from this value alone.
enum
{
    kAbyssoParamItemTypeGroup = 0,      // Container; holds no value
    kAbyssoParamItemTypeComboBox,       // Value is the GUID of the selected ComboBoxItem
    kAbyssoParamItemTypeComboBoxItem,   // The parent is always a ComboBox
    kAbyssoParamItemTypeCheckBox,       // Value is an int32_t (0 / 1)
    kAbyssoParamItemTypeStatic,         // Caption and value (read-only string)
    kAbyssoParamItemTypeDescription,    // Explanatory text
    kAbyssoParamItemTypeInt32,          // Slider
    kAbyssoParamItemTypeDouble,         // Slider
    kAbyssoParamItemTypeString,         // Text box
};

// Item properties, passed as the prop argument of GetAbyssoAddonProp.
// The type written to data depends on the item type.
//   Type         ... int32_t
//   Caption      ... const char* (write the pointer; the string is not copied)
//   Description  ... const char*
//   CurrentValue ... AbyssoGuid for ComboBox / int32_t for CheckBox and Int32 /
//                    double for Double / const char* for String and Static
//   DefaultValue ... same type as CurrentValue
//   Min/Max/Step ... int32_t for Int32, double for Double.
//                    For String, Max is the maximum character count
//
// Returned strings remain valid until the next call to SetAbyssoAddonLang.
enum
{
    kAbyssoPropType = 0,
    kAbyssoPropCaption,
    kAbyssoPropCurrentValue,
    kAbyssoPropDefaultValue,
    kAbyssoPropMinValue,
    kAbyssoPropMaxValue,
    kAbyssoPropStep,        // Slider step; 0 disables snapping
    kAbyssoPropDescription,
};

// Change flags returned by SetAbyssoAddonValue and ResetAbyssoAddonValue.
#define ABYSSO_CHANGE_NONE   0x0
#define ABYSSO_CHANGE_REDRAW 0x1 // Redraw the image
#define ABYSSO_CHANGE_PROPS  0x2 // Re-read item properties (controls are not recreated)
#define ABYSSO_CHANGE_TREE   0x4 // Re-enumerate the tree and rebuild the panel

// Error codes. Zero or positive indicates success; negative indicates an error.
#define ABYSSO_ERROR_INVALID_ARG      -1
#define ABYSSO_ERROR_UNKNOWN_GUID     -2
#define ABYSSO_ERROR_BUFFER_TOO_SMALL -3
#define ABYSSO_ERROR_READONLY         -4
#define ABYSSO_ERROR_NOT_SUPPORTED    -5

// A single parameter item. structSize is set by the caller (the host).
typedef struct
{
    int32_t    structSize;
    int32_t    type;       // kAbyssoParamItemType*
    AbyssoGuid guid;
    int32_t    isReadOnly; // Non-zero: the value cannot be changed (it is still displayed)
    int32_t    isEnabled;  // Zero: shown as disabled. Do not reject value requests
} AbyssoParamItem;

// Add-on identification. name uses the language set by SetAbyssoAddonLang.
typedef struct
{
    int32_t     structSize;
    AbyssoGuid  guid;
    const char* name;
    const char* version;
} AbyssoAddonInfo;

// Data passed from the host during initialization.
typedef struct
{
    int32_t     structSize;
    int32_t     abiVersion; // ABYSSO_ADDON_ABI_VERSION
    const char* hostVersion;
} AbyssoHostInfo;

// ---------------------------------------------------------------------------
// Exported functions
//
// Only GetAbyssoAddonAbiVersion, GetAbyssoAddonInfo and RenderAbyssoAddonFrame
// are required. All others are optional; when one is not exported, the host
// treats the corresponding feature as unsupported. Functions that have to be
// implemented as a group are listed in the reference.
// ---------------------------------------------------------------------------

// Required. The first function the host calls, before initialization.
ABYSSO_ADDON_API int32_t GetAbyssoAddonAbiVersion(void);

// Optional. Return zero or a positive value on success; a negative value marks
// the add-on as unavailable. When absent, no initialization is required.
ABYSSO_ADDON_API int32_t InitializeAbyssoAddon(const AbyssoHostInfo* host);
// Optional. When absent, no cleanup is required.
ABYSSO_ADDON_API void    ShutdownAbyssoAddon(void);

// Display language as a BCP 47 tag; the default is "en-US".
// Optional. When absent, language switching is treated as unsupported.
ABYSSO_ADDON_API void    SetAbyssoAddonLang(const char* bcp47);

// Required. The GUID identifies the add-on for routing and in project files.
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfo(AbyssoAddonInfo* out);

// Item tree. A nullptr target means the root of this add-on.
// Optional. When absent, the child count is treated as 0
// (no items of this add-on appear in the parameter panel).
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItemCount(const AbyssoGuid* target);
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItem(const AbyssoGuid* target, int32_t index,
                                                      AbyssoParamItem* out);

// Retrieves an item property. On success, returns the number of bytes written.
// Optional, but required as soon as a single item is published.
ABYSSO_ADDON_API int32_t GetAbyssoAddonProp(const AbyssoGuid* target, int32_t prop,
                                            void* data, int32_t dataSize);
// Sets an item value. On success, returns a combination of ABYSSO_CHANGE_* flags.
// Optional. When absent, the host treats every item as read-only.
ABYSSO_ADDON_API int32_t SetAbyssoAddonValue(const AbyssoGuid* target,
                                             const void* data, int32_t dataSize);

// A nullptr target resets every item to its default value.
// Optional. When absent, the call is treated as returning ABYSSO_CHANGE_NONE.
ABYSSO_ADDON_API int32_t ResetAbyssoAddonValue(const AbyssoGuid* target);

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

#define ABYSSO_RENDER_LIVE 0x1 // A frame during playback (0 while stopped)

// A render request for one frame. structSize is set by the caller (the host).
// Fields are only ever appended; do not access data beyond structSize.
typedef struct
{
    int32_t structSize;

    int32_t width;
    int32_t height;
    int32_t qualityDivisor; // 1 = full resolution. Honouring it is optional
    int32_t flags;          // ABYSSO_RENDER_*
    int32_t reserved0;      // Reserved; always 0

    double  centerX;
    double  centerY;
    double  scale;

    // Current position on the timeline, in seconds. The same position must
    // always produce the same image. It can decrease when the user seeks.
    // Do not consult the system clock.
    double  timeSeconds;

    int32_t* outPixels;     // width * height entries, allocated by the host

    // Cancellation flag. Once non-zero, rendering may be stopped.
    // It is always 0 at the start of a frame, and can be nullptr,
    // so check it before dereferencing.
    const volatile int32_t* cancelFlag;
} AbyssoRenderRequest;

// Tests whether a field lies within structSize. Always use it before reading a field.
#define ABYSSO_REQUEST_HAS(req, field)                                              \
    ((req) != nullptr && (req)->structSize >=                                       \
     (int32_t)(offsetof(AbyssoRenderRequest, field) +                               \
               sizeof(((const AbyssoRenderRequest*)0)->field)))

// Required. 1 = completed / 0 = cancelled (outPixels is discarded) / negative = error.
// The same render request must always produce the same image.
ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req);

// Requests cancellation of the frame being rendered; called from another thread.
// Optional. When absent, cancellation is treated as unsupported.
ABYSSO_ADDON_API void CancelAbyssoAddonFrame(void);

// ---------------------------------------------------------------------------
// Details overlay
//
// A display-only list. Items listed here do not appear in the parameter tree,
// but are retrieved through GetAbyssoAddonProp like any other item.
// Update the values only when a frame has completed, and do so atomically.
// A row whose CurrentValue is an empty string is not displayed.
// ---------------------------------------------------------------------------

// Optional. When absent, the info item count is treated as 0.
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItemCount(void);
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItem(int32_t index, AbyssoGuid* out);
