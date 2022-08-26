/* cyrusdb_rocksdb.c - Support for RocksDB
 *
 * Copyright (c) 1994-2018 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Carnegie Mellon University
 *      Center for Technology Transfer and Enterprise Creation
 *      4615 Forbes Avenue
 *      Suite 302
 *      Pittsburgh, PA  15213
 *      (412) 268-7393, fax: (412) 268-7395
 *      innovation@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <list>
#include <memory>
#include <string>

using std::list;
using std::string;
using std::unique_ptr;

/****** CYRUS DB API ******/

#include <config.h>

#include "cyrusdb.h"

#include <string>

extern "C" {

struct dbengine {
    string path;
    int refcount;
};

struct txn {
};

static list<unique_ptr<struct dbengine>> db_list;

HIDDEN int myinit(const char *dbdir __attribute__((unused)),
                  int myflags __attribute__((unused)))
{
    return 0;
}

HIDDEN int mydone(void)
{
    return 0;
}

HIDDEN int mysync(void)
{
    return 0;
}

HIDDEN int myarchive(const strarray_t *fnames __attribute__((unused)),
                     const char *dirname __attribute__((unused)))
{
    return 0;
}


HIDDEN int myunlink(const char *fname __attribute__((unused)),
                    int flags __attribute__((unused)))
{
    return 0;
}

static int myopen(const char *path __attribute__((unused)),
                  int flags  __attribute__((unused)),
                  struct dbengine **ret  __attribute__((unused)),
                  struct txn **mytid  __attribute__((unused)))
{
    for (auto it = db_list.begin(); it != db_list.end(); ++it) {
        if ((*it)->path.compare(path) == 0) {
            ++(*it)->refcount;
            *ret = (*it).get();
            return 0;
        }
    }

    unique_ptr<struct dbengine> dbe {new dbengine};
    db_list.push_back(std::move(dbe));
    *ret = db_list.back().get();

    return 0;
}

static int myclose(struct dbengine *dbe)
{
    for (auto it = db_list.begin(); it != db_list.end(); ++it) {
        if ((*it).get() == dbe) {
            if (0 == --(*it)->refcount) {
                it = db_list.erase(it);
            }
            return 0;
        }
    }
    return 0;
}

static int myfetch(struct dbengine *db __attribute__((unused)),
                   const char *key __attribute__((unused)),
                   size_t keylen __attribute__((unused)),
                   const char **data __attribute__((unused)),
                   size_t *datalen __attribute__((unused)),
                   struct txn **tidptr __attribute__((unused)))
{
    return 0;
}

static int myfetchlock(struct dbengine *db __attribute__((unused)),
                       const char *key __attribute__((unused)),
                       size_t keylen __attribute__((unused)),
                       const char **data __attribute__((unused)),
                       size_t *datalen __attribute__((unused)),
                       struct txn **tidptr __attribute__((unused)))
{
    return 0;
}

static int myfetchnext(struct dbengine *db __attribute__((unused)),
                       const char *key __attribute__((unused)),
                       size_t keylen __attribute__((unused)),
                       const char **foundkey __attribute__((unused)),
                       size_t *fklen __attribute__((unused)),
                       const char **data  __attribute__((unused)),
                       size_t *datalen  __attribute__((unused)),
                       struct txn **tidptr __attribute__((unused)))
{
    return 0;
}

static int myforeach(struct dbengine *db  __attribute__((unused)),
                     const char *prefix  __attribute__((unused)),
                     size_t prefixlen  __attribute__((unused)),
                     foreach_p *goodp  __attribute__((unused)),
                     foreach_cb *cb  __attribute__((unused)),
                     void *rock  __attribute__((unused)),
                     struct txn **tidptr  __attribute__((unused)))
{
    return 0;
}

static int mycreate(struct dbengine *db __attribute__((unused)),
                    const char *key __attribute__((unused)),
                    size_t keylen __attribute__((unused)),
                    const char *data __attribute__((unused)),
                    size_t datalen __attribute__((unused)),
                    struct txn **tidptr __attribute__((unused)))
{
    return 0;
}

static int mystore(struct dbengine *db __attribute__((unused)),
                   const char *key __attribute__((unused)),
                   size_t keylen __attribute__((unused)),
                   const char *data __attribute__((unused)),
                   size_t datalen __attribute__((unused)),
                   struct txn **tidptr __attribute__((unused)))
{
    return 0;
}

static int mydelete(struct dbengine *db __attribute__((unused)),
                    const char *key __attribute__((unused)),
                    size_t keylen __attribute__((unused)),
                    struct txn **tidptr __attribute__((unused)),
                    int force __attribute__((unused)))
{
    return 0;
}


static int mycommit(struct dbengine *db __attribute__((unused)),
                    struct txn *tid __attribute__((unused)))
{
    return 0;
}

static int myabort(struct dbengine *db __attribute__((unused)),
                   struct txn *tid __attribute__((unused)))
{
    return 0;
}

/* mydump:
   if detail == 1, dump all records.
   if detail == 2, dump active records only
*/
static int mydump(struct dbengine *db __attribute__((unused)),
                  int detail __attribute__((unused)))
{
    return 0;
}

static int myconsistent(struct dbengine *db __attribute__((unused)))
{
    return 0;
}

static int mycheckpoint(struct dbengine *db __attribute__((unused)))
{
    return 0;
}

static int mycompar(struct dbengine *db __attribute__((unused)),
                    const char *a __attribute__((unused)),
                    int alen __attribute__((unused)),
                    const char *b __attribute__((unused)),
                    int blen __attribute__((unused)))
{
    return 0;
}


HIDDEN struct cyrusdb_backend cyrusdb_rocksdb =
{
    "rocksdb",

    &myinit,
    &mydone,
    &mysync,
    &myarchive,
    &myunlink,

    &myopen,
    &myclose,

    &myfetch,
    &myfetchlock,
    &myfetchnext,

    &myforeach,
    &mycreate,
    &mystore,
    &mydelete,

    &mycommit,
    &myabort,

    &mydump,
    &myconsistent,
    &mycheckpoint,
    &mycompar
};

}

