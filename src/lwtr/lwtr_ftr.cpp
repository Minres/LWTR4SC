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

#include "lwtr.h"
#include <cstring>
#include <ftr/ftr_writer.h>
#include <numeric>
#include <sstream>
#include <sysc/utils/sc_report.h>

namespace lwtr {
namespace {
// ----------------------------------------------------------------------------
template <typename WRITER> struct Writer {
    std::unique_ptr<WRITER> output_writer;
    Writer(const std::string& name)
    : output_writer(new WRITER(name)) {}

    Writer() {}

    inline bool open(const std::string& name) {
        output_writer.reset(new WRITER(name));
        return output_writer->cw.enc.ofs.is_open();
    }

    inline void close() { output_writer.reset(nullptr); }

    inline bool is_open() { return output_writer->cw.enc.ofs.is_open(); }

    inline static WRITER& writer() { return *get().output_writer; }

    inline static Writer& get() {
        static Writer db;
        return db;
    }

    static inline void writeAttribute(uint64_t tx_id, ftr::event_type pos, nonstd::string_view const& name, value const& v) {
        char hier_full_name[1024] = {};
        if(name.length())
            strncpy(hier_full_name, name.data(), name.length());
        writeAttribute(tx_id, pos, v, hier_full_name, hier_full_name + name.length());
    }

private:
    static inline nonstd::string_view get_full_name(char const* hier_full_name, char* insert_point) {
        if(insert_point == hier_full_name)
            return "unnamed";
        *insert_point = 0;
        return nonstd::string_view(hier_full_name, insert_point - hier_full_name);
    }

    static void writeAttribute(uint64_t tx_id, ftr::event_type pos, value const& v, char const* hier_full_name, char* insert_point) {
        switch(v.index()) {
        case 0: // no data
            break;
        case 1: // std::string
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::STRING, nonstd::get<1>(v));
            break;
        case 2: // char*
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::STRING, nonstd::get<2>(v));
            break;
        case 3: // double
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::FLOATING_POINT_NUMBER,
                                    nonstd::get<3>(v));
            break;
        case 4: // bool
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::BOOLEAN, nonstd::get<4>(v));
            break;
        case 5: // uint64_t,
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::UNSIGNED, nonstd::get<5>(v));
            break;
        case 6: // int64_t,
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::INTEGER, nonstd::get<6>(v));
            break;
        case 7: // sc_dt::sc_bv_base
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::BIT_VECTOR,
                                    nonstd::get<7>(v).to_string());
            break;
        case 8: // sc_dt::sc_lv_base
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::LOGIC_VECTOR,
                                    nonstd::get<8>(v).to_string());
            break;
        case 9: // sc_core::sc_time
            writer().writeAttribute(tx_id, pos, get_full_name(hier_full_name, insert_point), ftr::data_type::TIME,
                                    nonstd::get<9>(v).value());
            break;
        case 10: // object
            for(auto& e : nonstd::get<10>(v)) {
                auto const& name = std::get<0>(e);
                auto old_insert_point = insert_point;
                if(insert_point != hier_full_name)
                    *insert_point = '.';
                auto res = strncpy(++insert_point, name.c_str(), name.length());
                insert_point += name.length();
                writeAttribute(tx_id, pos, std::get<1>(e), hier_full_name, insert_point);
                insert_point = old_insert_point;
            }
            break;
        }
    }
};
// ----------------------------------------------------------------------------
template <typename DB> void tx_db_cbf(tx_db const& _tx_db, callback_reason reason) {
    static std::string file_name("tx_default");
    switch(reason) {
    case CREATE: {
        if(_tx_db.get_name().length() != 0) {
            file_name = _tx_db.get_name();
        }
        file_name += ".ftr";
        if(Writer<DB>::get().open(file_name)) {
            double secs = sc_core::sc_time::from_value(1ULL).to_seconds();
            auto exp = rint(log(secs) / log(10.0));
            Writer<DB>::writer().writeInfo(static_cast<int8_t>(exp));
            std::stringstream ss;
            ss << "opening file " << file_name;
            SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
        } else {
            std::stringstream ss;
            ss << "Can't open text recording file. " << strerror(errno);
            SC_REPORT_ERROR(__FUNCTION__, ss.str().c_str());
        }
    } break;
    case DELETE: {
        std::stringstream ss;
        ss << "closing file " << file_name;
        SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
        Writer<DB>::get().close();
    } break;
    default:
        SC_REPORT_ERROR(__FUNCTION__, "Unknown reason in tx_db callback");
    }
}
// ----------------------------------------------------------------------------
template <typename DB> void tx_fiber_cbf(const tx_fiber& s, callback_reason reason) {
    if(Writer<DB>::get().is_open() && reason == CREATE) {
        Writer<DB>::writer().writeStream(s.get_id(), s.get_name(), s.get_fiber_kind());
    }
}
// ----------------------------------------------------------------------------
template <typename DB> void tx_generator_cbf(const tx_generator_base& g, callback_reason reason) {
    if(Writer<DB>::get().is_open() && reason == CREATE) {
        Writer<DB>::writer().writeGenerator(g.get_id(), g.get_name(), g.get_tx_fiber().get_id());
    }
}
// ----------------------------------------------------------------------------
template <typename DB> void tx_handle_cbf(const tx_handle& t, callback_reason reason, value const& v) {
    if(!Writer<DB>::get().is_open())
        return;
    if(t.get_tx_fiber().get_tx_db() == nullptr)
        return;
    if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
        return;
    switch(reason) {
    case BEGIN: {
        Writer<DB>::writer().startTransaction(t.get_id(), t.get_tx_generator_base().get_id(),
                                              t.get_tx_generator_base().get_tx_fiber().get_id(),
                                              t.get_begin_sc_time() / sc_core::sc_time(1, sc_core::SC_PS));
        Writer<DB>::writeAttribute(t.get_id(), ftr::event_type::BEGIN, t.get_tx_generator_base().get_begin_attribute_name(), v);
    } break;
    case END: {
        Writer<DB>::writeAttribute(t.get_id(), ftr::event_type::END, t.get_tx_generator_base().get_end_attribute_name(), v);
        Writer<DB>::writer().endTransaction(t.get_id(), t.get_end_sc_time() / sc_core::sc_time(1, sc_core::SC_PS));
    } break;
    default:;
    }
}
// ----------------------------------------------------------------------------
template <typename DB> void tx_handle_record_attribute_cbf(tx_handle const& t, const char* attribute_name, value const& v) {
    if(t.get_tx_fiber().get_tx_db() == nullptr)
        return;
    if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
        return;
    if(!Writer<DB>::get().is_open())
        return;
    Writer<DB>::writeAttribute(t.get_id(), ftr::event_type::RECORD, attribute_name == nullptr ? "" : attribute_name, v);
}
// ----------------------------------------------------------------------------
template <typename DB> void tx_handle_relation_cbf(const tx_handle& tr_1, const tx_handle& tr_2, tx_relation_handle relation_handle) {
    auto const& f_1 = tr_1.get_tx_fiber();
    if(f_1.get_tx_db() == nullptr)
        return;
    if(f_1.get_tx_db()->get_recording() == false)
        return;
    if(Writer<DB>::get().is_open()) {
        auto const& f_2 = tr_1.get_tx_fiber();
        Writer<DB>::writer().writeRelation(f_1.get_tx_db()->get_relation_name(relation_handle), f_1.get_id(), tr_1.get_id(), f_2.get_id(),
                                           tr_2.get_id());
    }
}
// ----------------------------------------------------------------------------
} // namespace
// ----------------------------------------------------------------------------
void tx_ftr_init(bool compressed) {
    if(compressed) {
        tx_db::register_class_cb(tx_db_cbf<ftr::ftr_writer<true>>);
        tx_fiber::register_class_cb(tx_fiber_cbf<ftr::ftr_writer<true>>);
        tx_generator_base::register_class_cb(tx_generator_cbf<ftr::ftr_writer<true>>);
        tx_handle::register_class_cb(tx_handle_cbf<ftr::ftr_writer<true>>);
        tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<ftr::ftr_writer<true>>);
        tx_handle::register_relation_cb(tx_handle_relation_cbf<ftr::ftr_writer<true>>);
    } else {
        tx_db::register_class_cb(tx_db_cbf<ftr::ftr_writer<false>>);
        tx_fiber::register_class_cb(tx_fiber_cbf<ftr::ftr_writer<false>>);
        tx_generator_base::register_class_cb(tx_generator_cbf<ftr::ftr_writer<false>>);
        tx_handle::register_class_cb(tx_handle_cbf<ftr::ftr_writer<false>>);
        tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<ftr::ftr_writer<false>>);
        tx_handle::register_relation_cb(tx_handle_relation_cbf<ftr::ftr_writer<false>>);
    }
}
} // namespace lwtr
// ----------------------------------------------------------------------------
