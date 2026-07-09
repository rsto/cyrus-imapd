/* jscontact.h -- Routines for converting JSContact and vCard */
/* SPDX-License-Identifier: BSD-3-Clause-CMU */
/* See COPYING file at the root of the distribution for more details. */

#ifndef JSCONTACT_H
#define JSCONTACT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <jansson.h>
#include <libical/vcard.h>

#include "jmap_util.h"

#define JSCONTACT_MAJOR_VERSION 1 /**< The current JSContact major version. */
#define JSCONTACT_MINOR_VERSION 0 /**< The current JSContact minor version. */

/** @brief Configuration for JSContact/vCard conversion */
typedef struct {
    // No config options available for now.
} jscontact_cfg_t;

/** @brief Convert a JSContact Card to a vCard.
 *
 *  @param cfg     Conversion configuration, or NULL for defaults.
 *  @param jcard   JSON object representing a JSContact Card.
 *  @param parser  JMAP parser used to report property errors.
 *  @return        A newly allocated vCard component, or NULL on error. */
vcardcomponent *jscontact_to_vcard(jscontact_cfg_t *cfg,
                                   json_t *jcard,
                                   struct jmap_parser *parser);

/** @brief Convert a vCard to a JSContact Card.
 *
 *  @param cfg    Conversion configuration, or NULL for defaults.
 *  @param vcard  A vCard component.
 *  @return       A newly allocated JSON object representing a JSContact Card,
 *                or NULL on error. */
json_t *jscontact_from_vcard(jscontact_cfg_t *cfg, vcardcomponent *vcard);

#ifdef __cplusplus
}
#endif

#endif
