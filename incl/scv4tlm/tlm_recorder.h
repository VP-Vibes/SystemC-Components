/*******************************************************************************
 * Copyright 2016, 2018, 2021 MINRES Technologies GmbH
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

#ifndef TLM2_RECORDER_H_
#define TLM2_RECORDER_H_

#include "tlm_gp_data_ext.h"
#include "tlm_recording_extension.h"
#include "tlm_extension_recording_registry.h"
#include <array>
#include <unordered_map>
#include <regex>
#include <scv.h>
#include <string>
#include <tlm/tlm_mm.h>
#include <tlm>
#include <tlm_utils/peq_with_cb_and_phase.h>

//! SCV components for TLM2.0
namespace scv4tlm {
namespace impl {
//! \brief the class to hold the information to be recorded on the timed
//! streams
template <typename TYPES = tlm::tlm_base_protocol_types> class tlm_recording_payload : public TYPES::tlm_payload_type {
public:
    scv_tr_handle parent;
    uint64 id;
    tlm_recording_payload& operator=(const typename TYPES::tlm_payload_type& x) {
        id = reinterpret_cast<uintptr_t>(&x);
        this->set_command(x.get_command());
        this->set_address(x.get_address());
        this->set_data_ptr(nullptr);
        this->set_data_length(x.get_data_length());
        this->set_response_status(x.get_response_status());
        this->set_byte_enable_ptr(nullptr);
        this->set_byte_enable_length(x.get_byte_enable_length());
        this->set_streaming_width(x.get_streaming_width());
        return (*this);
    }
    explicit tlm_recording_payload(tlm::tlm_mm_interface* mm)
    : TYPES::tlm_payload_type(mm)
    , parent()
    , id(0) {}
};
template <typename TYPES = tlm::tlm_base_protocol_types> struct tlm_recording_types {
    using tlm_payload_type = tlm_recording_payload<TYPES>;
    using tlm_phase_type = typename TYPES::tlm_phase_type;
};

} // namespace impl
/*! \brief The TLM2 transaction recorder
 *
 * This module records all TLM transaction to a SCV transaction stream for
 * further viewing and analysis.
 * The handle of the created transaction is storee in an tlm_extension so that
 * another instance of the scv_tlm_recorder
 * e.g. further down the opath can link to it.
 */
