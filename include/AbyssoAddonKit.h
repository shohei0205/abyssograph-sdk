// SPDX-License-Identifier: MIT
// Copyright (c) 2026 shohei-s.com

#pragma once

// Abyssograph add-on SDK --- helper class and macros.
//
// This header is optional. An add-on only requires AbyssoAddon.h; this header
// takes on item tree registration and the implementation of the exported
// functions (Prop / Set / Reset / info items). It is header-only and does not
// affect the ABI. See section 7 of docs/api-reference.en.md for usage.

#include "AbyssoAddon.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <string>
#include <vector>

// Version of this helper. Unrelated to the ABI; it marks source compatibility only.
#define ABYSSO_KIT_VERSION 1

// GUID literal. The bytes are written in the usual GUID notation, without 0x.
//   constexpr AbyssoGuid kAddon = ABYSSO_GUID(72,76,a1,f7,bc,bd,44,96,
//                                             a3,90,dc,54,7f,4f,74,07);
#define ABYSSO_GUID_BYTES(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    { 0x##a, 0x##b, 0x##c, 0x##d, 0x##e, 0x##f, 0x##g, 0x##h,             \
      0x##i, 0x##j, 0x##k, 0x##l, 0x##m, 0x##n, 0x##o, 0x##p }

// The 16 arguments are named individually; the traditional MSVC preprocessor
// collapses __VA_ARGS__ into a single argument.
#define ABYSSO_GUID(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)     AbyssoGuid { ABYSSO_GUID_BYTES(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) }

namespace abysso {

// Flag with which OnChange states that the image does not change this time.
// AddonKit removes it before returning, so it never appears in the ABI.
constexpr int32_t kNoRedraw = 0x40000000;

inline bool GuidEqual(const AbyssoGuid& a, const AbyssoGuid& b)
{
    return std::memcmp(a.bytes, b.bytes, 16) == 0;
}

// Truncates to a number of UTF-8 characters, not bytes.
inline void TruncateUtf8(std::string& text, int32_t maxChars)
{
    if (maxChars <= 0) return;

    int32_t chars = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        // A continuation byte (10xxxxxx) does not start a character.
        if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue;
        if (chars == maxChars) { text.resize(i); return; }
        ++chars;
    }
}

// ---------------------------------------------------------------------------
// Multi-language string
//
// Strings are not copied; pass static string literals.
//
//   { "Preset" }                    ... a single language
//   { "Preset", "プリセット" }      ... en / ja
//   Text::Langs({ { "en", "..." }, { "ja", "..." }, { "fr", "..." } })
//
// "en" matches "en-US". If nothing matches, the first entry is returned.
// ---------------------------------------------------------------------------
class Text
{
public:
    struct Entry { const char* lang; const char* text; };

    Text() = default;
    Text(const char* only) { Add("", only); }
    Text(const char* en, const char* ja) { Add("en", en); Add("ja", ja); }

    static Text Langs(std::initializer_list<Entry> entries)
    {
        Text t;
        for (const Entry& e : entries) t.Add(e.lang, e.text);
        return t;
    }

    bool Empty() const { return count_ == 0; }

    // Tests the language tag. "ja" matches "ja-JP".
    static bool Is(const char* tag, const std::string& lang) { return Matches(tag, lang); }

    const char* Get(const std::string& lang) const
    {
        if (count_ == 0) return "";
        for (int i = 0; i < count_; ++i)
        {
            if (Matches(entries_[i].lang, lang)) return Safe(entries_[i].text);
        }
        return Safe(entries_[0].text);
    }

private:
    static constexpr int kMax = 8;

    static const char* Safe(const char* s) { return s != nullptr ? s : ""; }

    static bool Matches(const char* tag, const std::string& lang)
    {
        if (tag == nullptr || *tag == '\0') return false;
        const size_t len = std::strlen(tag);
        if (lang.size() < len) return false;
        for (size_t i = 0; i < len; ++i)
        {
            if (Lower(tag[i]) != Lower(lang[i])) return false;
        }
        // "en" matches "en-US" but not "eng".
        return lang.size() == len || lang[len] == '-' || lang[len] == '_';
    }

    static char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

    void Add(const char* lang, const char* text)
    {
        if (count_ >= kMax) return;
        entries_[count_].lang = lang;
        entries_[count_].text = text;
        ++count_;
    }

    Entry entries_[kMax] = {};
    int   count_ = 0;
};

// ---------------------------------------------------------------------------
// Render request wrapper
//
//   abysso::Request r(req);
//   if (!r.Valid()) return ABYSSO_ERROR_INVALID_ARG;
//   const double t = r.TimeSeconds();   // 0.0 when the host does not provide it
// ---------------------------------------------------------------------------
class Request
{
public:
    explicit Request(const AbyssoRenderRequest* req) : req_(req) {}

    // Validation to perform before rendering starts.
    bool Valid() const
    {
        return req_ != nullptr && req_->width > 0 && req_->height > 0
            && req_->outPixels != nullptr;
    }

    const AbyssoRenderRequest* Raw() const { return req_; }

    int32_t  Width()  const { return req_ != nullptr ? req_->width  : 0; }
    int32_t  Height() const { return req_ != nullptr ? req_->height : 0; }
    double   CenterX() const { return req_ != nullptr ? req_->centerX : 0.0; }
    double   CenterY() const { return req_ != nullptr ? req_->centerY : 0.0; }
    double   Scale()  const { return req_ != nullptr ? req_->scale : 1.0; }
    int32_t  Flags()  const { return req_ != nullptr ? req_->flags : 0; }
    int32_t* Pixels() const { return req_ != nullptr ? req_->outPixels : nullptr; }

    // Values below 1 are treated as 1.
    int32_t QualityDivisor() const
    {
        const int32_t d = req_ != nullptr ? req_->qualityDivisor : 1;
        return d < 1 ? 1 : d;
    }

