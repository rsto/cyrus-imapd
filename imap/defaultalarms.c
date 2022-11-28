#include "config.h"

#include "bsearch.h"
#include "caldav_util.h"
#include "defaultalarms.h"

HIDDEN int defaultalarms_read_annot(const char *mboxname,
                                           const char *userid,
                                           const char *annot,
                                           struct message_guid *guid,
                                           struct buf *content,
                                           int *is_dlistp)
{
    struct buf mybuf = BUF_INITIALIZER;
    annotatemore_lookup(mboxname, annot, userid, &mybuf);

    if (!buf_len(&mybuf))
        return CYRUSDB_NOTFOUND;

    if (buf_len(&mybuf)) {
        struct dlist *dl = NULL;
        if (dlist_parsemap(&dl, 1, 0, buf_base(&mybuf), buf_len(&mybuf)) == 0) {
            if (content) {
                const char *val = NULL;
                if (dlist_getatom(dl, "CONTENT", &val)) {
                    buf_setcstr(content, val);
                }
            }
            if (guid) {
                const char *guidrep = NULL;
                dlist_getatom(dl, "GUID", &guidrep);
                if (guidrep) {
                    message_guid_decode(guid, guidrep);
                }
            }
            if (is_dlistp) {
                *is_dlistp = 1;
            }
        }
        else {
            /* This is just the VALARM iCalendar string */
            if (guid) {
                message_guid_generate(guid, mybuf.s, mybuf.len);
            }
            if (content) {
                buf_copy(content, &mybuf);
            }
            if (is_dlistp) {
                *is_dlistp = 0;
            }
        }
        dlist_free(&dl);
    }

    buf_free(&mybuf);
    return 0;
}


static int load_alarms(const char *mboxname, const char *userid,
                       const char *annot, const char *fallback_annot,
                       icalcomponent **alarmsp)
{
    icalcomponent *ical = NULL;
    struct buf buf = BUF_INITIALIZER;
    *alarmsp = NULL;

    int r = defaultalarms_read_annot(mboxname,
            userid, annot, NULL, &buf, NULL);

    if (r == CYRUSDB_NOTFOUND && fallback_annot) {
        // Any new JMAP calendar should at least have the zero
        // value set in their default alarm annotation. If there
        // is no annotation set, this indicates that this user's
        // calendars did not get migrated to JMAP calendar default
        // alerts. Fall back reading their CalDAV alarms.
        buf_reset(&buf);
        r = defaultalarms_read_annot(mboxname,
                userid, fallback_annot, NULL, &buf, NULL);
    }

    if (r || !buf_len(&buf))
        goto done;

    ical = icalparser_parse_string(buf_cstring(&buf));
    if (ical) {
        if (icalcomponent_isa(ical) == ICAL_VALARM_COMPONENT) {
            // libical wraps multiple VALARMs in a XROOT,
            // so do the same for a single VALARM
            icalcomponent *root = icalcomponent_new(ICAL_XROOT_COMPONENT);
            icalcomponent_add_component(root, ical);
            ical = root;
        }
    }
    *alarmsp = ical;

done:
    buf_free(&buf);
    return r;
}


EXPORTED int defaultalarms_load(const char *mboxname,
                                const char *userid,
                                struct defaultalarms *alarms)
{
    int r = load_alarms(mboxname, userid,
            JMAP_ANNOT_DEFAULTALERTS_WITH_TIME,
            CALDAV_ANNOT_DEFAULTALARM_VEVENT_DATETIME,
            &alarms->with_time);

    if (!r) {
        r = load_alarms(mboxname, userid,
                JMAP_ANNOT_DEFAULTALERTS_WITHOUT_TIME,
                CALDAV_ANNOT_DEFAULTALARM_VEVENT_DATE,
                &alarms->with_date);
    }

    return r;
}

EXPORTED void defaultalarms_format_annot(struct buf *dst, const char *icalstr)
{
    struct dlist *dl = dlist_newkvlist(NULL, "DEFAULTALARMS");
    struct message_guid guid;
    if (*icalstr) {
        message_guid_generate(&guid, icalstr, strlen(icalstr));
    }
    else {
        message_guid_set_null(&guid);
    }
    dlist_setatom(dl, "CONTENT", icalstr);
    dlist_setatom(dl, "GUID", message_guid_encode(&guid));
    dlist_printbuf(dl, 1, dst);
    dlist_free(&dl);
}

