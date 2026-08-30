# Abyssograph Add-on API Reference

| Item | Value |
|---|---|
| ABI version | 2 (`ABYSSO_ADDON_ABI_VERSION`) |
| Required header | `include/AbyssoAddon.h` |
| Optional header | `include/AbyssoAddonKit.h` (implementation helper) |
| Add-on format | Windows x64 native DLL |
| Language | C++17 (the public interface uses C linkage) |

日本語: [api-reference.ja.md](api-reference.ja.md)

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Load sequence](#2-load-sequence)
- [3. Common conventions](#3-common-conventions)
  - [3.1 Linkage and calling convention](#31-linkage-and-calling-convention)
  - [3.2 GUIDs](#32-guids)
  - [3.3 structSize](#33-structsize)
  - [3.4 Return values and error codes](#34-return-values-and-error-codes)
  - [3.5 Strings](#35-strings)
  - [3.6 Threading](#36-threading)
- [4. Exported functions](#4-exported-functions)
  - [4.1 GetAbyssoAddonAbiVersion](#41-getabyssoaddonabiversion)
  - [4.2 GetAbyssoAddonInfo](#42-getabyssoaddoninfo)
  - [4.3 InitializeAbyssoAddon / ShutdownAbyssoAddon](#43-initializeabyssoaddon--shutdownabyssoaddon)
  - [4.4 SetAbyssoAddonLang](#44-setabyssoaddonlang)
  - [4.5 Enumerating the item tree](#45-enumerating-the-item-tree)
  - [4.6 GetAbyssoAddonProp](#46-getabyssoaddonprop)
  - [4.7 SetAbyssoAddonValue](#47-setabyssoaddonvalue)
  - [4.8 ResetAbyssoAddonValue](#48-resetabyssoaddonvalue)
- [5. Rendering](#5-rendering)
  - [5.1 AbyssoRenderRequest](#51-abyssorenderrequest)
  - [5.2 Coordinate system](#52-coordinate-system)
  - [5.3 Output pixels](#53-output-pixels)
  - [5.4 qualityDivisor](#54-qualitydivisor)
  - [5.5 flags](#55-flags)
  - [5.6 timeSeconds](#56-timeseconds)
  - [5.7 Cancellation](#57-cancellation)
  - [5.8 Reproducibility](#58-reproducibility)
- [6. Details overlay](#6-details-overlay)
- [7. AbyssoAddonKit.h](#7-abyssoaddonkith)
  - [7.1 Minimal structure](#71-minimal-structure)
  - [7.2 abysso::Text](#72-abyssotext)
  - [7.3 abysso::Request](#73-abyssorequest)
  - [7.4 abysso::AddonKit](#74-abyssoaddonkit)
  - [7.5 Details overlay](#75-details-overlay)
  - [7.6 Cancellation](#76-cancellation)
  - [7.7 Macros](#77-macros)
- [8. Installation and verification](#8-installation-and-verification)
- [9. Common problems](#9-common-problems)
- [10. Samples](#10-samples)

## 1. Overview

An Abyssograph add-on is a Windows native DLL that implements a rendering algorithm.
For each render request from the host it performs calculation, normalization and colorization,
and returns the pixel data of a single frame.

The subject does not have to be the Mandelbrot set. The host provides only the viewpoint and
the output size; what is drawn, and how, is decided by the add-on.

An add-on publishes its settings as a tree of items. The host does not interpret the meaning of
each item; it builds the UI from the item type alone. A "palette" or a "normalization mode" is
not a special concept to the host — both are simply combo box items.

There is no callback mechanism from an add-on to the host. Initialization data, language settings
and render requests are all passed one way, from the host to the add-on. The SDK therefore
consists of header files only.

## 2. Load sequence

The host (`AbyssographHost.dll`) loads each DLL found in its own folder and detects add-ons as follows.

1. Check for `GetAbyssoAddonAbiVersion`. If it is absent, the file is treated as an ordinary DLL and is
   not shown in the add-on list
2. Check for the required exports (`GetAbyssoAddonAbiVersion`, `GetAbyssoAddonInfo`,
   `RenderAbyssoAddonFrame`). If any is missing, the DLL is not loaded
3. Check that `GetAbyssoAddonAbiVersion()` returns `ABYSSO_ADDON_ABI_VERSION`. If it does not, the DLL is not loaded
4. Call `InitializeAbyssoAddon()` if it is exported. A negative return marks the add-on as unavailable
5. Call `SetAbyssoAddonLang()` if it is exported
6. Call `GetAbyssoAddonInfo()` to obtain the add-on GUID. If an add-on with the same GUID is already
   loaded, the one loaded later is rejected
7. Enumerate the item tree to build a GUID index. If an item GUID collides with one from another
   add-on, the entire add-on is rejected

While the user has an add-on selected, the host repeats the following.

- Retrieve the item tree and item properties to build the parameter UI
- Call `SetAbyssoAddonValue()` whenever a value changes, and update the UI according to the returned change flags
- Call `RenderAbyssoAddonFrame()` whenever an image is required
- Retrieve the values shown in the details overlay

On application exit the host calls `ShutdownAbyssoAddon()` if it is exported, then releases the DLL.

## 3. Common conventions

### 3.1 Linkage and calling convention

- All exported functions use C linkage. `ABYSSO_ADDON_API` applies `extern "C"` and `__declspec(dllexport)`
- Build the DLL for x64. Abyssograph is a 64-bit application, so x86 DLLs are not loaded
- A statically linked CRT (`/MT`) is recommended. Memory is never allocated in one module and freed
  in another, so separate runtimes are not a problem
- Do not let exceptions propagate across the C ABI boundary. A native exception crossing the DLL
  boundary terminates the whole application. Functions generated by `AbyssoAddonKit.h` catch
  exceptions internally with `catch (...)`

### 3.2 GUIDs

`AbyssoGuid` is a 16-byte identifier that does not depend on the Windows `GUID` type.

```cpp
typedef struct { uint8_t bytes[16]; } AbyssoGuid;
```

A GUID is required for the add-on itself, for every parameter item and for every info item.
GUIDs must be unique across all add-ons; if a collision is detected, the add-on loaded later is rejected.

Do not reuse the GUIDs contained in the samples. Generate new ones with `New-Guid` (PowerShell),
`uuidgen` or a similar tool. Once a GUID has been distributed, do not change it: project files
(`.abysso`) store values in the form `addon.<AddonGuid>.<ItemGuid>`, so changing a GUID breaks
compatibility with saved projects.

With `AbyssoAddonKit.h`, the `ABYSSO_GUID` macro lets a GUID be written in its usual notation.

```cpp
// 7276a1f7-bcbd-4496-a390-dc547f4f7407
constexpr AbyssoGuid kAddon = ABYSSO_GUID(72,76,a1,f7,bc,bd,44,96,
                                          a3,90,dc,54,7f,4f,74,07);
```

### 3.3 structSize

Every public struct begins with a `structSize` field, set by whichever side passes the struct.

| Direction | Structs | Implementation note |
|---|---|---|
| Host → add-on | `AbyssoHostInfo`, `AbyssoRenderRequest` | Do not access fields beyond `structSize`. Use `ABYSSO_REQUEST_HAS` to test |
| Add-on → host | `AbyssoAddonInfo`, `AbyssoParamItem` | If the `structSize` set by the host is smaller than `sizeof`, return `ABYSSO_ERROR_BUFFER_TOO_SMALL` |

This allows fields to be appended to the end of a struct without changing the ABI version.
An older add-on receives a shorter struct and simply never reads the unknown tail.

### 3.4 Return values and error codes

Zero or a positive value indicates success; a negative value indicates an error.

| Constant | Value | Meaning |
|---|---|---|
| `ABYSSO_ERROR_INVALID_ARG` | -1 | Invalid argument |
| `ABYSSO_ERROR_UNKNOWN_GUID` | -2 | Unknown GUID |
| `ABYSSO_ERROR_BUFFER_TOO_SMALL` | -3 | Insufficient buffer |
| `ABYSSO_ERROR_READONLY` | -4 | Write to a read-only item |
| `ABYSSO_ERROR_NOT_SUPPORTED` | -5 | Unsupported combination |

### 3.5 Strings

Strings returned by `GetAbyssoAddonProp()` and similar functions are UTF-8 `const char*`.
The host does not take ownership and copies the contents when it needs to retain them.

| Kind | Lifetime |
|---|---|
| Item captions and descriptions | Until the next call to `SetAbyssoAddonLang()` |
| Info item values | Until the next query to the same add-on |

Returning a static string literal is acceptable.

### 3.6 Threading

The following functions are called on the UI thread.

- `GetAbyssoAddonParamChildItemCount`
- `GetAbyssoAddonParamChildItem`
- `GetAbyssoAddonProp`
- `SetAbyssoAddonValue`
- `ResetAbyssoAddonValue`
- `SetAbyssoAddonLang`

`RenderAbyssoAddonFrame()` is called on the rendering thread. Values can change while a frame is
being drawn, so copy the parameters you need into local variables at the start of rendering.
`CancelAbyssoAddonFrame()` is called from another thread during rendering.
Info item values are read at a different time from rendering, so update them atomically.

The value of a `Static` item in the parameter tree is likewise read on the UI thread, as listed
above. When a `Static` item displays a value computed by the rendering thread, hand it over through
`std::atomic` or an equivalent. Unlike the details overlay (chapter 6), `Static` values are not
double-buffered, so the synchronization is the add-on's responsibility.

## 4. Exported functions

Three functions are required: `GetAbyssoAddonAbiVersion`, `GetAbyssoAddonInfo` and
`RenderAbyssoAddonFrame`. All others are optional; if one is not exported, the host treats the
corresponding feature as unsupported. The host never substitutes behaviour of its own.

| Function | Required | Behaviour when not implemented |
|---|---|---|
| `GetAbyssoAddonAbiVersion` | Yes | The DLL is not recognized as an add-on |
| `GetAbyssoAddonInfo` | Yes | Not loaded |
| `RenderAbyssoAddonFrame` | Yes | Not loaded |
| `InitializeAbyssoAddon` | No | No initialization required |
| `ShutdownAbyssoAddon` | No | No cleanup required |
| `SetAbyssoAddonLang` | No | Language switching unsupported; never called |
| `GetAbyssoAddonParamChildItemCount` | No | Child item count treated as 0 |
| `GetAbyssoAddonParamChildItem` | No | Same |
| `GetAbyssoAddonProp` | No | Both parameter items and info items treated as absent |
| `SetAbyssoAddonValue` | No | All items treated as read-only |
| `ResetAbyssoAddonValue` | No | Treated as returning `ABYSSO_CHANGE_NONE` |
| `CancelAbyssoAddonFrame` | No | Cancellation unsupported |
| `GetAbyssoAddonInfoItemCount` | No | Info item count treated as 0 |
| `GetAbyssoAddonInfoItem` | No | Same |

The following groups do not work unless every member is implemented.

- `GetAbyssoAddonParamChildItemCount` and `GetAbyssoAddonParamChildItem`
- `GetAbyssoAddonInfoItemCount` and `GetAbyssoAddonInfoItem`
- Both of the above also require `GetAbyssoAddonProp`

### 4.1 GetAbyssoAddonAbiVersion

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonAbiVersion(void);
```

Required. Returns the ABI version supported by the add-on; implementations normally return
`ABYSSO_ADDON_ABI_VERSION` directly.

This is the first function the host calls, and it is called before initialization. Its presence
determines whether a DLL is recognized as an add-on at all.

### 4.2 GetAbyssoAddonInfo

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfo(AbyssoAddonInfo* out);
```

Required. Returns identification data for the add-on.

```cpp
typedef struct
{
    int32_t     structSize;
    AbyssoGuid  guid;
    const char* name;
    const char* version;
} AbyssoAddonInfo;
```

| Field | Description |
|---|---|
| `structSize` | Set by the host |
| `guid` | GUID that uniquely identifies the add-on |
| `name` | Display name (UTF-8) |
| `version` | Version string; the format is free |

`name` should be the name shown to the user (what the add-on draws), not the DLL file name.
If `SetAbyssoAddonLang()` is implemented, the name may vary with the current language.

### 4.3 InitializeAbyssoAddon / ShutdownAbyssoAddon

```cpp
ABYSSO_ADDON_API int32_t InitializeAbyssoAddon(const AbyssoHostInfo* host);
ABYSSO_ADDON_API void    ShutdownAbyssoAddon(void);
```

Both optional. Useful for building tables, initializing caches or loading configuration files.

```cpp
typedef struct
{
    int32_t     structSize;
    int32_t     abiVersion;   // ABYSSO_ADDON_ABI_VERSION
    const char* hostVersion;  // host identifier, informational
} AbyssoHostInfo;
```

`InitializeAbyssoAddon()` is called once when the add-on is loaded. Return zero or a positive value
on success; a negative value marks the add-on as having failed to load.

`ShutdownAbyssoAddon()` is called on application exit. Release any resources you acquired.

### 4.4 SetAbyssoAddonLang

```cpp
ABYSSO_ADDON_API void SetAbyssoAddonLang(const char* bcp47);
```

Optional. Notifies the display language as a BCP 47 tag (`en-US`, `ja-JP`, …). The default is `en-US`.

After this call, item captions, item descriptions and info item values should be returned in the
requested language. Pointers returned before the call may become invalid.
An add-on that offers a single language does not need to implement this function.

### 4.5 Enumerating the item tree

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItemCount(const AbyssoGuid* target);
ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItem(const AbyssoGuid* target,
                                                      int32_t index,
                                                      AbyssoParamItem* out);
```

Optional, but only as a pair. Enumerates the parameter tree. A `nullptr` `target` means the add-on's
root; otherwise it identifies the item with that GUID.

```cpp
typedef struct
{
    int32_t    structSize;
    int32_t    type;       // kAbyssoParamItemType*
    AbyssoGuid guid;
    int32_t    isReadOnly;
    int32_t    isEnabled;
} AbyssoParamItem;
```

#### Item types

| Constant | UI | Value |
|---|---|---|
| `kAbyssoParamItemTypeGroup` | Group frame | None |
| `kAbyssoParamItemTypeComboBox` | Combo box | GUID of the selected `ComboBoxItem` |
| `kAbyssoParamItemTypeComboBoxItem` | Choice | None (the parent is always a combo box) |
| `kAbyssoParamItemTypeCheckBox` | Check box | `int32_t` (0 / 1) |
| `kAbyssoParamItemTypeStatic` | Label and value | `const char*` (read-only) |
| `kAbyssoParamItemTypeDescription` | Explanatory text | None |
| `kAbyssoParamItemTypeInt32` | Slider | `int32_t` |
| `kAbyssoParamItemTypeDouble` | Slider | `double` |
| `kAbyssoParamItemTypeString` | Text box | `const char*` (UTF-8) |

The value of a `ComboBox` is the GUID of the selected `ComboBoxItem`, and that GUID is what a
project file stores. No index is saved, so reordering the choices, or inserting one in the middle,
does not shift the selection recorded in existing projects.

#### isReadOnly and isEnabled

| Field | Meaning | Value requests | Project file |
|---|---|---|---|
| `isReadOnly` | The value cannot be changed | Reject with `ABYSSO_ERROR_READONLY` | Not saved |
| `isEnabled` | Zero means the item is shown as disabled | Must not be rejected | Saved |

`isEnabled` controls display state only. Loading a project file pushes values in tree order, so
rejecting requests for disabled items causes saved values to fall back to their defaults.

**Do not use `isReadOnly` to lock an item conditionally.** The host excludes read-only items when
it saves a project file, so an item locked at the time of saving is not written to the file. On
loading, an item that is absent from the file returns to its default, so the value is already lost
by the time the lock is released.

For example, making a "manual value" read-only while an "automatic" check box is on means that
reopening a project saved in the automatic state finds the manual value back at its default. Use
`isEnabled` for this instead. Restrict `isReadOnly` to items that hold no value (such as `Static`)
and to items that are read-only at all times.

#### Limits

- A child count that is negative, or greater than 4096, is treated as 0
- The tree may be at most 16 levels deep; a deeper tree causes the add-on to be rejected
- The host descends only into `Group` and `ComboBox` items

#### Errors

| Constant | Condition |
|---|---|
| `ABYSSO_ERROR_INVALID_ARG` | `index` out of range |
| `ABYSSO_ERROR_UNKNOWN_GUID` | Unknown `target` |
| `ABYSSO_ERROR_BUFFER_TOO_SMALL` | `out->structSize` too small |

### 4.6 GetAbyssoAddonProp

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonProp(const AbyssoGuid* target, int32_t prop,
                                            void* data, int32_t dataSize);
```

Optional, but required as soon as a single item is published. Retrieves item properties and values.
On success, return the number of bytes written to `data`.

| `prop` | Type written | Description |
|---|---|---|
| `kAbyssoPropType` | `int32_t` | Item type |
| `kAbyssoPropCaption` | `const char*` | Caption. Write the pointer, not the string contents |
| `kAbyssoPropDescription` | `const char*` | Description; an empty string when there is none |
| `kAbyssoPropCurrentValue` | Depends on item type | Current value |
| `kAbyssoPropDefaultValue` | Depends on item type | Default value |
| `kAbyssoPropMinValue` | `int32_t` / `double` | Minimum (`Int32` / `Double`) |
| `kAbyssoPropMaxValue` | `int32_t` / `double` | Maximum. For `String`, the maximum character count (`int32_t`) |
| `kAbyssoPropStep` | `int32_t` / `double` | Slider step; 0 disables snapping |

The type used for `kAbyssoPropCurrentValue` and `kAbyssoPropDefaultValue` follows the item type.

| Item type | Value type |
|---|---|
| `ComboBox` | `AbyssoGuid` (GUID of the selected `ComboBoxItem`) |
| `CheckBox` / `Int32` | `int32_t` |
| `Double` | `double` |
| `String` / `Static` | `const char*` (UTF-8) |
| `Group` / `ComboBoxItem` / `Description` | No value; return `ABYSSO_ERROR_NOT_SUPPORTED` |

For `String`, `kAbyssoPropMaxValue` is a character count, not a UTF-8 byte count.

Return `ABYSSO_ERROR_NOT_SUPPORTED` for combinations you do not provide (for example the minimum of
a `Double` with no range), and `ABYSSO_ERROR_BUFFER_TOO_SMALL` when `dataSize` is insufficient.

Info item GUIDs are also passed to this function (see [6. Details overlay](#6-details-overlay)).

### 4.7 SetAbyssoAddonValue

```cpp
ABYSSO_ADDON_API int32_t SetAbyssoAddonValue(const AbyssoGuid* target,
                                             const void* data, int32_t dataSize);
```

Optional. Updates an item value. The format of `data` matches `kAbyssoPropCurrentValue`; for a
`ComboBox`, the `AbyssoGuid` of the `ComboBoxItem` to select is passed.

If this function is not implemented, the host presents every item of the add-on as read-only.
A display-only add-on can be written simply by omitting it.

On success, return a bitwise OR of change flags rather than an error code.

| Constant | Value | UI behaviour |
|---|---|---|
| `ABYSSO_CHANGE_NONE` | 0x0 | Nothing |
| `ABYSSO_CHANGE_REDRAW` | 0x1 | Redraw |
| `ABYSSO_CHANGE_PROPS` | 0x2 | Re-read item properties and values (controls are not recreated) |
| `ABYSSO_CHANGE_TREE` | 0x4 | Re-enumerate the tree and rebuild the panel |

Return `ABYSSO_CHANGE_PROPS` when a value change causes other items to change as well — applying a
preset, or altering `isEnabled` or `isReadOnly`. Because there is no callback from an add-on to the
host, this flag is how such dependencies are reported.

The return value does not state whether the input was valid; it states what the UI must update.
To tell the user that an input could not be interpreted, show the reason in a `Static` item and
return `ABYSSO_CHANGE_PROPS`.

### 4.8 ResetAbyssoAddonValue

```cpp
ABYSSO_ADDON_API int32_t ResetAbyssoAddonValue(const AbyssoGuid* target);
```

Optional. A `nullptr` `target` resets every item; otherwise only the specified item is reset.
The return value uses the same change flags as `SetAbyssoAddonValue()`.

The value after a reset must match what `kAbyssoPropDefaultValue` reports.
If the function is not implemented, the host treats it as returning `ABYSSO_CHANGE_NONE`.

## 5. Rendering

```cpp
ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req);
```

Required. Generates one frame and stores it in `outPixels`.

| Return value | Meaning |
|---|---|
| 1 | Rendering completed |
| 0 | Rendering cancelled (the contents of `outPixels` are discarded) |
| Negative | Error |

### 5.1 AbyssoRenderRequest

```cpp
typedef struct
{
    int32_t structSize;

    int32_t width;
    int32_t height;
    int32_t qualityDivisor;
    int32_t flags;
    int32_t reserved0;      // reserved; always 0

    double  centerX;
    double  centerY;
    double  scale;

    double  timeSeconds;

    int32_t* outPixels;

    const volatile int32_t* cancelFlag;
} AbyssoRenderRequest;
```

Fields may be appended to the end of this struct. Before reading a field, confirm that it lies
within `structSize` using `ABYSSO_REQUEST_HAS`.

```cpp
if (ABYSSO_REQUEST_HAS(req, timeSeconds))
{
    // req->timeSeconds may be read
}
```

If a field is out of range, the host does not provide that information; choose a default in the add-on.

### 5.2 Coordinate system

Map screen coordinates to the complex plane exactly as follows. A different mapping means the same
viewpoint shows a different region than in other add-ons.

```cpp
const double aspect = (double)width / (double)height;

const double minX = centerX - scale * aspect;
const double maxX = centerX + scale * aspect;
const double minY = centerY - scale;
const double maxY = centerY + scale;

const double x0 = minX + (maxX - minX) * px / width;
const double y0 = minY + (maxY - minY) * py / height;
```

`scale` is half the height of the visible region. The default overview is approximately
`centerX = -0.5`, `centerY = 0`, `scale = 1.5`.

### 5.3 Output pixels

`outPixels` is an array of `width * height` `int32_t` values allocated by the host, in row-major
order (`outPixels[py * width + px]`).

The pixel format is `0x00RRGGBB`; the upper 8 bits are unused.
Always fill all `width * height` entries, regardless of `qualityDivisor`.

### 5.4 qualityDivisor

A hint that reduced rendering quality is acceptable. A value of 2, for example, permits computing at
half resolution in each axis and scaling the result up. Honouring it is optional; rendering at full
resolution is always valid. Treat values below 1 as 1.

### 5.5 flags

| Constant | Value | Meaning |
|---|---|---|
| `ABYSSO_RENDER_LIVE` | 0x1 | A frame during playback; 0 when stopped |

Use it for caching strategy and cancellation decisions. `qualityDivisor` alone cannot tell you
whether playback is running, because playback may be configured to render at full resolution.

### 5.6 timeSeconds

The current position on the timeline, in seconds. The host always sets this value; reading it is optional.

- The same position must always produce the same image
- It is 0 when playback starts, and holds its position when playback stops
- Seeking can decrease it; it is not monotonically increasing
- It is 0 for a new project and immediately after loading one
- Frame-sequence export derives it from the frame number, not from the system clock

### 5.7 Cancellation

There are two cancellation paths, and the host uses both.

| Path | Description |
|---|---|
| `req->cancelFlag` | Once non-zero, rendering may be stopped. It is always 0 at the start of a frame. It can be `nullptr`, so check before dereferencing |
| `CancelAbyssoAddonFrame()` | Called from another thread. Optional |

If you watch `cancelFlag`, implementing `CancelAbyssoAddonFrame()` is unnecessary.
An add-on that supports neither always renders to completion, which makes playback stutter for
expensive images. Return 0 when you stop early.

Cancellation is independent of `ABYSSO_RENDER_LIVE` in `flags`. One sample (`JuliaAddon`) tests for
cancellation only while `live` is set, but that is an implementation choice made to reduce how often
the test runs, not a rule. Testing unconditionally is equally correct.

**As of 0.4.0.0 the host processes render requests serially and never issues a cancellation.** The
two paths above exist for a future host that does. Not implementing them causes no problem today,
but implementing them keeps playback smooth on a later host.

### 5.8 Reproducibility

The rendered result must be determined solely by the contents of the render request. Depending on
the system clock (`clock()`, `std::chrono::system_clock`), on random numbers, or on state carried
over from the previous frame makes playback and export disagree, and the image cannot be reproduced
by reopening the project.

Parameter values held by the add-on are also part of the request. Because they can change during
rendering, copy them into local variables at the start of a frame.

## 6. Details overlay

```cpp
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItemCount(void);
ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItem(int32_t index, AbyssoGuid* out);
```

Optional, but only as a pair. Enumerates the items shown in the details overlay, typically metadata
or statistics about the rendered result.

Info items form a display-only list, independent of the parameter tree, with these properties.

- They do not appear in the parameter tree
- They are not editable
- They are not saved to project files
- Their contents are retrieved through `GetAbyssoAddonProp()` using `kAbyssoPropCaption` and
  `kAbyssoPropCurrentValue`

The host keeps the enumerated list and, while the overlay is shown, re-reads only
`kAbyssoPropCurrentValue`. Captions are re-read on enumeration and after `SetAbyssoAddonLang()`.

Publish only values that correspond to a completed frame; when rendering is cancelled, keeping the
previous values is recommended. Values are read at a different time from rendering, so update them
atomically.

When `kAbyssoPropCurrentValue` is an empty string the row is not displayed, which can be used to show
an item only in certain modes. If an add-on publishes no rows at all, the overlay box itself is not shown.

## 7. AbyssoAddonKit.h

`AbyssoAddonKit.h` is a header-only library that assists add-on implementation. Its use is optional
and it does not affect the ABI; whether you include it makes no difference to compatibility.

It removes the following boilerplate.

- Item tree management
- Implementation of `GetAbyssoAddonProp`, `SetAbyssoAddonValue` and `ResetAbyssoAddonValue`
- Default value management
- Multi-language support
- Details overlay management
- Implementation of the exported functions

Calculation and colorization are outside its scope.

### 7.1 Minimal structure

```cpp
#include "AbyssoAddon.h"
#include "AbyssoAddonKit.h"

namespace {

constexpr AbyssoGuid kAddon = ABYSSO_GUID(/* 16 bytes */);
constexpr AbyssoGuid kGroup = ABYSSO_GUID(/* ... */);
constexpr AbyssoGuid kReal  = ABYSSO_GUID(/* ... */);
constexpr AbyssoGuid kInfoC = ABYSSO_GUID(/* ... */);

struct Values { double cr = -0.123; };
Values& V() { static Values v; return v; }

abysso::AddonKit& Kit()
{
    static abysso::AddonKit kit([](abysso::AddonKit& k) {
        auto g = k.Group(kGroup, { "JULIA SET", "ジュリア集合" });
        g.Double(kReal, { "c (real)", "c の実部" }, &V().cr)
         .Range(-2.0, 2.0).Step(0.001).Default(-0.123);
        k.InfoRow(kInfoC, { "c", "c" });
    });
    return kit;
}

} // namespace

ABYSSO_ADDON_ABI_VERSION_IMPL()
ABYSSO_ADDON_INFO_IMPL(kAddon,
                       Kit().Japanese() ? "ジュリア集合" : "Julia set",
                       "1.0.0.0")
ABYSSO_ADDON_EXPORT_ALL(Kit())

ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req)
{
    abysso::Request r(req);
    if (!r.Valid()) return ABYSSO_ERROR_INVALID_ARG;
    Kit().BeginFrame();
    // rendering
    return 1;
}
```

`AddonKit` can be neither copied nor moved, so hold it in a function-local static. A namespace-scope
variable would depend on initialization order against the GUID constants and the bound variables.

See the `JuliaAddon` sample for a fuller example.

### 7.2 abysso::Text

Represents a multi-language string. Strings are not copied, so pass static string literals.

```cpp
{ "Preset" }                    // single language
{ "Preset", "プリセット" }      // en / ja

abysso::Text::Langs({ { "en", "..." }, { "ja", "..." }, { "fr", "..." } });
```

`"en"` matches `"en-US"`. If no language matches, the first registered string is used.

### 7.3 abysso::Request

A wrapper around `AbyssoRenderRequest`. It applies `ABYSSO_REQUEST_HAS` internally, so the add-on
does not have to account for struct version differences.

| Method | Description |
|---|---|
| `Valid()` | Pre-render validation (`nullptr`, size, output buffer) |
| `Width()` / `Height()` | Render size |
| `CenterX()` / `CenterY()` / `Scale()` | Viewpoint |
| `Aspect()` / `MinX()` / `MaxX()` / `MinY()` / `MaxY()` | The mapping in 5.2 |
| `Pixels()` | Output buffer |
| `Flags()` / `Live()` | Render flags |
| `QualityDivisor()` | Value clamped so that it is at least 1 |
| `TimeSeconds(fallback = 0.0)` | Timeline position; `fallback` when not provided |
| `CancelFlag()` | Cancellation flag; `nullptr` when not provided |
| `Raw()` | The underlying struct |

### 7.4 abysso::AddonKit

Manages the item tree and item values. `AddonKit` does not store parameter values itself; it binds to
variables owned by the add-on, and rendering code continues to read those variables directly.

Top-level items are registered through `AddonKit`; nested items through the `Node` returned by each
registration method.

| Method | Item created | Bound to |
|---|---|---|
| `Group(guid, caption)` | `Group` | None |
| `Combo(guid, caption, int32_t*)` | `ComboBox` | Index of the selected choice |
| `.Item(guid, caption)` | `ComboBoxItem` | None (returns the combo box itself) |
| `Check(guid, caption, int32_t*)` | `CheckBox` | 0 / 1 |
| `Int(guid, caption, int32_t*)` | `Int32` | `int32_t` |
| `Double(guid, caption, double*)` | `Double` | `double` |
| `String(guid, caption, std::string*)` | `String` | `std::string` |
| `Static(guid, caption, lambda)` | `Static` | The lambda's return value |
| `Note(guid, text)` | `Description` | None |

Every attribute method returns the `Node`, so they can be chained.

| Method | Description |
|---|---|
| `Default(value)` | Default value, used both by `kAbyssoPropDefaultValue` and by reset |
| `Range(min, max)` | Permitted range; out-of-range values are clamped on assignment |
| `Step(step)` | Slider step |
| `MaxLength(chars)` | Maximum character count for `String` (not bytes) |
| `Desc(text)` | Item description |
| `ReadOnly(bool)` | Make the item read-only (the value is not saved to the project file; see 4.5) |
| `ReadOnlyIf(predicate)` | Make the item read-only conditionally. **On an item that holds a value it is registered as `EnabledIf()`** (see below) |
| `EnabledIf(predicate)` | Show as disabled conditionally (value requests are still accepted) |
| `Redraw(bool)` | Treat the item as not requiring a redraw when its value changes |
| `OnChange(lambda)` | Called after the value is updated; returns additional `ABYSSO_CHANGE_*` flags |
| `OnReset(lambda)` | Called after an individual item is reset |

`OnChange` and `OnReset` run after the bound variable has been updated, which is why they take no
arguments. Update other items inside them and return `ABYSSO_CHANGE_PROPS` to express a dependency.
Returning `abysso::kNoRedraw` withdraws the redraw request for that call only.

A `ComboBox` binds to the zero-based index in registration order; `AddonKit` converts to and from GUIDs.
That index is an internal representation of the add-on; what a project file stores is the GUID of the
selected choice (4.5).

The lambda of a `Static` item is called on the UI thread (3.6). When it returns a value the rendering
thread updates, hand it over through `std::atomic` or an equivalent.

When `ReadOnlyIf()` is applied to an item that holds a value, `AddonKit` registers it as `EnabledIf()`
instead, because a read-only item is not saved to the project file and a value locked at the moment of
saving would be lost (4.5). A disabled item is equally impossible to operate, but it accepts value
requests, so the value is saved and restored.

As a result of this conversion, `isReadOnly` stays 0 on an item that holds a value and `isEnabled`
becomes the negation of the predicate. If `EnabledIf()` is also present, the item is enabled only while
both allow it. Items that hold no value (`Group`, `Static`, `Description`) are not converted.

`ReadOnly(true)`, which is unconditional, is not converted: its value never changes through user input,
so nothing is lost by not saving it.

### 7.5 Details overlay

```cpp
kit.InfoRow(kInfoIter, { "Iterations", "反復回数" });   // registration order is display order

// call after a frame has been completed
kit.Publish([iter, manual](abysso::AddonKit::Writer& w) {
    w.Setf(kInfoIter, "%d", iter);
    if (manual) w.Append(kInfoIter, w.Ja() ? " (手動)" : " (manual)");
});

kit.ClearInfo();   // blank every row
```

Capture by value in the lambda passed to `Publish()`. The same lambda is re-executed when the
language changes, so a captured reference may refer to a variable that has gone out of scope.

`Writer` provides `Set()`, `Setf()` and `Append()`, together with `Ja()` and `Lang()`.
`kit.ChoiceCaption(combo, index)` returns the caption of the selected `ComboBoxItem`.

### 7.6 Cancellation

```cpp
kit.BeginFrame();                       // reset the flag at the start of a frame

if (kit.Cancelled(r)) return 0;         // test inside the loop
```

`Cancelled()` covers both `req->cancelFlag` and `CancelAbyssoAddonFrame()`.
`ABYSSO_ADDON_EXPORT_CANCEL` forwards `CancelAbyssoAddonFrame()` to `kit.RequestCancel()`.

### 7.7 Macros

| Macro | Functions generated |
|---|---|
| `ABYSSO_ADDON_ABI_VERSION_IMPL()` | `GetAbyssoAddonAbiVersion` |
| `ABYSSO_ADDON_INFO_IMPL(guid, name, version)` | `GetAbyssoAddonInfo` (`name` and `version` are expressions) |
| `ABYSSO_ADDON_EXPORT_PARAMS(kit)` | `GetAbyssoAddonParamChildItemCount`, `GetAbyssoAddonParamChildItem`, `SetAbyssoAddonValue`, `ResetAbyssoAddonValue` |
| `ABYSSO_ADDON_EXPORT_PROP(kit)` | `GetAbyssoAddonProp` (required by both PARAMS and INFO) |
| `ABYSSO_ADDON_EXPORT_INFO(kit)` | `GetAbyssoAddonInfoItemCount`, `GetAbyssoAddonInfoItem` |
| `ABYSSO_ADDON_EXPORT_LANG(kit)` | `SetAbyssoAddonLang` |
| `ABYSSO_ADDON_EXPORT_CANCEL(kit)` | `CancelAbyssoAddonFrame` |
| `ABYSSO_ADDON_EXPORT_ALL(kit)` | PARAMS, PROP, INFO, LANG and CANCEL together |

`RenderAbyssoAddonFrame`, `InitializeAbyssoAddon` and `ShutdownAbyssoAddon` are written by hand.
Every function the macros generate includes `catch (...)`, so no exception crosses the C ABI boundary.

## 8. Installation and verification

1. Build the add-on for x64
2. Create the add-on folder if it does not exist yet:

   ```text
   %USERPROFILE%\Documents\Abyssograph\Addons\
   ```

3. Copy the DLL — together with any DLLs it depends on — into that folder
4. If the DLL was downloaded from the internet, unblock it first (see below)
5. Start Abyssograph

The location is the same for the Microsoft Store build and for the non-packaged build. The
application does not create the folder: create it yourself. `Documents` is a known folder, so the
actual path follows any redirection in effect (to OneDrive, for example).

Add-ons are loaded once, at startup. Adding, removing, enabling, disabling or unblocking a DLL takes
effect the next time the application starts.

**Load order.** The add-ons bundled with the application are loaded first, then the DLLs in the
folder above, in ordinal order by file name. When two add-ons report the same GUID the one loaded
later is dropped, so a bundled add-on always wins and the add-on selected at startup is always a
bundled one. Dependencies of an add-on are resolved from the add-on's own folder.

**Downloaded files are not loaded.** Windows marks files that came from the internet with the
`Zone.Identifier` alternate data stream. Abyssograph does not load a marked DLL: it appears in the
add-on list as *Unblock required* and cannot be enabled from there. Right-click the DLL in File
Explorer, choose Properties, select **Unblock**, and start the application again. The application
never removes the mark for you — unblocking is a deliberate step taken outside the application.

When two or more add-ons are present, an add-on selector appears on the title row of the parameter panel.

The load result is shown under Help → About → Add-ons: name, version and enabled state, plus the
reason for any add-on that failed to load (ABI version mismatch, missing required exports, duplicate
GUID, initialization failure, still blocked). Enabling and disabling takes effect on the next launch.

If an add-on causes a crash or a hang, it is disabled automatically on the next launch.
Re-enable it manually from the add-on list to use it again.

## 9. Common problems

| Cause | Resolution |
|---|---|
| Reusing a GUID from a sample | A collision with another add-on prevents loading. Generate your own GUIDs |
| Changing a GUID after distribution | Compatibility with saved projects is lost. Do not change it |
| Treating `isEnabled` as a write barrier | Value requests must be accepted even when disabled, or project restore stops working |
| Managing default values in several places | `kAbyssoPropDefaultValue` and reset drift apart. Keep defaults in one place |
| Treating `MaxLength` as a byte count | It is a character count; truncating by bytes corrupts UTF-8 |
| Not copying values at the start of a frame | Parameter changes during rendering make the result indeterminate |
| Using the system clock or random numbers | Redraw and export lose reproducibility (5.8) |
| Letting an exception escape the DLL boundary | The whole application terminates |
| Building for x86 | The DLL is not loaded. Build for x64 |
| Not checking `structSize` | Compatibility breaks when the SDK extends a struct |
| Linking the CRT dynamically | Loading fails where the runtime is absent. `/MT` is recommended |

## 10. Samples

| Sample | Description |
|---|---|
| `samples/MinimalAddon` | Minimal structure implementing only the three required functions; no parameters and no info items |
| `samples/JuliaAddon` | A practical example built on `AbyssoAddonKit.h`, covering parameters, dependent items, the details overlay, cancellation and `qualityDivisor` |

See [../README.md](../README.md) for build instructions.
