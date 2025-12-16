/*******************************************************************************
 * Copyright 2023 MINRES Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#pragma once

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <systemc>
#include <type_traits>
#include <vector>
// clang-format off
#include <nonstd/string_view.hpp>
#include <nonstd/variant.hpp>
// clang-format on

namespace lwtr {
struct no_data {};
struct value;
using object = std::vector<std::pair<std::string, value>>;
using value_base = nonstd::variant<no_data, std::string, char const*, double, bool, uint64_t, int64_t, sc_dt::sc_bv_base, sc_dt::sc_lv_base,
                                   sc_core::sc_time, object>;
struct value : public value_base {
    inline value() noexcept
    : value_base() {}
    template <typename Arg>
    inline value(Arg&& arg) noexcept
    : value_base(arg) {}
};

/// http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
/// \todo Fails to compile for unions
template <typename T> class has_record_member {
    struct Fallback {
        int record;
    };
    template <typename U, bool really_is_class = std::is_class<U>::value> struct Derived : U, Fallback {};
    template <typename U> struct Derived<U, false> : Fallback {};

    template <typename U, U> struct Check;

    typedef char ArrayOfOne[1];
    typedef char ArrayOfTwo[2];

    template <typename U> static ArrayOfOne& func(Check<int Fallback::*, &U::record>*);
    template <typename U> static ArrayOfTwo& func(...);

public:
    typedef has_record_member type;
    enum { value = sizeof(func<Derived<T>>(0)) == 2 };
};

template <typename T> struct key_value {
    std::string key;
    T const& value;
};

template <typename T> key_value<T> field(std::string const& k, T const& v) { return {k, v}; }

template <typename T> value record(T const& t);
template <typename T> value record(T& t);

class access {
    object o;

public:
    access() { o.reserve(64); }
    access(access const& o) = delete;
    access(access&& o) = delete;
    access& operator=(access const& o) = delete;
    access& operator=(access&& o) = delete;
    ~access() = default;

    template <typename T> access& operator&(key_value<T> const& kv) {
        o.emplace_back(std::make_pair(kv.key, record(kv.value)));
        return *this;
    }
    inline value get_value() const { return value(o); }
};

template <typename T, class Enable = void> struct value_converter {
    static value to_value(T const& v) {
        access a;
        record(a, v);
        return a.get_value();
    }

    static value to_value(T& v) {
        access a;
        record(a, v);
        return a.get_value();
    }
};
/// partial specialisations
template <typename T> struct value_converter<T, typename std::enable_if<std::is_const<T>::value>::type> {
    static value to_value(T& v) { return value_converter<typename std::remove_const<T>::type>::to_value(v); }
};

template <typename T> struct value_converter<T, typename std::enable_if<has_record_member<T>::value>::type> {
    static value to_value(T const& v) {
        access a;
        v.record(a);
        return a.get_value();
    }

    static value to_value(T& v) {
        access a;
        v.record(a);
        return a.get_value();
    }
};
template <> struct value_converter<no_data> {
    static value to_value(no_data v) { return value(); }
};
/// standard types
#define VAL_CONV(T)                                                                                                                        \
    template <> struct value_converter<T> {                                                                                                \
        static value to_value(T v) { return value(v); }                                                                                    \
    }
#define VAL_CONV2(T, T2)                                                                                                                   \
    template <> struct value_converter<T> {                                                                                                \
        static value to_value(T v) { return value(static_cast<T2>(v)); }                                                                   \
    }
VAL_CONV(char const*);
VAL_CONV(std::string);
VAL_CONV(double);
VAL_CONV(bool);
#if !defined(_MSC_VER)
VAL_CONV(int64_t);
#endif
VAL_CONV2(int32_t, int64_t);
VAL_CONV2(int16_t, int64_t);
VAL_CONV2(int8_t, int64_t);
#if !defined(_MSC_VER)
VAL_CONV(uint64_t);
#endif
VAL_CONV2(uint32_t, uint64_t);
VAL_CONV2(uint16_t, uint64_t);
VAL_CONV2(uint8_t, uint64_t);
VAL_CONV(sc_core::sc_time);

template <typename T> value record(T const& t) { return value_converter<T>::to_value(t); }

template <typename T> value record(T& t) { return value_converter<T>::to_value(t); }
#if !defined(__clang__) && !defined(__APPLE__)
template <> struct value_converter<sc_dt::uint64> {
    static value to_value(sc_dt::uint64 v) { return value(static_cast<uint64_t>(v)); }
};
template <> struct value_converter<sc_dt::int64> {
    static value to_value(sc_dt::int64 v) { return value(static_cast<int64_t>(v)); }
};
#endif
template <int W> struct value_converter<sc_dt::sc_uint<W>> {
    static value to_value(sc_dt::sc_uint<W> v) { return value(static_cast<uint64_t>(v)); }
};
template <int W> struct value_converter<sc_dt::sc_int<W>> {
    static value to_value(sc_dt::sc_int<W> v) { return value(static_cast<int64_t>(v)); }
};
template <int W> struct value_converter<sc_dt::sc_biguint<W>> {
    static value to_value(sc_dt::sc_biguint<W> v) { return value(static_cast<uint64_t>(v)); }
};
template <int W> struct value_converter<sc_dt::sc_bigint<W>> {
    static value to_value(sc_dt::sc_bigint<W> v) { return value(static_cast<int64_t>(v)); }
};
template <> struct value_converter<sc_dt::sc_bit> {
    static value to_value(sc_dt::sc_bit v) { return value(sc_dt::sc_bv<1>(v)); }
};
template <> struct value_converter<sc_dt::sc_logic> {
    static value to_value(sc_dt::sc_logic v) { return value(sc_dt::sc_lv<1>(v)); }
};
template <int W> struct value_converter<sc_dt::sc_bv<W>> {
    static value to_value(sc_dt::sc_bv<W> const& v) { return value(v); }
};
template <int W> struct value_converter<sc_dt::sc_lv<W>> {
    static value to_value(sc_dt::sc_lv<W> const& v) { return value(v); }
};
#ifdef SC_INCLUDE_FX
template <int W, int I, sc_dt::sc_q_mode Q, sc_dt::sc_o_mode O, int N> struct value_converter<sc_dt::sc_fixed<W, I, Q, O, N>> {
    static value to_value(sc_dt::sc_fixed<W, I, Q, O, N> const& v) { return value(v.to_double()); }
};
template <int W, int I, sc_dt::sc_q_mode Q, sc_dt::sc_o_mode O, int N> struct value_converter<sc_dt::sc_fixed_fast<W, I, Q, O, N>> {
    static value to_value(sc_dt::sc_fixed_fast<W, I, Q, O, N> const& v) { return value(v.to_double()); }
};
template <int W, int I, sc_dt::sc_q_mode Q, sc_dt::sc_o_mode O, int N> struct value_converter<sc_dt::sc_ufixed<W, I, Q, O, N>> {
    static value to_value(sc_dt::sc_ufixed<W, I, Q, O, N> const& v) { return value(v.to_double()); }
};
template <int W, int I, sc_dt::sc_q_mode Q, sc_dt::sc_o_mode O, int N> struct value_converter<sc_dt::sc_ufixed_fast<W, I, Q, O, N>> {
    static value to_value(sc_dt::sc_ufixed_fast<W, I, Q, O, N> const& v) { return value(v.to_double()); }
};
#endif

using tx_relation_handle = uint64_t;
enum callback_reason { CREATE, DELETE, SUSPEND, RESUME, BEGIN, END };
class tx_handle;
class tx_generator_base;

class tx_db {
    struct impl;
    std::unique_ptr<impl> pimpl;
    bool enable{true};

public:
    tx_db(std::string const& recording_file_name, sc_core::sc_time_unit = sc_core::SC_FS);

    virtual ~tx_db();

    static void set_default_db(tx_db*);

    static tx_db* get_default_db();

    using tx_db_class_cb = std::function<void(const tx_db&, callback_reason)>;
    static uint64_t register_class_cb(tx_db_class_cb);

    static void unregister_class_cb(uint64_t);

    std::string const& get_name() const;

    void set_recording(bool en) { enable = en; }

    bool get_recording() const { return enable; }

    tx_relation_handle create_relation(const char* relation_name) const;

    tx_relation_handle create_relation(std::string const& relation_name) const { return create_relation(relation_name.c_str()); }

    std::string const& get_relation_name(tx_relation_handle relation_h) const;
};

class tx_fiber {
    struct impl;
    std::unique_ptr<impl> pimpl;
    const std::string fiber_name;
    const std::string fiber_kind;
    tx_db const* db;
    uint64_t const id;

public:
    tx_fiber(std::string const& fiber_name, std::string const& fiber_kind, tx_db* tx_db_p = tx_db::get_default_db())
    : tx_fiber(fiber_name.c_str(), fiber_kind.c_str(), tx_db_p) {}

    tx_fiber(const char* fiber_name, const char* fiber_kind, tx_db* tx_db_p = tx_db::get_default_db());

    virtual ~tx_fiber();

    using tx_fiber_class_cb = std::function<void(const tx_fiber&, callback_reason)>;
    static uint64_t register_class_cb(tx_fiber_class_cb);

    static void unregister_class_cb(uint64_t);

    std::string const& get_name() const { return fiber_name; }

    std::string const& get_fiber_kind() const { return fiber_kind; }

    uint64_t get_id() const { return id; }

    tx_db const* get_tx_db() const { return db; }
};

class tx_generator_base {
    struct impl;
    std::unique_ptr<impl> pimpl;
    tx_fiber const& fiber;
    std::string const generator_name;
    std::string const begin_attr_name;
    std::string const end_attr_name;
    uint64_t const id;

public:
    tx_generator_base(std::string name, tx_fiber& s, std::string begin_attribute_name = "", std::string end_attribute_name = "");

    virtual ~tx_generator_base();

    std::string const& get_begin_attribute_name() const { return begin_attr_name; }

    std::string const& get_end_attribute_name() const { return end_attr_name; }

    using tx_generator_class_cb = std::function<void(tx_generator_base const&, callback_reason)>;
    static uint64_t register_class_cb(tx_generator_class_cb);

    static void unregister_class_cb(uint64_t);

    std::string const& get_name() const { return generator_name; }

    uint64_t get_id() const { return id; }

    tx_fiber const& get_tx_fiber() const { return fiber; }

protected:
    friend class tx_handle;
    tx_handle begin_tx(value const&, sc_core::sc_time const&, tx_relation_handle, tx_handle const* = nullptr) const;
    void end_tx(tx_handle&, value const&, sc_core::sc_time const&) const;
};

class tx_handle {
    struct impl;
    std::shared_ptr<impl> pimpl;
    friend class tx_generator_base;
    tx_handle(const tx_generator_base& gen, value const& v, sc_core::sc_time const& t);
    void deactivate(value const& v, sc_core::sc_time const& t);
    void end_tx(const value& v, sc_core::sc_time const& end_sc_time);

public:
    tx_handle() {}

    tx_handle(tx_handle const& o)
    : pimpl(o.pimpl) {}

    bool is_valid() const { return pimpl != nullptr; }

    bool is_active() const;

    uint64_t get_id() const;

    void end_tx() { end_tx(value(), sc_core::sc_time_stamp()); }

    template <typename END> void end_tx(const END& attr) { end_tx(record(attr), sc_core::sc_time_stamp()); }

    void end_tx_delayed(sc_core::sc_time const& end_time) { end_tx(value(), end_time); }

    template <typename END> void end_tx_delayed(sc_core::sc_time const& end_time, const END& attr) { end_tx(record(attr), end_time); }

    void record_attribute(char const* name, value const& attr);

    template <typename T> void record_attribute(std::string const& name, const T& attr) { record_attribute(name.c_str(), record(attr)); }

    template <typename T> void record_attribute(const char* name, const T& attr) { record_attribute(name, record(attr)); }

    template <typename T> void record_attribute(const T& attr) { record_attribute(nullptr, record(attr)); }

    using tx_handle_class_cb = std::function<void(const tx_handle&, callback_reason, value const&)>;
    static uint64_t register_class_cb(tx_handle_class_cb);

    static void unregister_class_cb(uint64_t);

    using tx_handle_attribute_cb = std::function<void(tx_handle const&, char const*, value const&)>;
    static uint64_t register_record_attribute_cb(tx_handle_attribute_cb);

    static void unregister_record_attribute_cb(uint64_t);

    using tx_handle_relation_cb = std::function<void(tx_handle const&, tx_handle const&, tx_relation_handle)>;
    static uint64_t register_relation_cb(tx_handle_relation_cb);

    static void unregister_relation_cb(uint64_t);

    bool add_relation(tx_relation_handle, const tx_handle&);

    bool add_relation(const char* relation_name, const tx_handle& other_tx_h) {
        return add_relation(get_tx_fiber().get_tx_db()->create_relation(relation_name), other_tx_h);
    };

    bool add_relation(std::string const& relation_name, const tx_handle& other_tx_h) {
        return add_relation(get_tx_fiber().get_tx_db()->create_relation(relation_name), other_tx_h);
    };

    sc_core::sc_time get_begin_sc_time() const;

    sc_core::sc_time get_end_sc_time() const;

    tx_fiber const& get_tx_fiber() const;

    tx_generator_base const& get_tx_generator_base() const;
};

template <typename BEGIN = no_data, typename END = no_data> class tx_generator : public tx_generator_base {
public:
    tx_generator(const char* name, tx_fiber& s)
    : tx_generator_base(name, s, "", "") {}

    tx_generator(const char* name, tx_fiber& s, std::string const& attribute_name)
    : tx_generator(name, s, attribute_name, std::is_same<BEGIN, no_data>{}) {}

    tx_generator(const char* name, tx_fiber& s, std::string const& begin_attribute_name, std::string const& end_attribute_name)
    : tx_generator_base(name, s, begin_attribute_name, end_attribute_name) {}

    tx_generator() = default;

    virtual ~tx_generator() = default;

    tx_handle begin_tx() { return tx_generator_base::begin_tx(value(), sc_core::sc_time_stamp(), 0); }

    tx_handle begin_tx(tx_relation_handle relation_h, tx_handle const& other_tx_h) {
        return tx_generator_base::begin_tx(value(), sc_core::sc_time_stamp(), relation_h, &other_tx_h);
    }

    tx_handle begin_tx(const char* relation_name, tx_handle const& other_tx_h) {
        return tx_generator_base::begin_tx(value(), sc_core::sc_time_stamp(), get_tx_fiber().get_tx_db()->create_relation(relation_name),
                                           &other_tx_h);
    }

    tx_handle begin_tx(BEGIN const& begin_attr) {
        auto v = ::lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, sc_core::sc_time_stamp(), 0);
    }

    tx_handle begin_tx(const BEGIN& begin_attr, tx_relation_handle relation_h, const tx_handle& other_tx_h) {
        auto v = lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, sc_core::sc_time_stamp(), relation_h, &other_tx_h);
    }

    tx_handle begin_tx(const BEGIN& begin_attr, const char* relation_name, const tx_handle& other_tx_h) {
        auto v = lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, sc_core::sc_time_stamp(), get_tx_fiber().get_tx_db()->create_relation(relation_name),
                                           &other_tx_h);
    }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time) { return tx_generator_base::begin_tx(value(), begin_sc_time, 0); }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time, tx_relation_handle relation_h, const tx_handle& other_tx_h) {
        return tx_generator_base::begin_tx(value(), begin_sc_time, relation_h, &other_tx_h);
    }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time, const char* relation_name, const tx_handle& other_tx_h) {
        return tx_generator_base::begin_tx(value(), begin_sc_time, get_tx_fiber().get_tx_db()->create_relation(relation_name), &other_tx_h);
    }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time, const BEGIN& begin_attr) {
        auto v = lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, begin_sc_time, 0);
    }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time, const BEGIN& begin_attr, tx_relation_handle relation_h,
                               const tx_handle& other_tx_h) {
        auto v = lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, begin_sc_time, relation_h, &other_tx_h);
    }

    tx_handle begin_tx_delayed(sc_core::sc_time const& begin_sc_time, const BEGIN& begin_attr, const char* relation_name,
                               const tx_handle& other_tx_h) {
        auto v = lwtr::record(begin_attr);
        return tx_generator_base::begin_tx(v, begin_sc_time, get_tx_fiber().get_tx_db()->create_relation(relation_name), &other_tx_h);
    }

    void end_tx(tx_handle& t) { tx_generator_base::end_tx(t, value(), sc_core::sc_time_stamp()); }

    void end_tx(tx_handle& t, const END& end_attr) {
        auto v = ::lwtr::record(end_attr);
        tx_generator_base::end_tx(t, v, sc_core::sc_time_stamp());
    }

    void end_tx_delayed(tx_handle& t, sc_core::sc_time const& end_sc_time) { tx_generator_base::end_tx(t, value(), end_sc_time); }

    void end_tx_delayed(tx_handle& t, sc_core::sc_time const& end_sc_time, const END& end_attr) {
        auto v = lwtr::record(end_attr);
        tx_generator_base::end_tx(t, v, end_sc_time);
    }

private:
    tx_generator(const char* name, tx_fiber& s, std::string const& attribute_name, std::true_type)
    : tx_generator_base(name, s, attribute_name, "") {}

    tx_generator(const char* name, tx_fiber& s, std::string const& attribute_name, std::false_type)
    : tx_generator_base(name, s, "", attribute_name) {}
};

void tx_text_init();

void tx_text_gz_init();

void tx_text_lz4_init();

void tx_ftr_init(bool compressed);
} // namespace lwtr