    bool Live() const { return (Flags() & ABYSSO_RENDER_LIVE) != 0; }

    // Aspect ratio and the resulting range on the complex plane.
    double Aspect() const
    {
        const int32_t h = Height();
        return h > 0 ? static_cast<double>(Width()) / static_cast<double>(h) : 1.0;
    }

    double MinX() const { return CenterX() - Scale() * Aspect(); }
    double MaxX() const { return CenterX() + Scale() * Aspect(); }
    double MinY() const { return CenterY() - Scale(); }
    double MaxY() const { return CenterY() + Scale(); }

    // Returns fallback when the field is not provided.
    double TimeSeconds(double fallback = 0.0) const
    {
        return ABYSSO_REQUEST_HAS(req_, timeSeconds) ? req_->timeSeconds : fallback;
    }

    const volatile int32_t* CancelFlag() const
    {
        return ABYSSO_REQUEST_HAS(req_, cancelFlag) ? req_->cancelFlag : nullptr;
    }

private:
    const AbyssoRenderRequest* req_;
};

// ---------------------------------------------------------------------------
// AddonKit --- manages the item tree and the info items
//
// AddonKit holds no values; it binds to variables owned by the add-on.
//
// Threading: registration and value access (ChildItem / Prop / Set / Reset /
// SetLang) are called by the host from a single thread. Only Publish is called
// from the rendering thread, so only the info items are guarded.
// ---------------------------------------------------------------------------
class AddonKit
{
public:
    class Node;

    AddonKit() = default;

    // Registration is performed in the constructor. AddonKit can be neither
    // copied nor moved, so the instance is passed to the supplied lambda.
    explicit AddonKit(const std::function<void(AddonKit&)>& build) { build(*this); }

    AddonKit(const AddonKit&) = delete;
    AddonKit& operator=(const AddonKit&) = delete;

    // -----------------------------------------------------------------------
    // Registration of top-level items. For nested items, use the methods of
    // the same name on the returned Node
    // -----------------------------------------------------------------------
    Node Root();
    Node Group(const AbyssoGuid& guid, Text caption);
    Node Combo(const AbyssoGuid& guid, Text caption, int32_t* bind);
    Node Check(const AbyssoGuid& guid, Text caption, int32_t* bind);
    Node Int(const AbyssoGuid& guid, Text caption, int32_t* bind);
    Node Double(const AbyssoGuid& guid, Text caption, double* bind);
    Node String(const AbyssoGuid& guid, Text caption, std::string* bind);
    Node Static(const AbyssoGuid& guid, Text caption, std::function<std::string()> value);
    Node Note(const AbyssoGuid& guid, Text text);

    // -----------------------------------------------------------------------
    // Info items, shown in the details overlay
    // -----------------------------------------------------------------------

    // Registers one info item. Registration order is display order.
    void InfoRow(const AbyssoGuid& guid, Text caption);

    // Updates every info value. Call it only when a frame has completed.
    // Capture by value: the lambda is re-executed when the language changes.
    //
    //   kit.Publish([iter, manual](abysso::AddonKit::Writer& w) {
    //       w.Setf(kInfoIter, "%d", iter);
    //       if (manual) w.Append(kInfoIter, w.Ja() ? " (手動)" : " (manual)");
    //   });
    class Writer;
    void Publish(std::function<void(Writer&)> fill);

    // Blanks every info value. An item with an empty string is not displayed.
    void ClearInfo();

    // -----------------------------------------------------------------------
    // Language
    // -----------------------------------------------------------------------
    void SetLang(const char* bcp47);
    const std::string& Lang() const { return lang_; }
    bool Japanese() const { return Text::Is("ja", lang_); }

    // -----------------------------------------------------------------------
    // Cancellation, covering both CancelAbyssoAddonFrame and the cancelFlag of
    // the render request. Call BeginFrame() at the start of a frame and test
    // Cancelled(r) inside the loop.
    // -----------------------------------------------------------------------
    void BeginFrame() { cancel_.store(false, std::memory_order_relaxed); }
    void RequestCancel() { cancel_.store(true, std::memory_order_relaxed); }
    bool Cancelled(const Request& r) const
    {
        if (cancel_.load(std::memory_order_relaxed)) return true;
        const volatile int32_t* flag = r.CancelFlag();
        return flag != nullptr && *flag != 0;
    }

    // -----------------------------------------------------------------------
    // API forwarded to the exported functions; called by the macros, and may
    // also be called directly
    // -----------------------------------------------------------------------
    int32_t ChildItemCount(const AbyssoGuid* target) const;
    int32_t ChildItem(const AbyssoGuid* target, int32_t index, AbyssoParamItem* out) const;
    int32_t Prop(const AbyssoGuid* target, int32_t prop, void* data, int32_t dataSize) const;
    int32_t Set(const AbyssoGuid* target, const void* data, int32_t dataSize);
    int32_t Reset(const AbyssoGuid* target);
    // Caption of the index-th choice of a ComboBox, in the current language.
    const char* ChoiceCaption(const AbyssoGuid& combo, int32_t index) const;

    int32_t InfoItemCount() const;
    int32_t InfoItem(int32_t index, AbyssoGuid* out) const;

    // -----------------------------------------------------------------------
    // Writes info item values
    // -----------------------------------------------------------------------
    class Writer
    {
    public:
        void Set(const AbyssoGuid& guid, const std::string& value);
        void Setf(const AbyssoGuid& guid, const char* format, ...);
        void Append(const AbyssoGuid& guid, const std::string& value);
        bool Ja() const { return ja_; }
        const std::string& Lang() const { return lang_; }

    private:
        friend class AddonKit;
        Writer(const AddonKit& kit, std::vector<std::string>& out, std::string lang, bool ja)
            : kit_(kit), out_(out), lang_(std::move(lang)), ja_(ja) {}

