/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __ENTRYTRACKER_H__
#define __ENTRYTRACKER_H__

#include "config.h"

#include <stdio.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <map>

#include "globalregistry.h"
#include "kis_mutex.h"
#include "trackedelement.h"
#include "kis_net_microhttpd.h"

// Allocate and track named fields and give each one a custom int
class entry_tracker : public kis_net_httpd_cppstream_handler, public lifetime_global {
public:
    static std::string global_name() { return "ENTRYTRACKER"; }

    static std::shared_ptr<entry_tracker> create_entrytracker(global_registry *in_globalreg) {
        std::shared_ptr<entry_tracker> mon(new entry_tracker(in_globalreg));
        in_globalreg->entrytracker = mon.get();
        in_globalreg->register_lifetime_global(mon);
        in_globalreg->insert_global(global_name(), mon);
        return mon;
    }

private:
    entry_tracker(global_registry *in_globalreg);

public:
    virtual ~entry_tracker();

    // Register a field name; field names are plain strings, and must be unique for
    // each type; Using namespaces is recommended, ie "plugin.foo.some_field".
    //
    // A builder instance must be provided as a std::unique_ptr, this instance
    // will be used to construct the field based on the ID in the future.
    //
    // The description is a human-readable description which is published in the field
    // listing system and is intended to assist consumers of the API.
    //
    // Return: Registered field number, or negative on error (such as field exists with
    // conflicting type)
    int register_field(const std::string& in_name, 
            std::unique_ptr<tracker_element> in_builder,
            const std::string& in_desc);

    // Reserve a field name, and return an instance.  If the field ALREADY EXISTS, return
    // an instance.
    std::shared_ptr<tracker_element> register_and_get_field(const std::string& in_name, 
            std::unique_ptr<tracker_element> in_builder,
            const std::string& in_desc);

    template<typename TE> 
    std::shared_ptr<TE> register_and_get_field_as(const std::string& in_name,
            std::unique_ptr<tracker_element> in_builder,
            const std::string& in_desc) {
        return std::static_pointer_cast<TE>(register_and_get_field(in_name, std::move(in_builder),
                    in_desc));
    }

    int get_field_id(const std::string& in_name);
    std::string get_field_name(int in_id);
    std::string get_field_description(int in_id);

    // Generate a shared field instance, using the builder
    template<class T> std::shared_ptr<T> get_shared_instance_as(const std::string& in_name) {
        return std::static_pointer_cast<T>(get_shared_instance(in_name));
    }
    std::shared_ptr<tracker_element> get_shared_instance(const std::string& in_name);

    template<class T> std::shared_ptr<T> get_shared_instance_as(int in_id) {
        return std::static_pointer_cast<T>(get_shared_instance(in_id));
    }
    std::shared_ptr<tracker_element> get_shared_instance(int in_id);

    // Register a serializer for auto-serialization based on type
    void register_serializer(const std::string& type, std::shared_ptr<tracker_element_serializer> in_ser);
    void remove_serializer(const std::string& type);
    bool can_serialize(const std::string& type);
    bool Serialize(const std::string& type, std::ostream &stream, shared_tracker_element elem,
            std::shared_ptr<tracker_element_serializer::rename_map> name_map = nullptr);

    // HTTP api
    virtual bool httpd_verify_path(const char *path, const char *method);

    virtual void httpd_create_stream_response(kis_net_httpd *httpd,
            kis_net_httpd_connection *connection,
            const char *url, const char *method, const char *upload_data,
            size_t *upload_data_size, std::stringstream &stream);

protected:
    global_registry *globalreg;

    kis_recursive_timed_mutex entry_mutex;
    kis_recursive_timed_mutex serializer_mutex;

    int next_field_num;

    struct reserved_field {
        // ID we assigned
        int field_id;

        // Readable metadata
        std::string field_name;
        std::string field_description;

        // Builder instance
        std::unique_ptr<tracker_element> builder;
    };

    std::map<std::string, std::shared_ptr<reserved_field> > field_name_map;
    std::map<int, std::shared_ptr<reserved_field> > field_id_map;
    std::map<std::string, std::shared_ptr<tracker_element_serializer> > serializer_map;
};

class SerializerScope {
public:
    SerializerScope(shared_tracker_element e, 
            std::shared_ptr<tracker_element_serializer::rename_map> name_map) {
        elem = e;
        rnmap = name_map;

        if (rnmap != NULL) {
            auto nmi = rnmap->find(elem);
            if (nmi != rnmap->end()) {
                tracker_element_serializer::pre_serialize_path(nmi->second);
            } else {
                elem->pre_serialize();
            } 
        } else {
            elem->pre_serialize();
        }
    }

    virtual ~SerializerScope() {
        if (rnmap != NULL) {
            auto nmi = rnmap->find(elem);
            if (nmi != rnmap->end()) {
                tracker_element_serializer::post_serialize_path(nmi->second);
            } else {
                elem->post_serialize();
            } 
        } else {
            elem->post_serialize();
        }

    }

protected:
    shared_tracker_element elem;
    std::shared_ptr<tracker_element_serializer::rename_map> rnmap;
};

#endif