static void init_alarms(icalcomponent *alarms)
{
    struct buf buf = BUF_INITIALIZER;

    icalcomponent *valarm;
    for (valarm = icalcomponent_get_first_component(alarms, ICAL_VALARM_COMPONENT);
         valarm;
         valarm = icalcomponent_get_next_component(alarms, ICAL_VALARM_COMPONENT)) {

        if (!icalcomponent_get_x_property_by_name(valarm, "X-JMAP-DEFAULT-ALARM")) {
            icalproperty *prop = icalproperty_new(ICAL_X_PROPERTY);
            icalproperty_set_x_name(prop, "X-JMAP-DEFAULT-ALARM");
            icalproperty_set_value(prop, icalvalue_new_boolean(1));
            icalcomponent_add_property(valarm, prop);
        }

        if (!icalcomponent_get_uid(valarm)) {
            icalcomponent *myalarm = icalcomponent_clone(valarm);
            icalcomponent_normalize(myalarm);
            buf_setcstr(&buf, icalcomponent_as_ical_string(myalarm));
            icalcomponent_free(myalarm);

            uint8_t digest[SHA1_DIGEST_LENGTH+1];
            xsha1(buf_base(&buf), buf_len(&buf), digest);
            digest[SHA1_DIGEST_LENGTH] = '\0';
            icalcomponent_set_uid(valarm, (const char*) digest);
        }
    }

    buf_free(&buf);
}

static int compare_valarm_reverse(const void **va, const void **vb)
{
    icalcomponent *a = (icalcomponent*)(*va);
    icalcomponent *b = (icalcomponent*)(*vb);

    // Regular alarms sort before snooze alarms
    int is_snooze_a =
        !!icalcomponent_get_first_property(a, ICAL_RELATEDTO_PROPERTY);
    int is_snooze_b =
        !!icalcomponent_get_first_property(b, ICAL_RELATEDTO_PROPERTY);

    if (is_snooze_a != is_snooze_b)
        return -(is_snooze_a - is_snooze_b);

    // Default alarms sort before non-default alarms
    int is_default_a =
        !!icalcomponent_get_x_property_by_name(a, "X-JMAP-DEFAULT-ALARM");
    int is_default_b =
        !!icalcomponent_get_x_property_by_name(b, "X-JMAP-DEFAULT-ALARM");

    if (is_default_a != is_default_b)
        return -(is_default_a - is_default_b);

    // Finally, sort by UID
    return -strcmpsafe(icalcomponent_get_uid(a), icalcomponent_get_uid(b));
}