        const AddonKit&           kit_;
        std::vector<std::string>& out_;
        std::string               lang_;
        bool                      ja_;
    };

    // -----------------------------------------------------------------------
    // Handle used to build a single item. It holds an index rather than a
    // value, so copying is cheap
    // -----------------------------------------------------------------------
    class Node
    {
    public:
        Node(AddonKit* kit, int index) : kit_(kit), index_(index) {}

        // --- Child items ---
        Node Group(const AbyssoGuid& guid, Text caption);
        Node Combo(const AbyssoGuid& guid, Text caption, int32_t* bind);
        Node Check(const AbyssoGuid& guid, Text caption, int32_t* bind);
        Node Int(const AbyssoGuid& guid, Text caption, int32_t* bind);
        Node Double(const AbyssoGuid& guid, Text caption, double* bind);
        Node String(const AbyssoGuid& guid, Text caption, std::string* bind);
        Node Static(const AbyssoGuid& guid, Text caption, std::function<std::string()> value);
        Node Note(const AbyssoGuid& guid, Text text);

        // A choice of a ComboBox. It returns the ComboBox itself, so calls chain.
        Node Item(const AbyssoGuid& guid, Text caption);

        // --- Attributes ---
        // The default value is defined here only; both kAbyssoPropDefaultValue and
        // ResetAbyssoAddonValue use it.
        Node Default(int32_t value);
        Node Default(double value);
        Node Default(const std::string& value);
        Node Range(int32_t min, int32_t max);
        Node Range(double min, double max);
        Node Step(int32_t step);
        Node Step(double step);
        Node MaxLength(int32_t chars);   // For String; a character count
        Node Desc(Text description);     // Description shown with the item
        Node ReadOnly(bool value = true);
        Node EnabledIf(std::function<bool()> predicate);
        // On an item that holds a value this registers EnabledIf instead:
        // a read-only value is not saved to the project file.
        Node ReadOnlyIf(std::function<bool()> predicate);

        // Marks an item whose value does not affect the image (by default it does).
        Node Redraw(bool value);

        // Called after the value has been written; returns additional
        // ABYSSO_CHANGE_* flags. Other items may be updated here; returning
        // ABYSSO_CHANGE_PROPS makes the UI re-read them. Returning
        // abysso::kNoRedraw withdraws the redraw request for that call only.
        Node OnChange(std::function<int32_t()> handler);

        // Called after this item alone has been reset to its default value.
        Node OnReset(std::function<int32_t()> handler);

        const AbyssoGuid& Guid() const;

    private:
        AddonKit* kit_;
        int       index_;
    };

private:
    struct ItemDef
    {
        AbyssoGuid guid{};
        int32_t    type = kAbyssoParamItemTypeGroup;
        int        parent = -1;
        Text       caption;
        Text       description;

        int32_t*     bindInt    = nullptr;
        double*      bindDouble = nullptr;
        std::string* bindString = nullptr;

        int32_t     defInt    = 0;
        double      defDouble = 0.0;
        std::string defString;

        bool    hasIntRange = false;
        int32_t minInt = 0, maxInt = 0, stepInt = 0;
        bool    hasDoubleRange = false;
        double  minDouble = 0.0, maxDouble = 0.0, stepDouble = 0.0;
        int32_t maxLength = 0;

        bool redraw   = true;
        bool readOnly = false;

        std::function<bool()>        enabledIf;
        std::function<bool()>        readOnlyIf;
        std::function<int32_t()>     onChange;
        std::function<int32_t()>     onReset;
        std::function<std::string()> staticValue;

        mutable std::string cache; // Storage for the value returned for Static
    };

    struct InfoDef
    {
        AbyssoGuid guid{};
        Text       caption;
    };

    friend class Node;

    // --- Registration ---
    int  Add(int parent, int32_t type, const AbyssoGuid& guid, Text caption);
    ItemDef&       At(int index)       { return items_[static_cast<size_t>(index)]; }
    const ItemDef& At(int index) const { return items_[static_cast<size_t>(index)]; }

    // --- Lookup ---
    int Find(const AbyssoGuid& guid) const;
    int FindInfo(const AbyssoGuid& guid) const;
    int NthChild(int parent, int32_t index) const;
    int32_t ChildCount(int parent) const;
    int32_t OrdinalOf(int index) const;

    // --- Predicates ---
    bool IsReadOnly(const ItemDef& item) const;
    bool IsEnabled(const ItemDef& item) const;
    bool HoldsValue(const ItemDef& item) const;

    // --- Values ---
    int32_t WriteCurrent(int node, void* data, int32_t dataSize) const;
    int32_t WriteDefault(int node, void* data, int32_t dataSize) const;
    void    ApplyDefault(ItemDef& item);
    int32_t PropOfInfo(int index, int32_t prop, void* data, int32_t dataSize) const;

    template <typename T>
    static int32_t Write(void* data, int32_t dataSize, const T& value)
    {
        if (data == nullptr) return ABYSSO_ERROR_INVALID_ARG;
        if (dataSize < static_cast<int32_t>(sizeof(T))) return ABYSSO_ERROR_BUFFER_TOO_SMALL;
        *static_cast<T*>(data) = value;
        return static_cast<int32_t>(sizeof(T));
    }

    void Republish();

    std::vector<ItemDef> items_;
    std::vector<InfoDef> infoItems_;
    std::string           lang_ = "en-US";

    // Info values are swapped through a double buffer.
    std::vector<std::string>      info_[2];
    std::atomic<int>              infoSlot_{ 0 };
    std::function<void(Writer&)>  infoFill_;
    mutable std::mutex            infoMutex_;

    std::atomic<bool> cancel_{ false };
};

// ===========================================================================
// Implementation
// ===========================================================================