template <typename TYPES = tlm::tlm_base_protocol_types>
class tlm_recorder : public virtual tlm::tlm_fw_transport_if<TYPES>,
public virtual tlm::tlm_bw_transport_if<TYPES> {
    std::string get_parent(char const* hier_name) {
        std::string ret(hier_name);
        auto pos = ret.rfind('.');
        return pos == std::string::npos? ret : ret.substr(0, pos);
    }

public:
    using recording_types = impl::tlm_recording_types<TYPES>;
    using mm = tlm::tlm_mm<recording_types>;
    using tlm_recording_payload = impl::tlm_recording_payload<TYPES>;

    //! \brief the attribute to selectively enable/disable recording
    sc_core::sc_attribute<bool> enableTracing;

    //! \brief the attribute to selectively enable/disable timed recording
    sc_core::sc_attribute<bool> enableTimedTracing{"enableTimedTracing", true};

    //! \brief the attribute to selectively enable/disable DMI recording
    sc_core::sc_attribute<bool> enableDmiTracing{"enableDmiTracing", false};

    //! \brief the attribute to selectively enable/disable transport dbg recording
    sc_core::sc_attribute<bool> enableTrDbgTracing{"enableTrDbgTracing", false};

    //! \brief the port where fw accesses are forwarded to
    sc_core::sc_port_b<tlm::tlm_fw_transport_if<TYPES>>& fw_port;

    //! \brief the port where bw accesses are forwarded to
    sc_core::sc_port_b<tlm::tlm_bw_transport_if<TYPES>>& bw_port;

    /**
     * @fn  tlm_recorder(sc_core::sc_port_b<tlm::tlm_fw_transport_if<TYPES>>&, sc_core::sc_port_b<tlm::tlm_bw_transport_if<TYPES>>&, bool=true, scv_tr_db*=scv_tr_db::get_default_db())
     * @brief The constructor of the component
     *
     * @param fw_port the forward port to use in the forward path
     * @param bw_port the backward port to use in the backward path
     * @param recording_enabled if true recording is enabled by default
     * @param tr_db is a pointer to a transaction recording database. If none is
     * provided the default one is retrieved.
     *        If this database is not initialized (e.g. by not calling
     * scv_tr_db::set_default_db() ) recording is disabled.
     */
    tlm_recorder(sc_core::sc_port_b<tlm::tlm_fw_transport_if<TYPES>>& fw_port,
            sc_core::sc_port_b<tlm::tlm_bw_transport_if<TYPES>>& bw_port, bool recording_enabled = true,
            scv_tr_db* tr_db = scv_tr_db::get_default_db())
    : tlm_recorder(sc_core::sc_gen_unique_name("tlm_recorder"), fw_port, bw_port, recording_enabled, tr_db) { }
    /**
     * @fn  tlm_recorder(const char*, sc_core::sc_port_b<tlm::tlm_fw_transport_if<TYPES>>&, sc_core::sc_port_b<tlm::tlm_bw_transport_if<TYPES>>&, bool=true, scv_tr_db*=scv_tr_db::get_default_db())
     * @brief
     *
     * @param name is the SystemC module name of the recorder
     * @param fw_port the forward port to use in the forward path
     * @param bw_port the backward port to use in the backward path
     * @param recording_enabled if true recording is enabled by default
     * @param tr_db is a pointer to a transaction recording database. If none is
     * provided the default one is retrieved.
     *        If this database is not initialized (e.g. by not calling
     * scv_tr_db::set_default_db() ) recording is disabled.
     */
    tlm_recorder(const char* name, sc_core::sc_port_b<tlm::tlm_fw_transport_if<TYPES>>& fw_port,
            sc_core::sc_port_b<tlm::tlm_bw_transport_if<TYPES>>& bw_port, bool recording_enabled = true,
            scv_tr_db* tr_db = scv_tr_db::get_default_db())
    : enableTracing("enableTracing", recording_enabled)
    , fw_port(fw_port)
    , bw_port(bw_port)
    , b_timed_peq(this, &tlm_recorder::btx_cb)
    , nb_timed_peq(this, &tlm_recorder::nbtx_cb)
    , m_db(tr_db)
    , fixed_basename(name) {
    }

    virtual ~tlm_recorder() override {
        delete b_streamHandle;
        for(auto* p : b_trHandle)
            delete p; // NOLINT
        delete b_streamHandleTimed;
        for(auto* p : b_trTimedHandle)
            delete p; // NOLINT
        delete nb_streamHandle;
        for(auto* p : nb_trHandle)
            delete p; // NOLINT
        delete nb_streamHandleTimed;
        for(auto* p : nb_trTimedHandle)
            delete p; // NOLINT
        delete dmi_streamHandle;
        delete dmi_trGetHandle;
        delete dmi_trInvalidateHandle;
    }

    // TLM-2.0 interface methods for initiator and target sockets, surrounded with
    // Tx Recording
    /*! \brief The non-blocking forward transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "nb_fw" with current timestamps.
     * \param trans is the generic payload of the transaction
     * \param phase is the current phase of the transaction
     * \param delay is the annotated delay
     * \return the sync state of the transaction
     */
    tlm::tlm_sync_enum nb_transport_fw(typename TYPES::tlm_payload_type& trans,
            typename TYPES::tlm_phase_type& phase, sc_core::sc_time& delay) override;

    /*! \brief The non-blocking backward transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "nb_bw" with current timestamps.
     * \param trans is the generic payload of the transaction
     * \param phase is the current phase of the transaction
     * \param delay is the annotated delay
     * \return the sync state of the transaction
     */
    tlm::tlm_sync_enum nb_transport_bw(typename TYPES::tlm_payload_type& trans,
            typename TYPES::tlm_phase_type& phase, sc_core::sc_time& delay) override;

    /*! \brief The blocking transport function
     *
     * This type of transaction is forwarded and recorded to a transaction stream
     * named "b_tx" with current timestamps. Additionally a "b_tx_timed"
     * is been created recording the transactions at their annotated delay
     * \param trans is the generic payload of the transaction
     * \param delay is the annotated delay
     */
    void b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) override;
    /*! \brief The direct memory interface forward function
     *
     * This type of transaction is just forwarded and not recorded.
     * \param trans is the generic payload of the transaction
     * \param dmi_data is the structure holding the dmi information
     * \return if the dmi structure is valid
     */
    bool get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) override;
    /*! \brief The direct memory interface backward function
     *
     * This type of transaction is just forwarded and not recorded.
     * \param start_addr is the start address of the memory area being invalid
     * \param end_addr is the end address of the memory area being invalid
     */
    void invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) override;
    /*! \brief The debug transportfunction
     *
     * This type of transaction is just forwarded and not recorded.
     * \param trans is the generic payload of the transaction
     * \return the sync state of the transaction
     */
    unsigned int transport_dbg(typename TYPES::tlm_payload_type& trans) override;
    /*! \brief get the current state of transaction recording
     *
     * \return if true transaction recording is enabled otherwise transaction
     * recording is bypassed
     */
    bool isRecordingEnabled() const { return m_db && enableTracing.value; }

