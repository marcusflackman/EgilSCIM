/**
 * Copyright © 2017-2018  Max Wällstedt <>
 *
 * This file is part of EgilSCIM.
 *
 * EgilSCIM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EgilSCIM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with EgilSCIM.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Further development with groups and relations support
 * by Ola Mattsson - IT informa for Sambruk
 */

#include <iostream>
#include <algorithm>

#include "scim.hpp"
#include "utility/simplescim_error_string.hpp"
#include "utility/utils.hpp"
#include "model/base_object.hpp"
#include "model/object_list.hpp"
#include "config_file.hpp"
#include "cache_file.hpp"
#include "scim_json_parse.hpp"
#include "simplescim_scim_send.hpp"


variables::variables() {
    const config_file &config = config_file::instance();
    variable_entries.emplace(std::make_pair("cert", config.require("cert")));
    variable_entries.emplace(std::make_pair("key", config.require("key")));
}


int ScimActions::simplescim_scim_init() const {
    int err;

    /* Fetch variables from configuration file */

    if (!vars.valid()) {
        return -1;
    }

    /* Allocate new cache */

    if (scim_new_cache == nullptr) {
        return -1;
    }

    /* Initialise simplescim_scim_send */
    err = scim_sender::instance().send_init(vars.get("cert"),
                                           vars.get("key"),
                                           scim_server_info.get_pinned_public_keys(),
                                           scim_server_info.get_ca_bundle_path());

    if (err == -1) {
        return -1;
    }

    return 0;
}

void ScimActions::simplescim_scim_clear() const {
    /* Clear simplescim_scim_send */
    scim_sender::instance().send_clear();

    /* Delete new cache */
    scim_new_cache->clear();
}

/**
 * Compares 'current' to 'cache' and performs 'copy_func'
 * on objects in both 'current' and cache if they are equal,
 * 'create_func' on objects in 'current' but not in
 * 'cache', performs 'update_func' on object in both
 * 'current' and 'cache' if the object has been updated.
 */
void ScimActions::process_changes(const object_list& current,
                                 const object_list &cache,
                                 statistics& stats) const {
    int err;

    for (const auto &iter : current) {

        const std::string &uid = iter.first;

        auto object = iter.second;
        auto cached_object = cache.get_object(uid);

        if (cached_object == nullptr) {
            ++stats.n_create;
            auto create_functor = ScimActions::create_func(*object);
            err = create_functor(*this);

            if (err == -1) {
                ++stats.n_create_fail;
                std::cerr << simplescim_error_string_get() << std::endl;
            }
        } else {
            /* Object exists in 'cache' */
            if (*object == *cached_object) {

                // Object is the same, copy it
                ++stats.n_copy;
                auto copy_functor = ScimActions::copy_func(*cached_object);
                if (copy_functor(*this) == -1) {
                    ++stats.n_copy_fail;
                    std::cerr << simplescim_error_string_get() << std::endl;
                }
            } else {
                ++stats.n_update;
                ScimActions::update_func update_f(*object, *cached_object);

                if (update_f(*this) == -1) {
                    ++stats.n_update_fail;
                    std::cerr << simplescim_error_string_get() << std::endl;
                }
            }
        }
    }
}

/*
 * Performs 'delete_func' on objects in 'cache' but not
 * in 'current'.
 */
void ScimActions::process_deletes(const object_list& current,
                                  const object_list& cache,
                                  const std::string& type,
                                  statistics& stats) const {

    /** For every object in 'cache' of the given type */
    for (const auto &item : cache) {
        std::shared_ptr<base_object> object = item.second;
        if (object->getSS12000type() == type) {
            const std::string &uid = item.first;
            auto tmp = current.get_object(uid);

            if (tmp == nullptr) {
                // Object doesn't exist in 'current', delete it
                ++stats.n_delete;
                auto delete_f = ScimActions::delete_func(*object);
                int err = delete_f(*this);

                if (err == -1) {
                    ++stats.n_delete_fail;
                    fprintf(stderr, "%s\n", simplescim_error_string_get());
                }
            }
        }
    }
}

void ScimActions::print_statistics(const std::string& type,
                                   const statistics& stats) {
    printf("Status:   Success   Failure     Total  of type: %s\n", type.c_str());
    printf("Copy:   %9lu %9lu %9lu\n", stats.n_copy - stats.n_copy_fail, stats.n_copy_fail, stats.n_copy);
    printf("Create: %9lu %9lu %9lu\n", stats.n_create - stats.n_create_fail, stats.n_create_fail, stats.n_create);
    printf("Update: %9lu %9lu %9lu\n", stats.n_update - stats.n_update_fail, stats.n_update_fail, stats.n_update);
    printf("Delete: %9lu %9lu %9lu\n", stats.n_delete - stats.n_delete_fail, stats.n_delete_fail, stats.n_delete);
}

/**
 * Makes SCIM requests by comparing the two user lists and
 * reading JSON templates from the configuration file.
 * Updates (or creates) the cache file.
 * On success, zero is returned. On error, -1 is returned
 * and simplescim_error_string is set to an appropriate
 * error message.
 */