inline int AddonKit::Add(int parent, int32_t type, const AbyssoGuid& guid, Text caption)
{
    ItemDef item;
    item.guid    = guid;
    item.type    = type;
    item.parent  = parent;
    item.caption = caption;
    items_.push_back(std::move(item));
    return static_cast<int>(items_.size()) - 1;
}

inline int AddonKit::Find(const AbyssoGuid& guid) const
{
    for (size_t i = 0; i < items_.size(); ++i)
    {
        if (GuidEqual(items_[i].guid, guid)) return static_cast<int>(i);
    }
    return -1;
}

inline int AddonKit::FindInfo(const AbyssoGuid& guid) const
{
    for (size_t i = 0; i < infoItems_.size(); ++i)
    {
        if (GuidEqual(infoItems_[i].guid, guid)) return static_cast<int>(i);
    }
    return -1;
}

inline int AddonKit::NthChild(int parent, int32_t index) const
{
    int32_t seen = 0;
    for (size_t i = 0; i < items_.size(); ++i)
    {
        if (items_[i].parent != parent) continue;
        if (seen == index) return static_cast<int>(i);
        ++seen;
    }
    return -1;
}

inline int32_t AddonKit::ChildCount(int parent) const
{
    int32_t count = 0;
    for (size_t i = 0; i < items_.size(); ++i)
    {
        if (items_[i].parent == parent) ++count;
    }
    return count;
}

// Index within the parent. The order of the choices is the value of a ComboBox.
inline int32_t AddonKit::OrdinalOf(int index) const
{
    int32_t ordinal = 0;
    for (int i = 0; i < index; ++i)
    {
        if (items_[static_cast<size_t>(i)].parent == At(index).parent) ++ordinal;
    }
    return ordinal;
}

inline bool AddonKit::HoldsValue(const ItemDef& item) const
{
    switch (item.type)
    {
    case kAbyssoParamItemTypeComboBox:
    case kAbyssoParamItemTypeCheckBox:
    case kAbyssoParamItemTypeInt32:
    case kAbyssoParamItemTypeDouble:
    case kAbyssoParamItemTypeString:
        return true;
    default:
        return false;
    }
}

// Group, Description and Static are never writable. ComboBoxItem is not listed
// here: the flag expresses whether a control can be operated, and a choice is
// not a control of its own.
inline bool AddonKit::IsReadOnly(const ItemDef& item) const
{
    switch (item.type)
    {
    case kAbyssoParamItemTypeGroup:
    case kAbyssoParamItemTypeStatic:
    case kAbyssoParamItemTypeDescription:
        return true;
    default:
        break;
    }
    if (item.readOnlyIf) return item.readOnlyIf();
    return item.readOnly;
}

inline bool AddonKit::IsEnabled(const ItemDef& item) const
{
    return item.enabledIf ? item.enabledIf() : true;
}

inline void AddonKit::SetLang(const char* bcp47)
{
    lang_ = (bcp47 != nullptr && *bcp47 != '\0') ? bcp47 : "en-US";
    Republish(); // Info values depend on the language
}

inline void AddonKit::InfoRow(const AbyssoGuid& guid, Text caption)
{
    InfoDef row;
    row.guid    = guid;
    row.caption = caption;
    infoItems_.push_back(std::move(row));
    info_[0].resize(infoItems_.size());
    info_[1].resize(infoItems_.size());
}

inline void AddonKit::Publish(std::function<void(Writer&)> fill)
{
    std::lock_guard<std::mutex> lock(infoMutex_);
    infoFill_ = std::move(fill);
    Republish();
}

inline void AddonKit::ClearInfo()
{
    std::lock_guard<std::mutex> lock(infoMutex_);
    infoFill_ = nullptr;
    Republish();
}

// The update is atomic: the back buffer is filled first, then the index is swapped.
inline void AddonKit::Republish()
{
    const int slot = 1 - infoSlot_.load(std::memory_order_relaxed);
    std::vector<std::string>& out = info_[slot];
    out.assign(infoItems_.size(), std::string());

    if (infoFill_)
    {
        Writer writer(*this, out, lang_, Japanese());
        infoFill_(writer);
    }
    infoSlot_.store(slot, std::memory_order_release);
}

inline void AddonKit::Writer::Set(const AbyssoGuid& guid, const std::string& value)
{
    const int index = kit_.FindInfo(guid);
    if (index < 0) return;
    out_[static_cast<size_t>(index)] = value;
}

inline void AddonKit::Writer::Append(const AbyssoGuid& guid, const std::string& value)
{
    const int index = kit_.FindInfo(guid);
    if (index < 0) return;
    out_[static_cast<size_t>(index)] += value;
}

inline void AddonKit::Writer::Setf(const AbyssoGuid& guid, const char* format, ...)
{
    const int index = kit_.FindInfo(guid);
    if (index < 0 || format == nullptr) return;

    char buffer[512];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    out_[static_cast<size_t>(index)] = (written > 0) ? buffer : "";
}

// ---------------------------------------------------------------------------
// Implementation of the API forwarded to the exported functions
// ---------------------------------------------------------------------------

inline int32_t AddonKit::ChildItemCount(const AbyssoGuid* target) const
{
    int parent = -1;
    if (target != nullptr)
    {
        parent = Find(*target);
        if (parent < 0) return 0; // An unknown GUID has no children
    }
    return ChildCount(parent);
}

inline int32_t AddonKit::ChildItem(const AbyssoGuid* target, int32_t index,
                                   AbyssoParamItem* out) const
{
    if (out == nullptr || index < 0) return ABYSSO_ERROR_INVALID_ARG;
    if (out->structSize < static_cast<int32_t>(sizeof(AbyssoParamItem)))
        return ABYSSO_ERROR_BUFFER_TOO_SMALL;

    int parent = -1;
    if (target != nullptr)
    {
        parent = Find(*target);
        if (parent < 0) return ABYSSO_ERROR_UNKNOWN_GUID;
    }

    const int node = NthChild(parent, index);
    if (node < 0) return ABYSSO_ERROR_INVALID_ARG;

    const ItemDef& item = At(node);
    out->type       = item.type;
    out->guid       = item.guid;
    out->isReadOnly = IsReadOnly(item) ? 1 : 0;
    out->isEnabled  = IsEnabled(item) ? 1 : 0;
    return 0;
}

