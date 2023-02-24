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

#include "cbor/chunked_writer.h"
#include "lwtr.h"
#include <sysc/utils/sc_report.h>
#include <numeric>
#include <cstring>
#include <sstream>

namespace lwtr {
namespace {
// ----------------------------------------------------------------------------
template<typename WRITER>
struct Writer {
	std::unique_ptr<WRITER> output_writer;
	Writer(const std::string& name): output_writer(new WRITER(name))
	{}

	Writer()
	{}

	inline bool open(const std::string& name) {
		output_writer.reset(new WRITER(name));
		return output_writer->cw.enc.ofs.is_open();
	}

	inline void close() { output_writer.reset(nullptr); }

	inline bool is_open() { return output_writer->cw.enc.ofs.is_open();}

	inline static WRITER& writer() { return *get().output_writer; }

	inline static Writer &get() {
		static Writer db;
		return db;
	}
};
// ----------------------------------------------------------------------------
template<typename DB>
void tx_db_cbf(tx_db const& _tx_db, callback_reason reason) {
	static std::string file_name("tx_default");
	switch(reason) {
	case CREATE: {
		if(_tx_db.get_name().length() != 0) {
			file_name = _tx_db.get_name();
		}
		file_name+=".ftr";
		if(Writer<DB>::get().open(file_name)) {
		    Writer<DB>::writer().writeInfo(sc_core::sc_time(1, sc_core::SC_SEC)/sc_core::sc_time(1, sc_core::SC_PS));
			std::stringstream ss;
			ss << "opening file " << file_name;
			SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
		} else {
			std::stringstream ss;
			ss << "Can't open text recording file. " << strerror(errno);
			SC_REPORT_ERROR(__FUNCTION__, ss.str().c_str());
		}
	}
	break;
	case DELETE: {
		std::stringstream ss;
		ss << "closing file " << file_name;
		SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
		Writer<DB>::get().close();
	}
	break;
	default:
		SC_REPORT_ERROR(__FUNCTION__, "Unknown reason in tx_db callback");
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_fiber_cbf(const tx_fiber& s, callback_reason reason) {
	if(Writer<DB>::get().is_open() && reason == CREATE) {
		Writer<DB>::writer().writeStream(s.get_id(), s.get_name(), s.get_fiber_kind());
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
struct value_visitor {
	uint64_t const tx_id;
	cbor::event_type const pos;

	mutable std::vector<std::string> hier_name;

	value_visitor(uint64_t tx_id, cbor::event_type pos, std::string const& name):tx_id(tx_id), pos(pos) {
		if(name.length()) hier_name.push_back(name);
	}

	std::string get_full_name() const {
		if(hier_name.empty()) return "unnamed";
		auto buf_size = std::accumulate(hier_name.begin(), hier_name.end(), 0U,
				[](size_t a, std::string const& b){return a+b.size();});
		std::string res;
		res.reserve(buf_size+hier_name.size());
		for(auto&e: hier_name) {
			if(res.size()) res.append(".");
			res.append(e);
		}
		return res;
	}

	void operator()(const lwtr::no_data& v) const {	}
	void operator()(std::string const& v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::STRING, v);
	}
	void operator()(double v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::FLOATING_POINT_NUMBER, v);
	}
	void operator()(char const* v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::STRING, v);
	}
	void operator()(bool v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::BOOLEAN, v);
	}
	void operator()(uint8_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::UNSIGNED, v);
	}
	void operator()(uint16_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::UNSIGNED, v);
	}
	void operator()(uint32_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::UNSIGNED, v);
	}
	void operator()(uint64_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::UNSIGNED, v);
	}
	void operator()(int8_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::INTEGER, v);
	}
	void operator()(int16_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::INTEGER, v);
	}
	void operator()(int32_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::INTEGER, v);
	}
	void operator()(int64_t v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::INTEGER, v);
	}
	void operator()(sc_dt::sc_bv_base const& v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::BIT_VECTOR, v.to_string());
	}
	void operator()(sc_dt::sc_lv_base const& v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::LOGIC_VECTOR, v.to_string());
	}
	void operator()(sc_core::sc_time v) const {
		Writer<DB>::writer().writeAttribute(tx_id, pos, get_full_name(), cbor::data_type::STRING, v.to_string());
	}
	void operator()(lwtr::object const& v) const {
		for(auto& e:v) {
			hier_name.push_back(std::get<0>(e));
			mpark::visit(*this, std::get<1>(e) );
			hier_name.pop_back();
		}
	}
};
// ----------------------------------------------------------------------------
template<typename DB>
void tx_generator_cbf(const tx_generator_base& g, callback_reason reason) {
	if(Writer<DB>::get().is_open() && reason == CREATE) {
		Writer<DB>::writer().writeGenerator(g.get_id(), g.get_name(), g.get_tx_fiber().get_id());
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_cbf(const tx_handle& t, callback_reason reason, value const& v) {
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
				t.get_begin_sc_time()/sc_core::sc_time(1, sc_core::SC_PS));
		mpark::visit( value_visitor<DB>(t.get_id(), cbor::event_type::BEGIN, t.get_tx_generator_base().get_begin_attribute_name()), v);
	} break;
	case END: {
		mpark::visit( value_visitor<DB>(t.get_id(), cbor::event_type::END, t.get_tx_generator_base().get_end_attribute_name()), v);
		Writer<DB>::writer().endTransaction(t.get_id(), t.get_end_sc_time()/sc_core::sc_time(1, sc_core::SC_PS));
	} break;
	default:;
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_record_attribute_cbf(tx_handle const& t, const char* attribute_name, value const& v) {
	if(t.get_tx_fiber().get_tx_db() == nullptr)
		return;
	if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
		return;
	if(!Writer<DB>::get().is_open())
		return;
	std::string tmp_str = attribute_name == nullptr ? "" : attribute_name;
	mpark::visit( value_visitor<DB>(t.get_id(), cbor::event_type::RECORD,  tmp_str), v);
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_relation_cbf(const tx_handle& tr_1, const tx_handle& tr_2, tx_relation_handle relation_handle) {
	if(tr_1.get_tx_fiber().get_tx_db() == nullptr)
		return;
	if(tr_1.get_tx_fiber().get_tx_db()->get_recording() == false)
		return;
	if(!Writer<DB>::get().is_open())
		return;
	if(Writer<DB>::get().is_open()) {
		Writer<DB>::writer().writeRelation(tr_1.get_tx_fiber().get_tx_db()->get_relation_name(relation_handle), tr_1.get_id(),
							tr_2.get_id());
	}
}
// ----------------------------------------------------------------------------
}
// ----------------------------------------------------------------------------
void tx_ftr_init(bool compressed) {
	if(compressed) {
		tx_db::register_class_cb(tx_db_cbf<cbor::chunked_writer<true>>);
		tx_fiber::register_class_cb(tx_fiber_cbf<cbor::chunked_writer<true>>);
		tx_generator_base::register_class_cb(tx_generator_cbf<cbor::chunked_writer<true>>);
		tx_handle::register_class_cb(tx_handle_cbf<cbor::chunked_writer<true>>);
		tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<cbor::chunked_writer<true>>);
		tx_handle::register_relation_cb(tx_handle_relation_cbf<cbor::chunked_writer<true>>);
	} else {
		tx_db::register_class_cb(tx_db_cbf<cbor::chunked_writer<false>>);
		tx_fiber::register_class_cb(tx_fiber_cbf<cbor::chunked_writer<false>>);
		tx_generator_base::register_class_cb(tx_generator_cbf<cbor::chunked_writer<false>>);
		tx_handle::register_class_cb(tx_handle_cbf<cbor::chunked_writer<false>>);
		tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<cbor::chunked_writer<false>>);
		tx_handle::register_relation_cb(tx_handle_relation_cbf<cbor::chunked_writer<false>>);
	}
}
} // namespace lwtr
// ----------------------------------------------------------------------------