int ScimActions::perform(const data_server &current, const object_list &cached) const {
    int err;

    /* Initialise SCIM */
    err = simplescim_scim_init();

    if (err == -1) {
        return -1;
    }
    std::string types_string = config_file::instance().get("scim-type-send-order");
    string_vector types = string_to_vector(types_string);
    std::map<std::string, statistics> stats;
    for (const auto& type : types) {
        std::shared_ptr<object_list> allOfType = current.get_by_type(type);
        if (!allOfType) {
            allOfType = std::make_shared<object_list>();
        }
        process_changes(*allOfType, cached, stats[type]);
    }

    auto types_reversed(types);
    std::reverse(types_reversed.begin(), types_reversed.end());

    for (const auto& type : types_reversed) {
        std::shared_ptr<object_list> allOfType = current.get_by_type(type);
        if (!allOfType) {
            allOfType = std::make_shared<object_list>();
        }
        process_deletes(*allOfType, cached, type, stats[type]);        
    }
    
    for (const auto& type : types) {
        print_statistics(type, stats[type]);
    }

    /* Save new cache file */
    err = cache_file::instance().save(scim_new_cache);

    /* Clean up */
    simplescim_scim_clear();

    return err;
}

bool ScimActions::verify_json(const std::string & json, const std::string &type) const {
    if (json.empty())
        return false;
    else if (std::find(verified_types.begin(), verified_types.end(), type) != verified_types.end())
        return true;

    namespace pt = boost::property_tree;
    pt::ptree root;
    std::stringstream os;

    os << json;
    try {
        pt::read_json(os, root);
        verified_types.emplace_back(type);
    } catch (const boost::exception &ex) {
        std::cout << json << std::endl;
        std::cerr << "Failed to parse json " << boost::diagnostic_information(ex);
        return false;
    }
    return true;
}

int ScimActions::copy_func::operator()(const ScimActions &actions) {
    if (cached.get_uid().empty()) {
        return -1;
    }

    actions.scim_new_cache->add_object(cached.get_uid(),
                                       std::make_shared<base_object>(cached));

    return 0;

}

int ScimActions::delete_func::operator()(const ScimActions &actions) {

    if (object.get_uid().empty()) {
        simplescim_error_string_set_prefix("ScimActions::delete_func:"
                                           "get-attribute");
        simplescim_error_string_set_message("cached object does not have unique identifier attribute");
        return -1;
    }
    std::string url = actions.scim_server_info.get_url();
    std::string urlified = unifyurl(object.get_uid());
    std::string endpoint = config_file::instance().get(object.getSS12000type() + "-scim-url-endpoint");
    url += '/' + endpoint + '/' + urlified;
//	std::cout << url  << std::endl;
    /* Send SCIM delete request */
    int err = scim_sender::instance().send_delete(url);

    if (err != 0 && err != 404) { // Recache if delete failed, 404 is no failure
        actions.scim_new_cache->add_object(object.get_uid(), std::make_shared<base_object>(object));
    }

    return err;
}

int ScimActions::create_func::operator()(const ScimActions &actions) {

    base_object copied_user(create);

    /* Create JSON object for object */
    std::string type = copied_user.getSS12000type();
    if (type == "base") {
        type = "User";
    }

    std::string template_json = actions.conf.get(type + "-scim-json-template");
    std::string parsed_json = scim_json_parse(template_json, copied_user);

    if (!actions.verify_json(parsed_json, type))
        return -1;

    std::string url = actions.scim_server_info.get_url();
    std::string endpoint = config_file::instance().get(type + "-scim-url-endpoint");
    url += '/' + endpoint;
//	std::cout << url << " " << copied_user.get_uid() << std::endl;
    /* Send SCIM create request */
    std::optional<std::string> response_json = scim_sender::instance().send_create(url, parsed_json);
    std::string uid = copied_user.get_uid();
    if (response_json)
        actions.scim_new_cache->add_object(uid, std::make_shared<base_object>(copied_user));
    else
        return -1;

    return 0;
}

int ScimActions::update_func::operator()(const ScimActions &actions) {

    /* Copy object */
    base_object copied_user(object);

    std::string uid = copied_user.get_uid();

    if (uid.empty()) {
        return -1;
    }

    /* Create JSON object for object */
    std::string type = object.getSS12000type();
    if (type == "base") {
        type = "User";
    }
    std::string create_var = type + "-scim-json-template";
    std::string template_json = config_file::instance().get(create_var);
    std::string parsed_json = scim_json_parse(template_json, copied_user);

    if (!actions.verify_json(parsed_json, type)) {
        return -1;
    }

    std::string unified = unifyurl(object.get_uid());
    std::string url = actions.scim_server_info.get_url();
    std::string endpoint = config_file::instance().get(type + "-scim-url-endpoint");
    url += '/' + endpoint + '/' + unified;

    std::optional<std::string> response_json = scim_sender::instance().send_update(url, parsed_json);

    /* Insert copied object into new cache */
    if (!response_json) {
        actions.scim_new_cache->add_object(uid, std::make_shared<base_object>(object));
        return -1;
    } else {
        actions.scim_new_cache->add_object(uid, std::make_shared<base_object>(copied_user));
    }

    return 0;

}