inline int32_t AddonKit::WriteCurrent(int node, void* data, int32_t dataSize) const
{
    const ItemDef& item = At(node);
    switch (item.type)
    {
    case kAbyssoParamItemTypeComboBox:
    {
        const int32_t index = item.bindInt != nullptr ? *item.bindInt : item.defInt;
        int child = NthChild(node, index);
        if (child < 0) child = NthChild(node, 0);
        if (child < 0) return ABYSSO_ERROR_NOT_SUPPORTED;
        return Write<AbyssoGuid>(data, dataSize, At(child).guid);
    }
    case kAbyssoParamItemTypeCheckBox:
    case kAbyssoParamItemTypeInt32:
        return Write<int32_t>(data, dataSize,
                              item.bindInt != nullptr ? *item.bindInt : item.defInt);
    case kAbyssoParamItemTypeDouble:
        return Write<double>(data, dataSize,
                             item.bindDouble != nullptr ? *item.bindDouble : item.defDouble);
    case kAbyssoParamItemTypeString:
        item.cache = item.bindString != nullptr ? *item.bindString : item.defString;
        return Write<const char*>(data, dataSize, item.cache.c_str());
    case kAbyssoParamItemTypeStatic:
        item.cache = item.staticValue ? item.staticValue() : std::string();
        return Write<const char*>(data, dataSize, item.cache.c_str());
    default:
        return ABYSSO_ERROR_NOT_SUPPORTED;
    }
}

inline int32_t AddonKit::WriteDefault(int node, void* data, int32_t dataSize) const
{
    const ItemDef& item = At(node);
    switch (item.type)
    {
    case kAbyssoParamItemTypeComboBox:
    {
        int child = NthChild(node, item.defInt);
        if (child < 0) child = NthChild(node, 0);
        if (child < 0) return ABYSSO_ERROR_NOT_SUPPORTED;
        return Write<AbyssoGuid>(data, dataSize, At(child).guid);
    }
    case kAbyssoParamItemTypeCheckBox:
    case kAbyssoParamItemTypeInt32:
        return Write<int32_t>(data, dataSize, item.defInt);
    case kAbyssoParamItemTypeDouble:
        return Write<double>(data, dataSize, item.defDouble);
    case kAbyssoParamItemTypeString:
        item.cache = item.defString;
        return Write<const char*>(data, dataSize, item.cache.c_str());
    default:
        return ABYSSO_ERROR_NOT_SUPPORTED;
    }
}

inline int32_t AddonKit::PropOfInfo(int index, int32_t prop, void* data, int32_t dataSize) const
{
    const InfoDef& row = infoItems_[static_cast<size_t>(index)];
    switch (prop)
    {
    case kAbyssoPropType:
        return Write<int32_t>(data, dataSize, kAbyssoParamItemTypeStatic);
    case kAbyssoPropCaption:
        return Write<const char*>(data, dataSize, row.caption.Get(lang_));
    case kAbyssoPropCurrentValue:
    {
        const int slot = infoSlot_.load(std::memory_order_acquire);
        const std::vector<std::string>& values = info_[slot];
        const char* text = index < static_cast<int>(values.size())
            ? values[static_cast<size_t>(index)].c_str() : "";
        return Write<const char*>(data, dataSize, text);
    }
    case kAbyssoPropDescription:
        return Write<const char*>(data, dataSize, "");
    default:
        return ABYSSO_ERROR_NOT_SUPPORTED;
    }
}

inline int32_t AddonKit::Prop(const AbyssoGuid* target, int32_t prop,
                              void* data, int32_t dataSize) const
{
    if (target == nullptr) return ABYSSO_ERROR_INVALID_ARG;

    // Info items are searched first. They are not in the tree, but are read the same way.
    const int info = FindInfo(*target);
    if (info >= 0) return PropOfInfo(info, prop, data, dataSize);

    const int node = Find(*target);
    if (node < 0) return ABYSSO_ERROR_UNKNOWN_GUID;
    const ItemDef& item = At(node);

    switch (prop)
    {
    case kAbyssoPropType:
        return Write<int32_t>(data, dataSize, item.type);
    case kAbyssoPropCaption:
        return Write<const char*>(data, dataSize, item.caption.Get(lang_));
    case kAbyssoPropDescription:
        return Write<const char*>(data, dataSize, item.description.Get(lang_));
    case kAbyssoPropCurrentValue:
        return WriteCurrent(node, data, dataSize);
    case kAbyssoPropDefaultValue:
        return WriteDefault(node, data, dataSize);
    case kAbyssoPropMinValue:
        if (item.hasIntRange)    return Write<int32_t>(data, dataSize, item.minInt);
        if (item.hasDoubleRange) return Write<double>(data, dataSize, item.minDouble);
        return ABYSSO_ERROR_NOT_SUPPORTED;
    case kAbyssoPropMaxValue:
        if (item.type == kAbyssoParamItemTypeString && item.maxLength > 0)
            return Write<int32_t>(data, dataSize, item.maxLength);
        if (item.hasIntRange)    return Write<int32_t>(data, dataSize, item.maxInt);
        if (item.hasDoubleRange) return Write<double>(data, dataSize, item.maxDouble);
        return ABYSSO_ERROR_NOT_SUPPORTED;
    case kAbyssoPropStep:
        if (item.hasIntRange)    return Write<int32_t>(data, dataSize, item.stepInt);
        if (item.hasDoubleRange) return Write<double>(data, dataSize, item.stepDouble);
        return ABYSSO_ERROR_NOT_SUPPORTED;
    default:
        return ABYSSO_ERROR_NOT_SUPPORTED;
    }
}