private:
    //! event queue to hold time points of blocking transactions
    tlm_utils::peq_with_cb_and_phase<tlm_recorder, recording_types> b_timed_peq;
    //! event queue to hold time points of non-blocking transactions
    tlm_utils::peq_with_cb_and_phase<tlm_recorder, recording_types> nb_timed_peq;
    /*! \brief The thread processing the blocking accesses with their annotated
     * times
     *  to generate the timed view of blocking tx
     */
    void btx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase);
    /*! \brief The thread processing the non-blocking requests with their
     * annotated times
     * to generate the timed view of non-blocking tx
     */
    void nbtx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase);
    //! transaction recording database
    scv_tr_db* m_db{nullptr};
    //! blocking transaction recording stream handle
    scv_tr_stream* b_streamHandle{nullptr};
    //! transaction generator handle for blocking transactions
    std::array<scv_tr_generator<sc_dt::uint64, sc_dt::uint64>*, 3> b_trHandle{{nullptr, nullptr, nullptr}};
    //! timed blocking transaction recording stream handle
    scv_tr_stream* b_streamHandleTimed{nullptr};
    //! transaction generator handle for blocking transactions with annotated
    //! delays
    std::array<scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>*, 3> b_trTimedHandle{{nullptr, nullptr, nullptr}};
    std::unordered_map<uint64, scv_tr_handle> btx_handle_map;

    enum DIR { FW, BW, REQ=FW, RESP=BW};
    //! non-blocking transaction recording stream handle
    scv_tr_stream* nb_streamHandle{nullptr};
    //! non-blocking transaction recording stream handle with timing
    scv_tr_stream* nb_streamHandleTimed{nullptr};
    //! transaction generator handle for non-blocking transactions
    std::array<scv_tr_generator<std::string, std::string>*, 2> nb_trHandle{{nullptr, nullptr}};
    //! transaction generator handle for non-blocking transactions with annotated delays
    std::array<scv_tr_generator<>*, 2> nb_trTimedHandle{{nullptr, nullptr}};
    map<uint64, scv_tr_handle> nbtx_req_handle_map;
    map<uint64, scv_tr_handle> nbtx_last_req_handle_map;

    //! dmi transaction recording stream handle
    scv_tr_stream* dmi_streamHandle{nullptr};
    //! transaction generator handle for DMI transactions
    scv_tr_generator<tlm_gp_data, tlm_dmi_data>* dmi_trGetHandle{nullptr};
    scv_tr_generator<sc_dt::uint64, sc_dt::uint64>* dmi_trInvalidateHandle{nullptr};