static void merge_alarms(icalcomponent *comp, icalcomponent *alarms)
{
    // Remove existing alarms
    ptrarray_t old_alarms = PTRARRAY_INITIALIZER;
    strarray_t related_uids = STRARRAY_INITIALIZER;

    icalcomponent *valarm, *nextalarm;
    for (valarm = icalcomponent_get_first_component(comp, ICAL_VALARM_COMPONENT);
         valarm; valarm = nextalarm) {

        nextalarm = icalcomponent_get_next_component(comp, ICAL_VALARM_COMPONENT);

        icalcomponent_remove_component(comp, valarm);
        ptrarray_append(&old_alarms, valarm);

        icalproperty *prop = icalcomponent_get_first_property(valarm, ICAL_RELATEDTO_PROPERTY);
        if (prop) {
            const char *related_uid = icalproperty_get_relatedto(prop);
            if (related_uid)
                strarray_append(&related_uids, related_uid);
        }
    }

    // Create copy of new default alarms, if any
    ptrarray_t new_alarms = PTRARRAY_INITIALIZER;
    if (alarms) {
        icalcomponent *valarm;
        for (valarm = icalcomponent_get_first_component(alarms, ICAL_VALARM_COMPONENT);
             valarm;
             valarm = icalcomponent_get_next_component(alarms, ICAL_VALARM_COMPONENT)) {


            icalcomponent *myalarm = icalcomponent_clone(valarm);
            ptrarray_append(&new_alarms, myalarm);

            /* Replace default description with component summary */
            const char *desc = icalcomponent_get_summary(comp);
            if (desc && *desc != '\0') {
                icalproperty *prop =
                    icalcomponent_get_first_property(myalarm, ICAL_DESCRIPTION_PROPERTY);
                if (prop) {
                    icalcomponent_remove_property(myalarm, prop);
                    icalproperty_free(prop);
                }
                prop = icalproperty_new_description(desc);
                icalcomponent_add_property(myalarm, prop);
            }
        }
    }

    strarray_sort(&related_uids, cmpstringp_raw);

    // Sort in reverse order, we'll pop from the arrays later.
    ptrarray_sort(&old_alarms, compare_valarm_reverse);
    ptrarray_sort(&new_alarms, compare_valarm_reverse);

    int is_recur_main =
        !icalcomponent_get_first_property(comp, ICAL_RECURRENCEID_PROPERTY) &&
        (icalcomponent_get_first_property(comp, ICAL_RRULE_PROPERTY) ||
         icalcomponent_get_first_property(comp, ICAL_RDATE_PROPERTY));

    // Combine old and new default alarms
    icalcomponent *old, *new;
    do {
        old = ptrarray_pop(&old_alarms);
        new = ptrarray_pop(&new_alarms);

        if (new) {
            // Add JMAP default alarm
            icalcomponent_add_component(comp, new);
            if (old) {
                const char *old_uid = icalcomponent_get_uid(old);
                const char *new_uid = icalcomponent_get_uid(new);
                if (!strcmpsafe(old_uid, new_uid)) {
                    // A default alarm with the same UID already
                    // existed in the component. Use its new
                    // definition, but keep it acknowledged.
                    icalproperty *prop, *nextprop;
                    for (prop = icalcomponent_get_first_property(old,
                                ICAL_ACKNOWLEDGED_PROPERTY);
                         prop; prop = nextprop) {

                        nextprop = icalcomponent_get_next_property(old,
                                ICAL_ACKNOWLEDGED_PROPERTY);
                        icalcomponent_remove_property(old, prop);
                        icalcomponent_add_property(new, prop);
                    }

                    // Throw away old alarm
                    icalcomponent_free(old);
                    old = NULL;
                }
            }
        }

        if (old) {
            int is_default =
                !!icalcomponent_get_x_property_by_name(old, "X-JMAP-DEFAULT-ALARM");

            if (is_default) {
                // This is an outdated default alarm.
                const char *old_uid = icalcomponent_get_uid(old);
                int is_related = old_uid &&
                    strarray_find(&related_uids, old_uid, 0) >= 0;

                int is_acked = !!icalcomponent_get_first_property(old,
                        ICAL_ACKNOWLEDGED_PROPERTY);

                if (is_related || is_acked) {
                    // Keep acknowledged and snoozed alarms
                    icalcomponent_add_component(comp, old);

                    if (!is_acked) {
                        // Acknowledge so that clients ignore it.
                        icalcomponent_add_property(old,
                                icalproperty_new_acknowledged(
                                    icaltime_current_time_with_zone(
                                        icaltimezone_get_utc_timezone())));

                    }

                    if (is_recur_main) {
                        // Make sure this alarm can't fire anymore.
                        icalproperty *trigger =
                            icalcomponent_get_first_property(old, ICAL_TRIGGER_PROPERTY);
                        if (trigger) {
                            // Use Apple's magic 5545 timestamp
                            struct icaltriggertype expired_trigger = {
                                .time = {
                                    .year = 1976,
                                    .month = 4,
                                    .day = 1,
                                    .hour = 0,
                                    .minute = 55,
                                    .second = 45,
                                    .zone = icaltimezone_get_utc_timezone()
                                }
                            };
                            icalproperty_set_trigger(trigger, expired_trigger);
                        }
                    }
                }
                else icalcomponent_free(old);
            }
            else {
                // Keep user-defined alarm
                icalcomponent_add_component(comp, old);
            }
        }
    } while (old || new);

    ptrarray_fini(&old_alarms);
    ptrarray_fini(&new_alarms);
    strarray_fini(&related_uids);
}

EXPORTED void defaultalarms_insert(struct defaultalarms *alarms,
                                   icalcomponent *ical, int force)
{
    if (!alarms || (!alarms->with_time && !alarms->with_date))
        return;

    if (alarms->with_time)
        init_alarms(alarms->with_time);

    if (alarms->with_date)
        init_alarms(alarms->with_date);

    icalcomponent *comp = icalcomponent_get_first_real_component(ical);
    icalcomponent_kind kind = icalcomponent_isa(comp);
    if (kind != ICAL_VEVENT_COMPONENT && kind != ICAL_VTODO_COMPONENT)
        return;

    for ( ; comp; comp = icalcomponent_get_next_component(ical, kind)) {

        if (!force && icalcomponent_get_usedefaultalerts(comp) <= 0)
            continue;

        // Determine which default alarms to add
        int is_date;
        if (kind == ICAL_VTODO_COMPONENT) {
            if (icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY))
                is_date = icalcomponent_get_dtstart(comp).is_date;
            else if (icalcomponent_get_first_property(comp, ICAL_DUE_PROPERTY))
                is_date = icalcomponent_get_due(comp).is_date;
            else
                is_date = 1;
        }
        else is_date = icalcomponent_get_dtstart(comp).is_date;

        merge_alarms(comp, is_date ?  alarms->with_date : alarms->with_time);
    }
}

EXPORTED void defaultalarms_fini(struct defaultalarms *defalarms)
{
    if (defalarms) {
        if (defalarms->with_time)
            icalcomponent_free(defalarms->with_time);

        if (defalarms->with_date)
            icalcomponent_free(defalarms->with_date);
    }
}