inline int32_t AddonKit::Set(const AbyssoGuid* target, const void* data, int32_t dataSize)
{
    if (target == nullptr || data == nullptr) return ABYSSO_ERROR_INVALID_ARG;

    const int node = Find(*target);
    if (node < 0) return ABYSSO_ERROR_UNKNOWN_GUID;

    ItemDef& item = At(node);
    if (IsReadOnly(item)) return ABYSSO_ERROR_READONLY;
    if (!HoldsValue(item)) return ABYSSO_ERROR_NOT_SUPPORTED; // ComboBoxItem

    switch (item.type)
    {
    case kAbyssoParamItemTypeComboBox:
    {
        if (dataSize < static_cast<int32_t>(sizeof(AbyssoGuid)))
            return ABYSSO_ERROR_BUFFER_TOO_SMALL;
        const int chosen = Find(*static_cast<const AbyssoGuid*>(data));
        if (chosen < 0 || At(chosen).parent != node) return ABYSSO_ERROR_UNKNOWN_GUID;
        if (item.bindInt != nullptr) *item.bindInt = OrdinalOf(chosen);
        break;
    }
    case kAbyssoParamItemTypeCheckBox:
    {
        if (dataSize < static_cast<int32_t>(sizeof(int32_t)))
            return ABYSSO_ERROR_BUFFER_TOO_SMALL;
        if (item.bindInt != nullptr)
            *item.bindInt = (*static_cast<const int32_t*>(data) != 0) ? 1 : 0;
        break;
    }
    case kAbyssoParamItemTypeInt32:
    {
        if (dataSize < static_cast<int32_t>(sizeof(int32_t)))
            return ABYSSO_ERROR_BUFFER_TOO_SMALL;
        int32_t value = *static_cast<const int32_t*>(data);
        // AddonKit clamps the value when a range has been registered.
        if (item.hasIntRange) value = std::clamp(value, item.minInt, item.maxInt);
        if (item.bindInt != nullptr) *item.bindInt = value;
        break;
    }
    case kAbyssoParamItemTypeDouble:
    {
        if (dataSize < static_cast<int32_t>(sizeof(double)))
            return ABYSSO_ERROR_BUFFER_TOO_SMALL;
        double value = *static_cast<const double*>(data);
        if (item.hasDoubleRange) value = std::clamp(value, item.minDouble, item.maxDouble);
        if (item.bindDouble != nullptr) *item.bindDouble = value;
        break;
    }
    case kAbyssoParamItemTypeString:
    {
        if (data == nullptr) return ABYSSO_ERROR_INVALID_ARG;
        const char* text = static_cast<const char*>(data);
        std::string value(text, text + std::strlen(text));
        TruncateUtf8(value, item.maxLength);
        if (item.bindString != nullptr) *item.bindString = value;
        break;
    }
    default:
        return ABYSSO_ERROR_NOT_SUPPORTED;
    }

    int32_t change = item.redraw ? ABYSSO_CHANGE_REDRAW : ABYSSO_CHANGE_NONE;
    if (item.onChange) change |= item.onChange();

    // kNoRedraw is internal to AddonKit and is never returned to the host.
    if ((change & kNoRedraw) != 0) change &= ~(kNoRedraw | ABYSSO_CHANGE_REDRAW);
    return change;
}

inline void AddonKit::ApplyDefault(ItemDef& item)
{
    switch (item.type)
    {
    case kAbyssoParamItemTypeComboBox:
    case kAbyssoParamItemTypeCheckBox:
    case kAbyssoParamItemTypeInt32:
        if (item.bindInt != nullptr) *item.bindInt = item.defInt;
        break;
    case kAbyssoParamItemTypeDouble:
        if (item.bindDouble != nullptr) *item.bindDouble = item.defDouble;
        break;
    case kAbyssoParamItemTypeString:
        if (item.bindString != nullptr) *item.bindString = item.defString;
        break;
    default:
        break;
    }
}

inline int32_t AddonKit::Reset(const AbyssoGuid* target)
{
    // A nullptr target resets every item; OnReset is not invoked.
    if (target == nullptr)
    {
        bool any = false;
        for (ItemDef& item : items_)
        {
            if (!HoldsValue(item)) continue;
            ApplyDefault(item);
            any = true;
        }
        if (!any) return ABYSSO_CHANGE_NONE;
        return ABYSSO_CHANGE_REDRAW | ABYSSO_CHANGE_PROPS;
    }

    const int node = Find(*target);
    if (node < 0) return ABYSSO_ERROR_UNKNOWN_GUID;

    ItemDef& item = At(node);
    if (IsReadOnly(item)) return ABYSSO_ERROR_READONLY;
    if (!HoldsValue(item)) return ABYSSO_ERROR_NOT_SUPPORTED; // ComboBoxItem

    ApplyDefault(item);
    int32_t change = item.redraw ? ABYSSO_CHANGE_REDRAW : ABYSSO_CHANGE_NONE;
    if (item.onReset) change |= item.onReset();
    return change;
}

inline const char* AddonKit::ChoiceCaption(const AbyssoGuid& combo, int32_t index) const
{
    const int node = Find(combo);
    if (node < 0) return "";
    int child = NthChild(node, index);
    if (child < 0) child = NthChild(node, 0);
    return child >= 0 ? At(child).caption.Get(lang_) : "";
}

inline int32_t AddonKit::InfoItemCount() const
{
    return static_cast<int32_t>(infoItems_.size());
}

inline int32_t AddonKit::InfoItem(int32_t index, AbyssoGuid* out) const
{
    if (out == nullptr || index < 0 || index >= InfoItemCount())
        return ABYSSO_ERROR_INVALID_ARG;
    *out = infoItems_[static_cast<size_t>(index)].guid;
    return 0;
}