public:
    void initialize_streams() {
        if(isRecordingEnabled() && !b_streamHandle){
            b_streamHandle = new scv_tr_stream((fixed_basename + "_bl").c_str(), "[TLM][base-protocol][b]", m_db);
            b_trHandle[tlm::TLM_READ_COMMAND] =
                    new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("read", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_WRITE_COMMAND] =
                    new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("write", *b_streamHandle, "start_delay", "end_delay");
            b_trHandle[tlm::TLM_IGNORE_COMMAND] =
                    new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("ignore", *b_streamHandle, "start_delay", "end_delay");
            if(enableTimedTracing.value) {
                b_streamHandleTimed = new scv_tr_stream((fixed_basename + "_bl_timed").c_str(), "[TLM][base-protocol][b][timed]", m_db);
                b_trTimedHandle[tlm::TLM_READ_COMMAND] =
                        new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("read", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_WRITE_COMMAND] =
                        new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("write", *b_streamHandleTimed);
                b_trTimedHandle[tlm::TLM_IGNORE_COMMAND] =
                        new scv_tr_generator<tlm::tlm_command, tlm::tlm_response_status>("ignore", *b_streamHandleTimed);
            }
            nb_streamHandle = new scv_tr_stream((fixed_basename + "_nb").c_str(), "[TLM][base-protocol][nb]", m_db);
            nb_trHandle[FW] = new scv_tr_generator<std::string, std::string>("fw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            nb_trHandle[BW] = new scv_tr_generator<std::string, std::string>("bw", *nb_streamHandle, "tlm_phase", "tlm_phase[return_path]");
            if(enableTimedTracing.value) {
                nb_streamHandleTimed = new scv_tr_stream((fixed_basename + "_nb_timed").c_str(), "[TLM][base-protocol][nb][timed]", m_db);
                nb_trTimedHandle[FW] = new scv_tr_generator<>("request", *nb_streamHandleTimed);
                nb_trTimedHandle[BW] = new scv_tr_generator<>("response", *nb_streamHandleTimed);
            }
            if(enableDmiTracing.value) {
                dmi_streamHandle = new scv_tr_stream((fixed_basename + "_dmi").c_str(), "[TLM][base-protocol][dmi]", m_db);
                dmi_trGetHandle = new scv_tr_generator<tlm_gp_data, tlm_dmi_data>("get", *dmi_streamHandle, "trans", "dmi_data");
                dmi_trInvalidateHandle = new scv_tr_generator<sc_dt::uint64, sc_dt::uint64>("invalidate", *dmi_streamHandle, "start_addr", "end_addr");
            }
        }
    }
private:
    const std::string fixed_basename;
    inline std::string phase2string(const tlm::tlm_phase& p) {
        std::stringstream ss;
        ss << p;
        return ss.str();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementations of functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TYPES>
void tlm_recorder<TYPES>::b_transport(typename TYPES::tlm_payload_type& trans, sc_core::sc_time& delay) {
    tlm_recording_payload* req{nullptr};
    if(!isRecordingEnabled()) {
        fw_port->b_transport(trans, delay);
        return;
    } else if(!b_streamHandle) initialize_streams();
    // Get a handle for the new transaction
    scv_tr_handle h = b_trHandle[trans.get_command()]->begin_transaction(delay.value(), sc_time_stamp());
    tlm_gp_data tgd(trans);

    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(b_streamHandleTimed) {
        req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        req->id = h.get_id();
        b_timed_peq.notify(*req, tlm::BEGIN_REQ, delay);
    }

    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordBeginTx(h, trans);
    tlm_recording_extension* preExt = nullptr;

    trans.get_extension(preExt);
    if(preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
            trans.set_extension(preExt);
    } else {
        h.add_relation(rel_str(PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    scv_tr_handle preTx{preExt->txHandle};
    preExt->txHandle = h;
    if(trans.get_command() == tlm::TLM_WRITE_COMMAND && tgd.data_length < 8)
        h.record_attribute("trans.data_value", tgd.get_data_value());
    fw_port->b_transport(trans, delay);
    if(preExt && preExt->get_creator() == this) {
        // clean-up the extension if this is the original creator
        trans.set_extension(static_cast<tlm_recording_extension*>(nullptr));
        if(!trans.has_mm()) {
            delete preExt;
        }
    } else {
        preExt->txHandle = preTx;
    }

    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans", tgd);
    if(trans.get_command() == tlm::TLM_READ_COMMAND && tgd.data_length < 8)
        h.record_attribute("trans.data_value", tgd.get_data_value());
    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordEndTx(h, trans);
    // End the transaction
    b_trHandle[trans.get_command()]->end_transaction(h, delay.value(), sc_time_stamp());
    // and now the stuff for the timed tx
    if(b_streamHandleTimed) {
        b_timed_peq.notify(*req, tlm::END_RESP, delay);
    }
}

template <typename TYPES>
void tlm_recorder<TYPES>::btx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase) {
    scv_tr_handle h;
    // Now process outstanding recordings
    switch(phase) {
    case tlm::BEGIN_REQ: {
        tlm_gp_data tgd(rec_parts);
        h = b_trTimedHandle[rec_parts.get_command()]->begin_transaction(rec_parts.get_command());
        h.record_attribute("trans", tgd);
        h.add_relation(rel_str(PARENT_CHILD), rec_parts.parent);
        btx_handle_map[rec_parts.id] = h;
    } break;
    case tlm::END_RESP: {
        auto it = btx_handle_map.find(rec_parts.id);
        sc_assert(it != btx_handle_map.end());
        h = it->second;
        btx_handle_map.erase(it);
        h.end_transaction(h, rec_parts.get_response_status());
        rec_parts.release();
    } break;
    default:
        sc_assert(!"phase not supported!");
    }
    return;
}

template <typename TYPES>
tlm::tlm_sync_enum tlm_recorder<TYPES>::nb_transport_fw(typename TYPES::tlm_payload_type& trans,
        typename TYPES::tlm_phase_type& phase,
        sc_core::sc_time& delay) {
    if(!isRecordingEnabled())
        return fw_port->nb_transport_fw(trans, phase, delay);
    else  if(!nb_streamHandle) initialize_streams();
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    // Get a handle for the new transaction
    scv_tr_handle h = nb_trHandle[FW]->begin_transaction(phase2string(phase));
    tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    if(preExt == nullptr) { // we are the first recording this transaction
        preExt = new tlm_recording_extension(h, this);
        if(trans.has_mm())
            trans.set_auto_extension(preExt);
        else
            trans.set_extension(preExt);
    } else {
        // link handle if we have a predecessor
        h.add_relation(rel_str(PREDECESSOR_SUCCESSOR), preExt->txHandle);
    }
    // update the extension
    if(preExt)
        preExt->txHandle = h;
    h.record_attribute("delay", delay.to_string());
    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordBeginTx(h, trans);
    tlm_gp_data tgd(trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(nb_streamHandleTimed) {
        auto* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = fw_port->nb_transport_fw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    h.record_attribute("trans.uid", reinterpret_cast<uintptr_t>(&trans));
    h.record_attribute("tlm_sync", status);
    h.record_attribute("delay[return_path]", delay.to_string());
    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans", tgd);
    if(tgd.data_length < 8) {
        uint64_t buf = 0;
        // FIXME: this is endianess dependent
        for(size_t i = 0; i < tgd.data_length; i++)
            buf += (*tgd.data) << i * 8;
        h.record_attribute("trans.data_value", buf);
    }
    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordEndTx(h, trans);
    // get the extension and free the memory if it was mine
    if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_ACCEPTED && phase == tlm::END_RESP)) {
        trans.get_extension(preExt);
        if(preExt && preExt->get_creator() == this) {
            trans.set_extension(static_cast<tlm_recording_extension*>(nullptr));
            if(!trans.has_mm()) {
                delete preExt;
            }
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
            tlm_recording_payload* req = mm::get().allocate();
            req->acquire();
            (*req) = trans;
            req->parent = h;
            nb_timed_peq.notify(*req, (status == tlm::TLM_COMPLETED && phase == tlm::BEGIN_REQ) ? tlm::END_RESP : phase,
                    delay);
        }
    } else if(nb_streamHandleTimed && status == tlm::TLM_UPDATED) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    // End the transaction
    nb_trHandle[FW]->end_transaction(h, phase2string(phase));
    return status;
}

template <typename TYPES>
tlm::tlm_sync_enum tlm_recorder<TYPES>::nb_transport_bw(typename TYPES::tlm_payload_type& trans,
        typename TYPES::tlm_phase_type& phase,
        sc_core::sc_time& delay) {
    if(!isRecordingEnabled())
        return bw_port->nb_transport_bw(trans, phase, delay);
    else  if(!nb_streamHandle) initialize_streams();
    /*************************************************************************
     * prepare recording
     *************************************************************************/
    tlm_recording_extension* preExt = nullptr;
    trans.get_extension(preExt);
    // sc_assert(preExt != nullptr && "ERROR on backward path");
    // Get a handle for the new transaction
    scv_tr_handle h = nb_trHandle[BW]->begin_transaction(phase2string(phase));
    // link handle if we have a predecessor and that's not ourself
    if(preExt) {
        h.add_relation(rel_str(PREDECESSOR_SUCCESSOR), preExt->txHandle);
        // and set the extension handle to this transaction
        preExt->txHandle = h;
    }
    h.record_attribute("delay", delay.to_string());
    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordBeginTx(h, trans);
    tlm_gp_data tgd(trans);
    /*************************************************************************
     * do the timed notification
     *************************************************************************/
    if(nb_streamHandleTimed) {
        tlm_recording_payload* req = mm::get().allocate();
        req->acquire();
        (*req) = trans;
        req->parent = h;
        nb_timed_peq.notify(*req, phase, delay);
    }
    /*************************************************************************
     * do the access
     *************************************************************************/
    tlm::tlm_sync_enum status = bw_port->nb_transport_bw(trans, phase, delay);
    /*************************************************************************
     * handle recording
     *************************************************************************/
    h.record_attribute("trans.uid", reinterpret_cast<uintptr_t>(&trans));
    h.record_attribute("tlm_sync", status);
    h.record_attribute("delay[return_path]", delay.to_string());
    tgd.response_status = trans.get_response_status();
    h.record_attribute("trans", tgd);
    if(tgd.data_length < 8) {
        uint64_t buf = 0;
        // FIXME: this is endianess dependent
        for(size_t i = 0; i < tgd.data_length; i++)
            buf += (*tgd.data) << i * 8;
        h.record_attribute("trans.data_value", buf);
    }
    for(auto& extensionRecording : scv4tlm::tlm_extension_recording_registry<TYPES>::inst().get())
        if(extensionRecording)
            extensionRecording->recordEndTx(h, trans);
    // End the transaction
    nb_trHandle[BW]->end_transaction(h, phase2string(phase));
    if(status == tlm::TLM_COMPLETED || (status == tlm::TLM_UPDATED && phase == tlm::END_RESP)) {
        // the transaction is finished
        if(preExt && preExt->get_creator() == this) {
            // clean-up the extension if this is the original creator
            trans.set_extension(static_cast<tlm_recording_extension*>(nullptr));
            if(!trans.has_mm()) {
                delete preExt;
            }
        }
        /*************************************************************************
         * do the timed notification if req. finished here
         *************************************************************************/
        if(nb_streamHandleTimed) {
            tlm_recording_payload* req = mm::get().allocate();
            req->acquire();
            (*req) = trans;
            req->parent = h;
            nb_timed_peq.notify(*req, phase, delay);
        }
    }
    return status;
}

template <typename TYPES>
void tlm_recorder<TYPES>::nbtx_cb(tlm_recording_payload& rec_parts, const typename TYPES::tlm_phase_type& phase) {
    scv_tr_handle h;
    std::map<uint64, scv_tr_handle>::iterator it;
    tlm_gp_data tgd(rec_parts);
    switch(phase) { // Now process outstanding recordings
    case tlm::BEGIN_REQ:
        h = nb_trTimedHandle[REQ]->begin_transaction(rel_str(PARENT_CHILD), rec_parts.parent);
        h.record_attribute("trans", tgd);
        nbtx_req_handle_map[rec_parts.id] = h;
        break;
    case tlm::END_REQ:
        it = nbtx_req_handle_map.find(rec_parts.id);
        sc_assert(it != nbtx_req_handle_map.end());
        h = it->second;
        nbtx_req_handle_map.erase(it);
        h.end_transaction();
        nbtx_last_req_handle_map[rec_parts.id] = h;
        break;
    case tlm::BEGIN_RESP:
        it = nbtx_req_handle_map.find(rec_parts.id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            nbtx_req_handle_map.erase(it);
            h.end_transaction();
            nbtx_last_req_handle_map[rec_parts.id] = h;
        }
        h = nb_trTimedHandle[RESP]->begin_transaction(rel_str(PARENT_CHILD), rec_parts.parent);
        h.record_attribute("trans", tgd);
        nbtx_req_handle_map[rec_parts.id] = h;
        it = nbtx_last_req_handle_map.find(rec_parts.id);
        if(it != nbtx_last_req_handle_map.end()) {
            scv_tr_handle pred = it->second;
            nbtx_last_req_handle_map.erase(it);
            h.add_relation(rel_str(PREDECESSOR_SUCCESSOR), pred);
        }
        break;
    case tlm::END_RESP:
        it = nbtx_req_handle_map.find(rec_parts.id);
        if(it != nbtx_req_handle_map.end()) {
            h = it->second;
            nbtx_req_handle_map.erase(it);
            h.end_transaction();
        }
        break;
    default:
        // sc_assert(!"phase not supported!");
        break;
    }
    rec_parts.release();
    return;
}

template <typename TYPES>
bool tlm_recorder<TYPES>::get_direct_mem_ptr(typename TYPES::tlm_payload_type& trans, tlm::tlm_dmi& dmi_data) {
    if(!(m_db && enableDmiTracing.value))
        return fw_port->get_direct_mem_ptr(trans, dmi_data);
    else  if(!b_streamHandle) initialize_streams();
    scv_tr_handle h = dmi_trGetHandle->begin_transaction(tlm_gp_data(trans));
    bool status = fw_port->get_direct_mem_ptr(trans, dmi_data);
    dmi_trGetHandle->end_transaction(h, tlm_dmi_data(dmi_data));
    return status;
}
/*! \brief The direct memory interface backward function
 *
 * This type of transaction is just forwarded and not recorded.
 * \param start_addr is the start address of the memory area being invalid
 * \param end_addr is the end address of the memory area being invalid
 */
template <typename TYPES>
void tlm_recorder<TYPES>::invalidate_direct_mem_ptr(sc_dt::uint64 start_addr, sc_dt::uint64 end_addr) {
    if(!(m_db && enableDmiTracing.value)) {
        bw_port->invalidate_direct_mem_ptr(start_addr, end_addr);
        return;
    }
    scv_tr_handle h = dmi_trInvalidateHandle->begin_transaction(start_addr);
    bw_port->invalidate_direct_mem_ptr(start_addr, end_addr);
    dmi_trInvalidateHandle->end_transaction(h, end_addr);
    return;
}
/*! \brief The debug transportfunction
 *
 * This type of transaction is just forwarded and not recorded.
 * \param trans is the generic payload of the transaction
 * \return the sync state of the transaction
 */
template <typename TYPES> unsigned int tlm_recorder<TYPES>::transport_dbg(typename TYPES::tlm_payload_type& trans) {
    unsigned int count = fw_port->transport_dbg(trans);
    return count;
}

} // namespace scv4tlm

#endif /* TLM2_RECORDER_H_ */
