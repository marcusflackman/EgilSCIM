#ifndef SIMPLESCIM_LDAP_H
#define SIMPLESCIM_LDAP_H

#include "simplescim_user_list.h"

/**
 * Reads user data from LDAP into a user list object
 * according to the configuration file and returns a
 * pointer it. On error, NULL is returned and
 * simplescim_error_string is set to an appropriate error
 * message.
 */
struct simplescim_user_list *simplescim_ldap_get_users();

#endif