// ---------------------------------------------------------------------------
// Node --- registration handle
// ---------------------------------------------------------------------------

inline const AbyssoGuid& AddonKit::Node::Guid() const { return kit_->At(index_).guid; }

inline AddonKit::Node AddonKit::Node::Group(const AbyssoGuid& guid, Text caption)
{
    return Node(kit_, kit_->Add(index_, kAbyssoParamItemTypeGroup, guid, caption));
}

inline AddonKit::Node AddonKit::Node::Combo(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeComboBox, guid, caption);
    kit_->At(node).bindInt = bind;
    return Node(kit_, node);
}

inline AddonKit::Node AddonKit::Node::Check(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeCheckBox, guid, caption);
    kit_->At(node).bindInt = bind;
    return Node(kit_, node);
}

inline AddonKit::Node AddonKit::Node::Int(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeInt32, guid, caption);
    kit_->At(node).bindInt = bind;
    return Node(kit_, node);
}

inline AddonKit::Node AddonKit::Node::Double(const AbyssoGuid& guid, Text caption, double* bind)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeDouble, guid, caption);
    kit_->At(node).bindDouble = bind;
    return Node(kit_, node);
}

inline AddonKit::Node AddonKit::Node::String(const AbyssoGuid& guid, Text caption,
                                             std::string* bind)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeString, guid, caption);
    kit_->At(node).bindString = bind;
    return Node(kit_, node);
}

inline AddonKit::Node AddonKit::Node::Static(const AbyssoGuid& guid, Text caption,
                                             std::function<std::string()> value)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeStatic, guid, caption);
    kit_->At(node).staticValue = std::move(value);
    return Node(kit_, node);
}

// The text serves as both caption and description; the UI reads Description.
inline AddonKit::Node AddonKit::Node::Note(const AbyssoGuid& guid, Text text)
{
    const int node = kit_->Add(index_, kAbyssoParamItemTypeDescription, guid, text);
    kit_->At(node).description = text;
    return Node(kit_, node);
}

// Returns the ComboBox itself, so .Item().Item().Default(1) can be chained.
inline AddonKit::Node AddonKit::Node::Item(const AbyssoGuid& guid, Text caption)
{
    kit_->Add(index_, kAbyssoParamItemTypeComboBoxItem, guid, caption);
    return *this;
}

