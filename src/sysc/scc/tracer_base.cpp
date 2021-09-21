/*******************************************************************************
 * Copyright 2017, 2018 MINRES Technologies GmbH
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
/*
 * tracer.cpp
 *
 *  Created on: Nov 9, 2016
 *      Author: developer
 */

#include "tracer_base.h"
#include "sc_variable.h"
#include "traceable.h"
#include <cstring>
#include <systemc>

using namespace sc_core;
using namespace sc_dt;
using namespace scc;

template <typename T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
inline auto trace_helper(sc_trace_file* trace_file, const sc_object* object) -> bool {
    if(auto* ptr = dynamic_cast<const T*>(object)) {
        sc_core::sc_trace(trace_file, *ptr, object->name());
        return true;
    }
    return false;
}

template <typename T, typename std::enable_if<!std::is_class<T>::value, unsigned>::type = 0>
inline auto trace_helper(sc_trace_file*, const sc_object*) -> bool {
    return false;
}

template <typename T> inline auto variable_trace_helper(sc_trace_file* trace_file, const sc_object* object) -> bool {
    if(auto* ptr = dynamic_cast<const sc_variable*>(object)) {
        ptr->trace(trace_file);
        return true;
    }
    return false;
}

template <typename T>
inline auto try_trace_obj(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) -> bool {
    if((types_to_trace & trace_types::PORTS) == trace_types::PORTS) {
        if(trace_helper<sc_core::sc_in<T>>(trace_file, object))
            return true;
        if(trace_helper<sc_core::sc_out<T>>(trace_file, object))
            return true;
    }
    if((types_to_trace & trace_types::SIGNALS) == trace_types::SIGNALS) {
        if(trace_helper<sc_core::sc_signal<T, SC_ONE_WRITER>>(trace_file, object))
            return true;
        if(trace_helper<sc_core::sc_signal<T, SC_MANY_WRITERS>>(trace_file, object))
            return true;
        if(trace_helper<sc_core::sc_signal<T, SC_UNCHECKED_WRITERS>>(trace_file, object))
            return true;
    }
    if((types_to_trace & trace_types::VARIABLES) == trace_types::VARIABLES) {
        if(variable_trace_helper<T>(trace_file, object))
            return true;
    }
    if((types_to_trace & trace_types::OBJECTS) == trace_types::OBJECTS)
        if(trace_helper<T>(trace_file, object))
            return true;
    return false;
}

template<size_t SIZE>
struct ForLoop {
    template<template <size_t> class Func>
    static bool iterate(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        if(ForLoop<SIZE-(SIZE>128?8:1)>::template iterate<Func>(trace_file, object, types_to_trace))
            return true;
        else
            return Func<SIZE>()(trace_file, object, types_to_trace);
    }
};

template<>
struct ForLoop<1> {
    template<template <size_t> class Func>
    static bool iterate(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return Func<1>()(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_uint_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_uint<size>>(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_int_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_int<size>>(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_biguint_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_biguint<size>>(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_bigint_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_bigint<size>>(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_bv_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_bv<size>>(trace_file, object, types_to_trace);
    }
};

template <size_t size>
struct sc_lv_tester {
    bool operator()(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
        return try_trace_obj<sc_lv<size>>(trace_file, object, types_to_trace);
    }
};

void tracer_base::try_trace(sc_trace_file* trace_file, const sc_object* object, trace_types types_to_trace) {
    if(try_trace_obj<bool>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<char>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<unsigned char>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<short>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<unsigned short>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<int>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<unsigned int>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<long>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<unsigned long>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<long long>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<unsigned long long>(trace_file, object, types_to_trace))
        return;

    if(try_trace_obj<float>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<double>(trace_file, object, types_to_trace))
        return;
#if(SYSTEMC_VERSION >= 20171012)
    if(try_trace_obj<sc_time>(trace_file, object, types_to_trace))
        return;
#endif
    if(try_trace_obj<sc_bit>(trace_file, object, types_to_trace))
        return;
    if(try_trace_obj<sc_logic>(trace_file, object, types_to_trace))
        return;

    if(ForLoop<64>::iterate<sc_uint_tester>(trace_file, object, types_to_trace))
        return;
    if(ForLoop<64>::iterate<sc_int_tester>(trace_file, object, types_to_trace))
        return;
    if(ForLoop<1024>::iterate<sc_biguint_tester>(trace_file, object, types_to_trace))
        return;
    if(ForLoop<1024>::iterate<sc_bigint_tester>(trace_file, object, types_to_trace))
        return;
    if(ForLoop<1024>::iterate<sc_bv_tester>(trace_file, object, types_to_trace))
        return;
    if(ForLoop<1024>::iterate<sc_lv_tester>(trace_file, object, types_to_trace))
        return;
}

void tracer_base::descend(const sc_object* obj, bool trace_all) {
    if(obj == this)
        return;
    const char* kind = obj->kind();
    if(strcmp(kind, "tlm_signal") == 0) {
        obj->trace(trf);
        return;
    } else if(strcmp(kind, "sc_vector") == 0) {
        for(auto o : obj->get_child_objects())
            descend(o, trace_all);
        return;
    } else if((strcmp(kind, "sc_module") == 0 && trace_all) || dynamic_cast<const traceable*>(obj)) {
        obj->trace(trf);
        for(auto o : obj->get_child_objects())
            descend(o, trace_all);
    } else
        try_trace(trf, obj, types_to_trace);
}