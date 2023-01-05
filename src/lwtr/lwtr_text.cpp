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

#include <array>
#include <cerrno>
#include <cstring>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/printf.h>
#include <sstream>
#include <string>

#include "lwtr.h"

namespace lwtr {
// ----------------------------------------------------------------------------
#define FPRINT(FP, FMTSTR)                                                                                             \
    auto buf1 = fmt::format(FMT_STRING(FMTSTR));                                                                       \
    std::fwrite(buf1.c_str(), 1, buf1.size(), FP);
#define FPRINTF(FP, FMTSTR, ...)                                                                                       \
    auto buf2 = fmt::format(FMT_STRING(FMTSTR), __VA_ARGS__);                                                          \
    std::fwrite(buf2.c_str(), 1, buf2.size(), FP);
// ----------------------------------------------------------------------------
static FILE* my_text_file_p = nullptr;
static void tx_db_cbf(tx_db const& _tx_db, callback_reason reason) {
    static std::string my_text_file_name("tx_default.txlog");
    switch(reason) {
    case CREATE:
        if(_tx_db.get_name().length() != 0) {
            my_text_file_name = _tx_db.get_name();
        }
        my_text_file_p = fopen(my_text_file_name.c_str(), "w");
        if(my_text_file_p == nullptr) {
            std::stringstream ss;
            ss << "Can't open text recording file. " << strerror(errno);
            SC_REPORT_ERROR(__FUNCTION__, ss.str().c_str());
        } else {
            std::stringstream ss;
            ss << "opening file " << my_text_file_name;
            SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
        }
        break;
    case DELETE:
        if(my_text_file_p != nullptr) {
            std::stringstream ss;
            ss << "closing file " << my_text_file_name;
            SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
            fclose(my_text_file_p);
            my_text_file_p = nullptr;
        }
        break;
    default:
        SC_REPORT_ERROR(__FUNCTION__, "Unknown reason in tx_db callback");
    }
}
// ----------------------------------------------------------------------------
static void tx_fiber_cbf(const tx_fiber& s, callback_reason reason) {
    if(reason == CREATE) {
        if(my_text_file_p == nullptr)
            return;
        FPRINTF(my_text_file_p, "scv_tr_stream (ID {}, name \"{}\", kind \"{}\")\n", s.get_id(), s.get_name(),
                s.get_fiber_kind().length() ? s.get_fiber_kind() : "<no_stream_kind>");
    }
}
// ----------------------------------------------------------------------------
struct value_visitor : public boost::static_visitor<> {
	uint64_t const tx_id;

	mutable std::vector<std::string> hier_name;

	value_visitor(uint64_t tx_id, std::string const& name):tx_id(tx_id) {
		hier_name.push_back(name);
	}

	std::string get_full_name() const {
	    std::ostringstream os;
	    auto b = std::begin(hier_name);
	    auto e = std::end(hier_name);
	    if(b != e) {
	        std::copy(b, std::prev(e), std::ostream_iterator<std::string>(os, "."));
	        b = std::prev(e);
	    }
	    if(b != e)
	        os << *b;
	    return os.str();
	}

	void operator()(const lwtr::no_data& v) const {	}
	void operator()(std::string const& v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" STRING = \"{}\"\n", tx_id, get_full_name(), v);
	}
	void operator()(double v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" FLOATING_POINT_NUMBER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(char const* v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" STRING = \"{}\"\n", tx_id, get_full_name(), v);
	}
	void operator()(bool v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" BOOLEAN = {}\n", tx_id, get_full_name(), v ? "true" : "false");
	}
	void operator()(uint8_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint16_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint32_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint64_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int8_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int16_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int32_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int64_t v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(sc_dt::sc_bv_base const& v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" BIT_VECTOR = \"{}\"\n", tx_id, get_full_name(), v.to_string());
	}
	void operator()(sc_dt::sc_lv_base const& v) const {
		FPRINTF(my_text_file_p, "tx_record_attribute {} \"{}\" LOGIC_VECTOR = \"{}\"\n", tx_id, get_full_name(), v.to_string());
	}
	void operator()(std::vector<std::pair<std::string, value>> const& v) const {
		for(auto& e:v) {
			hier_name.push_back(e.first);
			boost::apply_visitor( *this, e.second );
			hier_name.pop_back();
		}
	}
};
// ----------------------------------------------------------------------------
static void tx_generator_cbf(const tx_generator_base& g, callback_reason reason) {
    if(reason != CREATE)
        return;
    if(my_text_file_p == nullptr)
        return;
    FPRINTF(my_text_file_p, "scv_tr_generator (ID {}, name \"{}\", scv_tr_stream {},\n)\n", g.get_id(), g.get_name(), g.get_tx_fiber().get_id());
}
// ----------------------------------------------------------------------------
static void tx_handle_cbf(const tx_handle& t, callback_reason reason, value const& v) {
    if(my_text_file_p == nullptr)
        return;
    if(t.get_tx_fiber().get_tx_db() == nullptr)
        return;
    if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
        return;
    switch(reason) {
    case BEGIN: {
        FPRINTF(my_text_file_p, "tx_begin {} {} {}\n", t.get_id(), t.get_tx_generator_base().get_id(),
                t.get_begin_sc_time().to_string());
        boost::apply_visitor( value_visitor(t.get_id(),  t.get_tx_generator_base().get_begin_attribute_name()), v);
    } break;
    case END: {
        boost::apply_visitor( value_visitor(t.get_id(),  t.get_tx_generator_base().get_end_attribute_name()), v);
        FPRINTF(my_text_file_p, "tx_end {} {} {}\n", t.get_id(), t.get_tx_generator_base().get_id(),
                t.get_end_sc_time().to_string());
    } break;
    default:;
    }
}
// ----------------------------------------------------------------------------
static void tx_handle_record_attribute_cbf(tx_handle const& t, const char* attribute_name, value const& v) {
    if(t.get_tx_fiber().get_tx_db() == nullptr)
        return;
    if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
        return;
    if(my_text_file_p == nullptr)
        return;
    std::string tmp_str = attribute_name == nullptr ? "" : attribute_name;
    boost::apply_visitor( value_visitor(t.get_id(),  tmp_str), v);
}
// ----------------------------------------------------------------------------
static void tx_handle_relation_cbf(const tx_handle& tr_1, const tx_handle& tr_2, tx_relation_handle relation_handle) {
    if(tr_1.get_tx_fiber().get_tx_db() == nullptr)
        return;
    if(tr_1.get_tx_fiber().get_tx_db()->get_recording() == false)
        return;
    if(my_text_file_p == nullptr)
        return;
    if(my_text_file_p) {
        FPRINTF(my_text_file_p, "tx_relation \"{}\" {} {}\n",
                tr_1.get_tx_fiber().get_tx_db()->get_relation_name(relation_handle), tr_1.get_id(),
                tr_2.get_id());
    }
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void tx_text_init() {
    tx_db::register_class_cb(tx_db_cbf);
    tx_fiber::register_class_cb(tx_fiber_cbf);
    tx_generator_base::register_class_cb(tx_generator_cbf);
    tx_handle::register_class_cb(tx_handle_cbf);
    tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf);
    tx_handle::register_relation_cb(tx_handle_relation_cbf);
}
} // namespace txrec
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