inline AddonKit::Node AddonKit::Node::Default(int32_t value)
{
    kit_->At(index_).defInt = value;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Default(double value)
{
    kit_->At(index_).defDouble = value;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Default(const std::string& value)
{
    kit_->At(index_).defString = value;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Range(int32_t min, int32_t max)
{
    ItemDef& item = kit_->At(index_);
    item.hasIntRange = true;
    item.minInt = min;
    item.maxInt = max;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Range(double min, double max)
{
    ItemDef& item = kit_->At(index_);
    item.hasDoubleRange = true;
    item.minDouble = min;
    item.maxDouble = max;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Step(int32_t step)
{
    kit_->At(index_).stepInt = step;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Step(double step)
{
    kit_->At(index_).stepDouble = step;
    return *this;
}

inline AddonKit::Node AddonKit::Node::MaxLength(int32_t chars)
{
    kit_->At(index_).maxLength = chars;
    return *this;
}

inline AddonKit::Node AddonKit::Node::Desc(Text description)
{
    kit_->At(index_).description = description;
    return *this;
}

inline AddonKit::Node AddonKit::Node::ReadOnly(bool value)
{
    kit_->At(index_).readOnly = value;
    return *this;
}

inline AddonKit::Node AddonKit::Node::EnabledIf(std::function<bool()> predicate)
{
    kit_->At(index_).enabledIf = std::move(predicate);
    return *this;
}

// On an item that holds a value, a conditional lock is registered as EnabledIf
// instead. The host excludes read-only items when it saves a project file, so a
// value locked at the moment of saving would be lost; a disabled item locks the
// control without that consequence. An existing EnabledIf is kept: the item is
// enabled only while both allow it.
inline AddonKit::Node AddonKit::Node::ReadOnlyIf(std::function<bool()> predicate)
{
    ItemDef& item = kit_->At(index_);
    if (!kit_->HoldsValue(item))
    {
        item.readOnlyIf = std::move(predicate);
        return *this;
    }

    if (item.enabledIf)
    {
        item.enabledIf = [was = std::move(item.enabledIf), lock = std::move(predicate)]
                         { return was() && !lock(); };
    }
    else
    {
        item.enabledIf = [lock = std::move(predicate)] { return !lock(); };
    }
    return *this;
}

inline AddonKit::Node AddonKit::Node::Redraw(bool value)
{
    kit_->At(index_).redraw = value;
    return *this;
}

inline AddonKit::Node AddonKit::Node::OnChange(std::function<int32_t()> handler)
{
    kit_->At(index_).onChange = std::move(handler);
    return *this;
}

inline AddonKit::Node AddonKit::Node::OnReset(std::function<int32_t()> handler)
{
    kit_->At(index_).onReset = std::move(handler);
    return *this;
}

// --- Top-level registration, delegated to Node ---

inline AddonKit::Node AddonKit::Root() { return Node(this, -1); }

inline AddonKit::Node AddonKit::Group(const AbyssoGuid& guid, Text caption)
{
    return Root().Group(guid, caption);
}

inline AddonKit::Node AddonKit::Combo(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    return Root().Combo(guid, caption, bind);
}

inline AddonKit::Node AddonKit::Check(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    return Root().Check(guid, caption, bind);
}

inline AddonKit::Node AddonKit::Int(const AbyssoGuid& guid, Text caption, int32_t* bind)
{
    return Root().Int(guid, caption, bind);
}

inline AddonKit::Node AddonKit::Double(const AbyssoGuid& guid, Text caption, double* bind)
{
    return Root().Double(guid, caption, bind);
}

inline AddonKit::Node AddonKit::String(const AbyssoGuid& guid, Text caption, std::string* bind)
{
    return Root().String(guid, caption, bind);
}

inline AddonKit::Node AddonKit::Static(const AbyssoGuid& guid, Text caption,
                                       std::function<std::string()> value)
{
    return Root().Static(guid, caption, std::move(value));
}

inline AddonKit::Node AddonKit::Note(const AbyssoGuid& guid, Text text)
{
    return Root().Note(guid, text);
}

} // namespace abysso

// ===========================================================================
// Macros that define the exported functions
//
// No exception leaves the C ABI boundary: every function generated here catches
// exceptions with catch (...) and converts them into an error code.
// ===========================================================================

#define ABYSSO_KIT_GUARD(expr, onError)                                            \
    try { return (expr); } catch (...) { return (onError); }

#define ABYSSO_KIT_GUARD_VOID(expr)                                                \
    try { (expr); } catch (...) { }

// --- Two of the three required functions -----------------------------------

#define ABYSSO_ADDON_ABI_VERSION_IMPL()                                            \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonAbiVersion(void)                        \
    {                                                                              \
        return ABYSSO_ADDON_ABI_VERSION;                                           \
    }

// Identification. name and version are expressions, so a name that depends on
// the language still fits in this single line.
//
//   ABYSSO_ADDON_INFO_IMPL(kAddonGuid,
//                          Kit().Japanese() ? "ジュリア集合" : "Julia set",
//                          "1.0.0.0")
#define ABYSSO_ADDON_INFO_IMPL(guidExpr, nameExpr, versionExpr)                    \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonInfo(AbyssoAddonInfo* out)              \
    {                                                                              \
        if (out == nullptr) return ABYSSO_ERROR_INVALID_ARG;                       \
        if (out->structSize < static_cast<int32_t>(sizeof(AbyssoAddonInfo)))       \
            return ABYSSO_ERROR_BUFFER_TOO_SMALL;                                  \
        try                                                                        \
        {                                                                          \
            const AbyssoGuid abysso_guid_ = (guidExpr);                            \
            std::memcpy(out->guid.bytes, abysso_guid_.bytes, 16);                  \
            out->name    = (nameExpr);                                             \
            out->version = (versionExpr);                                          \
            return 0;                                                              \
        }                                                                          \
        catch (...) { return ABYSSO_ERROR_INVALID_ARG; }                           \
    }

// --- Optional exported functions -------------------------------------------
//
//   ABYSSO_ADDON_EXPORT_PARAMS ... item tree and value access
//   ABYSSO_ADDON_EXPORT_PROP   ... required by both PARAMS and INFO
//   ABYSSO_ADDON_EXPORT_INFO   ... details overlay items
//   ABYSSO_ADDON_EXPORT_LANG   ... display language
//   ABYSSO_ADDON_EXPORT_CANCEL ... render cancellation
//   ABYSSO_ADDON_EXPORT_ALL    ... all five of the above

#define ABYSSO_ADDON_EXPORT_PARAMS(kitExpr)                                        \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItemCount(const AbyssoGuid* t)\
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).ChildItemCount(t), 0)                           \
    }                                                                              \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonParamChildItem(const AbyssoGuid* t,     \
                                                          int32_t index,           \
                                                          AbyssoParamItem* out)    \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).ChildItem(t, index, out), ABYSSO_ERROR_INVALID_ARG) \
    }                                                                              \
    ABYSSO_ADDON_API int32_t SetAbyssoAddonValue(const AbyssoGuid* t,              \
                                                 const void* data, int32_t size)   \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).Set(t, data, size), ABYSSO_ERROR_INVALID_ARG)   \
    }                                                                              \
    ABYSSO_ADDON_API int32_t ResetAbyssoAddonValue(const AbyssoGuid* t)            \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).Reset(t), ABYSSO_ERROR_INVALID_ARG)             \
    }

#define ABYSSO_ADDON_EXPORT_PROP(kitExpr)                                          \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonProp(const AbyssoGuid* t, int32_t prop,  \
                                                void* data, int32_t size)          \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).Prop(t, prop, data, size), ABYSSO_ERROR_INVALID_ARG) \
    }

#define ABYSSO_ADDON_EXPORT_INFO(kitExpr)                                          \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItemCount(void)                     \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).InfoItemCount(), 0)                             \
    }                                                                              \
    ABYSSO_ADDON_API int32_t GetAbyssoAddonInfoItem(int32_t index, AbyssoGuid* out) \
    {                                                                              \
        ABYSSO_KIT_GUARD((kitExpr).InfoItem(index, out), ABYSSO_ERROR_INVALID_ARG) \
    }

#define ABYSSO_ADDON_EXPORT_LANG(kitExpr)                                          \
    ABYSSO_ADDON_API void SetAbyssoAddonLang(const char* bcp47)                    \
    {                                                                              \
        ABYSSO_KIT_GUARD_VOID((kitExpr).SetLang(bcp47))                            \
    }

#define ABYSSO_ADDON_EXPORT_CANCEL(kitExpr)                                        \
    ABYSSO_ADDON_API void CancelAbyssoAddonFrame(void)                             \
    {                                                                              \
        ABYSSO_KIT_GUARD_VOID((kitExpr).RequestCancel())                           \
    }

#define ABYSSO_ADDON_EXPORT_ALL(kitExpr)                                           \
    ABYSSO_ADDON_EXPORT_PARAMS(kitExpr)                                            \
    ABYSSO_ADDON_EXPORT_PROP(kitExpr)                                              \
    ABYSSO_ADDON_EXPORT_INFO(kitExpr)                                              \
    ABYSSO_ADDON_EXPORT_LANG(kitExpr)                                              \
    ABYSSO_ADDON_EXPORT_CANCEL(kitExpr)
