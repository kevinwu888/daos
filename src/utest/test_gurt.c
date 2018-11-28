/* Copyright (C) 2016-2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted for any purpose (including commercial purposes)
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the
 *    documentation and/or materials provided with the distribution.
 *
 * 3. In addition, redistributions of modified forms of the source or binary
 *    code must carry prominent notices stating that the original code was
 *    changed and the date of the change.
 *
 *  4. All publications or advertising materials mentioning features or use of
 *     this software are asked, but not required, to acknowledge that it was
 *     developed by Intel Corporation and credit the contributors.
 *
 * 5. Neither the name of Intel Corporation, nor the name of any Contributor
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file tests macros in GURT
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "utest_cmocka.h"
#include "gurt/common.h"
#include "gurt/list.h"
#include "gurt/heap.h"
#include "gurt/dlog.h"
#include "gurt/hash.h"

/* machine epsilon */
#define EPSILON (1.0E-16)

static char *__root;

static void
test_time(void **state)
{
	struct timespec	t1;
	struct timespec	t2;
	uint64_t	timeleft;

	t1.tv_sec = 1;
	t1.tv_nsec = 1;
	d_timeinc(&t1, NSEC_PER_SEC + 1);

	assert_int_equal(t1.tv_sec, 2);
	assert_int_equal(t1.tv_nsec, 2);

	t2.tv_sec = 0;
	t2.tv_nsec = 0;
	assert_int_equal(d_timediff_ns(&t2, &t1),
			 (NSEC_PER_SEC * 2) + 2);

	t2.tv_sec = 2;
	t2.tv_nsec = 2 + NSEC_PER_USEC;
	assert(d_time2us(d_timediff(t1, t2)) - 1.0 < EPSILON);
	assert(d_time2us(d_timediff(t2, t1)) + 1.0 < EPSILON);

	t2.tv_nsec = 2 + NSEC_PER_MSEC;
	assert(d_time2ms(d_timediff(t1, t2)) - 1.0 < EPSILON);
	assert(d_time2ms(d_timediff(t2, t1)) + 1.0 < EPSILON);

	t2.tv_sec = 3;
	t2.tv_nsec = 2;
	assert(d_time2s(d_timediff(t1, t2)) - 1.0 < EPSILON);
	assert(d_time2s(d_timediff(t2, t1)) + 1.0 < EPSILON);

	t2.tv_sec = 2;
	t2.tv_nsec = 2;
	assert_int_equal(d_timediff_ns(&t2, &t1), 0);

	t2.tv_sec = 3;
	t2.tv_nsec = 2;
	assert_int_equal(d_timediff_ns(&t2, &t1), -NSEC_PER_SEC);

	t2.tv_sec = 2;
	t2.tv_nsec = 3;
	assert_int_equal(d_timediff_ns(&t2, &t1), -1);

	t2.tv_nsec = 1;
	assert_int_equal(d_timediff_ns(&t2, &t1),
			 1);

	d_timeinc(&t1, 100000);

	assert_int_equal(t1.tv_sec, 2);
	assert_int_equal(t1.tv_nsec, 100002);

	d_gettime(&t1);
	d_timeinc(&t1, NSEC_PER_SEC / 10);

	timeleft = d_timeleft_ns(&t1);
	/* This check shouldn't take 1 second */
	assert_in_range(timeleft, 0, NSEC_PER_SEC);

	/* Sleep for 1 second.  Time should expire */
	sleep(1);

	timeleft = d_timeleft_ns(&t1);
	assert_int_equal(timeleft, 0);
}

#define D_CHECK_STRLIMITS(name, base)				\
	do {							\
		value = d_errstr(-DER_ERR_##name##_BASE);	\
		assert_string_equal(value, "DER_UNKNOWN");	\
		value = d_errstr(-DER_ERR_##name##_LIMIT);	\
		assert_string_equal(value, "DER_UNKNOWN");	\
		value = d_errstr(DER_ERR_##name##_BASE);	\
		assert_string_equal(value, "DER_UNKNOWN");	\
		value = d_errstr(DER_ERR_##name##_LIMIT);	\
		assert_string_equal(value, "DER_UNKNOWN");	\
	} while (0);

#define D_CHECK_ERR_IN_RANGE(name, value)			\
	do {							\
		const char	*str = d_errstr(name);		\
		assert_string_not_equal(str, "DER_UNKNOWN");	\
	} while (0);

#define D_CHECK_IN_RANGE(name, base)		\
	D_FOREACH_##name##_ERR(D_CHECK_ERR_IN_RANGE)

void test_d_errstr(void **state)
{
	const char	*value;

	D_FOREACH_ERR_RANGE(D_CHECK_STRLIMITS)
	D_FOREACH_ERR_RANGE(D_CHECK_IN_RANGE)
	value = d_errstr(-DER_INVAL);
	assert_string_equal(value, "DER_INVAL");
	value = d_errstr(DER_INVAL);
	assert_string_equal(value, "DER_INVAL");
	value = d_errstr(5000000);
	assert_string_equal(value, "DER_UNKNOWN");
	value = d_errstr(3);
	assert_string_equal(value, "DER_UNKNOWN");
	value = d_errstr(-3);
	assert_string_equal(value, "DER_UNKNOWN");
	value = d_errstr(0);
	assert_string_equal(value, "DER_SUCCESS");
	value = d_errstr(DER_SUCCESS);
	assert_string_equal(value, "DER_SUCCESS");
	value = d_errstr(-DER_IVCB_FORWARD);
	assert_string_equal(value, "DER_IVCB_FORWARD");
	value = d_errstr(-DER_FREE_MEM);
	assert_string_equal(value, "DER_FREE_MEM");
	value = d_errstr(-DER_STALE);
	assert_string_equal(value, "DER_STALE");
}

static int
init_tests(void **state)
{
	char		*tmp;
	unsigned int	 seed;

	D_STRNDUP(__root, "/tmp/XXXXXX", 32);
	tmp = mkdtemp(__root);

	if (tmp != __root) {
		fprintf(stderr, "Could not create tmp dir\n");
		return -1;
	}

	/* Seed the random number generator once per test run */
	seed = time(NULL);
	fprintf(stdout, "Seeding this test run with seed=%u\n", seed);
	srand(seed);

	return d_log_init();
}

static int
fini_tests(void **state)
{
	rmdir(__root);
	free(__root);
	d_log_fini();

	return 0;
}

struct d_list_test_entry {
	int num;
	d_list_t link;
};

static D_LIST_HEAD(head1);

#define NUM_ENTRIES 20

static void
assert_list_node_status(void **state, d_list_t *head, int value, bool in_list)
{
	d_list_t			*pos;
	struct d_list_test_entry	*entry;

	d_list_for_each(pos, head) {
		entry = d_list_entry(pos, struct d_list_test_entry, link);
		if (entry->num == value) {
			if (in_list)
				return;
			assert(0);
		}
	}

	if (in_list)
		assert(0);
}

static void
assert_list_node_count(void **state, d_list_t *head, int count)
{
	d_list_t	*pos;
	int		i = 0;

	d_list_for_each(pos, head) {
		i++;
	}

	assert_int_equal(i, count);
}

static void
test_gurt_list(void **state)
{
	d_list_t			*pos;
	d_list_t			*temp;
	d_list_t			head2;
	d_list_t			head3;
	struct d_list_test_entry	*entry;
	struct d_list_test_entry	*tentry;
	struct d_list_test_entry	entry2;
	struct d_list_test_entry	entry3;
	int		i;

	D_INIT_LIST_HEAD(&head2);
	D_INIT_LIST_HEAD(&head3);

	entry2.num = 2000;
	entry3.num = 3000;
	d_list_add(&entry3.link, &head3);
	assert(!d_list_empty(&head3));
	d_list_splice(&head2, &head3);
	assert(!d_list_empty(&head3));
	D_INIT_LIST_HEAD(&head2);
	d_list_splice(&head3, &head2);
	assert(!d_list_empty(&head2));
	d_list_del(&entry3.link);
	assert(d_list_empty(&head2));
	D_INIT_LIST_HEAD(&head2);
	D_INIT_LIST_HEAD(&head3);
	d_list_add(&entry3.link, &head3);
	d_list_add(&entry2.link, &head2);
	d_list_splice(&head3, &head2);
	assert_list_node_count(state, &head2, 2);
	D_INIT_LIST_HEAD(&head3);
	d_list_move(&entry2.link, &head3);
	assert_list_node_status(state, &head3, entry2.num, true);
	assert_list_node_status(state, &head2, entry3.num, true);
	d_list_move_tail(&entry2.link, &head2);
	assert_list_node_status(state, &head2, entry2.num, true);
	assert_list_node_status(state, &head3, entry2.num, false);

	D_INIT_LIST_HEAD(&head2);

	for (i = NUM_ENTRIES * 2 - 1; i >= NUM_ENTRIES; i--) {
		D_ALLOC(entry, sizeof(struct d_list_test_entry));
		assert_non_null(entry);
		entry->num = i;
		d_list_add(&entry->link, &head2);
		assert_list_node_status(state, &head2, i, true);

		d_list_del_init(&entry->link);
		assert(d_list_empty(&entry->link));
		assert_list_node_status(state, &head2, i, false);

		d_list_add(&entry->link, &head2);
		assert_list_node_status(state, &head2, i, true);
	}

	for (i = 0; i < NUM_ENTRIES; i++) {
		D_ALLOC(entry, sizeof(struct d_list_test_entry));
		assert_non_null(entry);
		entry->num = i;
		d_list_add_tail(&entry->link, &head1);
		assert_list_node_status(state, &head1, i, true);

		d_list_del(&entry->link);
		assert_list_node_status(state, &head1, i, false);

		d_list_add_tail(&entry->link, &head1);
		assert_list_node_status(state, &head1, i, true);
	}

	d_list_splice_init(&head1, &head2);

	assert(d_list_empty(&head1));
	assert_list_node_count(state, &head2, NUM_ENTRIES * 2);

	i = 0;
	d_list_for_each_entry(entry, &head2, link) {
		assert_int_equal(i, entry->num);
		i++;
	}

	i = NUM_ENTRIES * 2 - 1;
	d_list_for_each_entry_reverse(entry, &head2, link) {
		assert_int_equal(i, entry->num);
		i--;
	}

	i = 0;
	d_list_for_each_safe(pos, temp, &head2) {
		entry = d_list_entry(pos, struct d_list_test_entry, link);
		assert_int_equal(i, entry->num);
		i++;
		if (i == NUM_ENTRIES)
			break;
		d_list_del(pos);
		D_FREE(entry);
	}

	d_list_for_each_entry_continue(entry, &head2, link) {
		assert_int_equal(i, entry->num);
		i++;
	}

	d_list_for_each_entry_safe(entry, tentry, &head2, link) {
		d_list_del(&entry->link);
		D_FREE(entry);
	}

	assert(d_list_empty(&head2));
}

struct dhlist_test_entry {
	int num;
	d_hlist_node_t link;
};

static D_HLIST_HEAD(hhead1);

static void
assert_hlist_node_status(void **state, d_hlist_head_t *head,
			 int value, bool in_list)
{
	d_hlist_node_t		*pos;
	struct dhlist_test_entry	*entry;

	dhlist_for_each(pos, head) {
		entry = d_hlist_entry(pos, struct dhlist_test_entry, link);
		if (entry->num == value) {
			if (in_list)
				return;
			assert(0);
		}
	}

	if (in_list)
		assert(0);
}

static void
assert_hlist_node_count(void **state, d_hlist_head_t *head, int count)
{
	d_hlist_node_t	*pos;
	int			i = 0;

	dhlist_for_each(pos, head) {
		i++;
	}

	assert_int_equal(i, count);
}

static void
test_gurt_hlist(void **state)
{
	d_hlist_node_t		*pos;
	d_hlist_node_t		*temp;
	d_hlist_head_t		head2;
	struct dhlist_test_entry	*entry;
	struct dhlist_test_entry	entry2;
	struct dhlist_test_entry	entry3;
	int		i;

	D_INIT_HLIST_NODE(&entry2.link);
	D_INIT_HLIST_NODE(&entry3.link);
	entry2.num = 2000;
	entry3.num = 3000;
	d_hlist_add_head(&entry3.link, &hhead1);
	d_hlist_add_before(&entry2.link, &entry3.link);
	assert(!d_hlist_empty(&hhead1));
	assert_hlist_node_status(state, &hhead1, entry2.num, true);
	assert_hlist_node_status(state, &hhead1, entry3.num, true);
	assert_hlist_node_count(state, &hhead1, 2);
	assert_non_null(entry2.link.next);
	assert_non_null(entry3.link.pprev);
	assert_int_equal(entry2.link.next, &entry3.link);
	assert_int_equal(entry3.link.pprev, &entry2.link);
	d_hlist_del_init(&entry2.link);
	assert_hlist_node_status(state, &hhead1, entry2.num, false);
	assert_hlist_node_count(state, &hhead1, 1);
	d_hlist_add_after(&entry2.link, &entry3.link);
	assert_hlist_node_count(state, &hhead1, 2);
	assert_non_null(entry2.link.pprev);
	assert_non_null(entry3.link.next);
	assert_int_equal(entry3.link.next, &entry2.link);
	assert_int_equal(entry2.link.pprev, &entry3.link);
	assert_hlist_node_status(state, &hhead1, entry2.num, true);
	assert_hlist_node_status(state, &hhead1, entry3.num, true);
	assert_hlist_node_count(state, &hhead1, 2);

	D_INIT_HLIST_HEAD(&head2);

	for (i = NUM_ENTRIES - 1; i >= 0; i--) {
		D_ALLOC(entry, sizeof(struct dhlist_test_entry));
		assert_non_null(entry);
		entry->num = i;
		d_hlist_add_head(&entry->link, &head2);
		assert_hlist_node_status(state, &head2, i, true);

		d_hlist_del_init(&entry->link);
		assert_hlist_node_status(state, &head2, i, false);

		d_hlist_add_head(&entry->link, &head2);
		assert_hlist_node_status(state, &head2, i, true);
	}

	assert_hlist_node_count(state, &head2, NUM_ENTRIES);

	i = 0;
	dhlist_for_each_entry(entry, pos, &head2, link) {
		assert_int_equal(i, entry->num);
		i++;
	}

	i = 0;
	dhlist_for_each_safe(pos, temp, &head2) {
		entry = d_hlist_entry(pos, struct dhlist_test_entry, link);
		assert_int_equal(i, entry->num);
		i++;
		if (i == NUM_ENTRIES / 2)
			break;
		d_hlist_del(pos);
		D_FREE(entry);
	}

	dhlist_for_each_entry_continue(entry, pos, link) {
		assert_int_equal(i, entry->num);
		i++;
	}

	dhlist_for_each_entry_safe(entry, pos, temp, &head2, link) {
		d_hlist_del(&entry->link);
		D_FREE(entry);
	}

	assert(d_hlist_empty(&head2));
}

struct test_minheap_node {
	struct d_binheap_node		dbh_node;
	int				key;
};

static bool
heap_node_cmp(struct d_binheap_node *a, struct d_binheap_node *b)
{
	struct test_minheap_node	*nodea, *nodeb;

	nodea = container_of(a, struct test_minheap_node, dbh_node);
	nodeb = container_of(b, struct test_minheap_node, dbh_node);

	return nodea->key < nodeb->key;
}

static void
test_binheap(void **state)
{
	struct d_binheap		*h = NULL;
	struct test_minheap_node	 n1, n2, n3;
	struct d_binheap_node		*n_tmp;
	uint32_t			 size;
	int				 rc;
	struct d_binheap_ops		 ops = {
		.hop_enter	= NULL,
		.hop_exit	= NULL,
		.hop_compare	= heap_node_cmp,
	};

	(void)state;

	rc = d_binheap_create(0, 0, NULL, &ops, &h);
	assert_int_equal(rc, 0);
	assert_non_null(h);

	n1.key = 1;
	n2.key = 2;
	n3.key = 3;

	rc = d_binheap_insert(h, &n1.dbh_node);
	assert_int_equal(rc, 0);
	rc = d_binheap_insert(h, &n2.dbh_node);
	assert_int_equal(rc, 0);
	rc = d_binheap_insert(h, &n3.dbh_node);
	assert_int_equal(rc, 0);

	n_tmp = d_binheap_root(h);
	assert_true(n_tmp == &n1.dbh_node);

	d_binheap_remove(h, &n1.dbh_node);
	n_tmp = d_binheap_root(h);
	assert_true(n_tmp == &n2.dbh_node);

	n_tmp = d_binheap_find(h, 0);
	assert_true(n_tmp == &n2.dbh_node);
	n_tmp = d_binheap_find(h, 1);
	assert_true(n_tmp == &n3.dbh_node);
	n_tmp = d_binheap_find(h, 2);
	assert_true(n_tmp == NULL);

	size = d_binheap_size(h);
	assert_true(size == 2);

	n_tmp = d_binheap_remove_root(h);
	assert_true(n_tmp == &n2.dbh_node);
	size = d_binheap_size(h);
	assert_true(size == 1);

	d_binheap_destroy(h);
}

#define LOG_DEBUG(fac, ...) \
	do {								\
		if (d_log_check((fac) | DLOG_DBG))			\
			d_log(d_log_check((fac) | DLOG_DBG), __VA_ARGS__);\
	} while (0)

#define LOG_INFO(fac, ...) \
	do {								\
		if (d_log_check((fac) | DLOG_INFO))			\
			d_log(d_log_check((fac) | DLOG_INFO), __VA_ARGS__);\
	} while (0)

static void
test_log(void **state)
{
	char *logmask;
	char *allocated_mask = NULL;
	int rc;
	int rc_dbgbit;
	int logfac1;
	int logfac2;
	char retbuf[1024];
	char *oldmask;
	uint64_t dbg_mask;
	uint64_t current_dbgmask;

	oldmask = getenv("D_LOG_MASK");

	setenv("D_LOG_MASK", "CLOG=DEBUG,T1=DEBUG", 1);
	memset(retbuf, 0x00, sizeof(retbuf));
	rc = d_log_init();
	assert_int_equal(rc, 0);

	logfac1 = d_log_allocfacility("T1", "TEST1");
	assert_int_not_equal(logfac1, 0);

	logfac2 = d_log_allocfacility("T2", "TEST2");
	assert_int_not_equal(logfac2, 0);

	LOG_DEBUG(logfac1, "log1 debug should not print\n");
	/* Sync the cart mask */
	d_log_sync_mask();

	LOG_DEBUG(logfac1, "log1 debug should print\n");
	LOG_DEBUG(logfac2, "log2 debug should not print\n");

	/* Alternatively, a component may have its own mask */
	logmask = getenv("TEST_LOG_MASK");
	if (logmask == NULL) {
		D_STRNDUP(allocated_mask, "ERR,T1=DEBUG,CLOG=DEBUG", 32);
		logmask = allocated_mask;
	}
	assert_non_null(logmask);

	rc = d_log_setmasks(logmask, -1);
	LOG_DEBUG(logfac1, "rc after 1st setmaks is %x\n", rc);
	rc = d_log_setmasks(logmask, -1);
	LOG_DEBUG(logfac1, "rc after 2nd setmasks is %x\n", rc);
	D_FREE(allocated_mask);

	d_log_getmasks(retbuf, 0, 1024, 0);
	LOG_DEBUG(logfac1, "log mask: %s\n\n", retbuf);
	memset(retbuf, 0x00, sizeof(retbuf));

	LOG_DEBUG(logfac1, "log1 debug test message %d\n", logfac1);
	LOG_DEBUG(logfac2, "log2 debug test message %d\n", logfac2);
	LOG_INFO(logfac1, "log1 info test message %d\n", logfac2);
	LOG_INFO(logfac2, "log2 info test message %d\n", logfac2);

	/* Test debug mask bits*/

	/* Attempt to set debug mask bits with facility mask not set to DEBUG */
	setenv("D_LOG_MASK", "T2=WARN", 1);
	setenv("DD_MASK", "trace", 1);
	d_log_sync_mask();
	D_STRNDUP(logmask, "T2=WARN", 32);
	assert_non_null(logmask);

	rc = d_log_setmasks(logmask, -1);
	assert_int_equal(rc & DLOG_PRIMASK, (3 << DLOG_PRISHIFT));
	D_FREE(logmask);

	rc = d_log_getmasks(retbuf, 0, 200, 0);
	LOG_DEBUG(logfac1, "log mask: %s\n\n", retbuf);
	memset(retbuf, 0x00, sizeof(retbuf));

	/* Set trace debug mask */
	setenv("D_LOG_MASK", "T1=DEBUG", 1);
	setenv("DD_MASK", "trace", 1); /* DB_TRACE stream is set */
	d_log_sync_mask();
	D_STRNDUP(logmask, "T1=DEBUG", 32);
	assert_non_null(logmask);

	rc = d_log_setmasks(logmask, -1);
	rc_dbgbit = d_log_getdbgbit(&dbg_mask, "trace");
	if (rc_dbgbit < 0)
		D_ERROR("Unable to get debug bit mask for trace\n");
	assert_int_equal(dbg_mask, (uint64_t)(rc));
	D_FREE(logmask);

	rc = d_log_getmasks(retbuf, 0, 200, 0);

	LOG_DEBUG(logfac1, "log mask: %s\n\n", retbuf);
	memset(retbuf, 0x00, sizeof(retbuf));

	/* Set test debug mask */
	setenv("DD_MASK", "test", 1); /* DB_TEST stream is now also set */
	d_log_sync_mask();
	D_STRNDUP(logmask, "T1=DEBUG", 32);
	assert_non_null(logmask);

	rc = d_log_setmasks(logmask, -1);
	current_dbgmask = dbg_mask;
	rc_dbgbit = d_log_getdbgbit(&dbg_mask, "test");
	if (rc_dbgbit < 0)
		D_ERROR("Unable to get debug bit mask for test\n");
	dbg_mask |= current_dbgmask;
	assert_int_equal(dbg_mask, (uint64_t)(rc));
	D_FREE(logmask);

	rc = d_log_getmasks(retbuf, 0, 200, 0);
	LOG_DEBUG(logfac1, "log mask: %s\n\n", retbuf);
	memset(retbuf, 0x00, sizeof(retbuf));

	if (oldmask)
		d_log_setmasks(oldmask, -1);
	else
		d_log_setmasks("ERR", -1);

	d_log_fini();
}

#define TEST_GURT_HASH_NUM_BITS (12)
#define TEST_GURT_HASH_NUM_ENTRIES (1 << TEST_GURT_HASH_NUM_BITS)
#define TEST_GURT_HASH_NUM_THREADS (16)
#define TEST_GURT_HASH_ENTRIES_PER_THREAD \
	(TEST_GURT_HASH_NUM_ENTRIES / TEST_GURT_HASH_NUM_THREADS)
#define TEST_GURT_HASH_KEY_LEN (65L)

struct test_hash_entry {
	int		tl_ref;
	d_list_t	tl_link;
	unsigned char	tl_key[TEST_GURT_HASH_KEY_LEN];
};

static struct test_hash_entry *
test_gurt_hash_link2ptr(d_list_t *link)
{
	return container_of(link, struct test_hash_entry, tl_link);
}

static bool
test_gurt_hash_op_key_cmp(struct d_hash_table *thtab, d_list_t *link,
			  const void *key, unsigned int ksize)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);

	assert_int_equal(ksize, TEST_GURT_HASH_KEY_LEN);
	return !memcmp(tlink->tl_key, key, ksize);
}

static void
test_gurt_hash_op_rec_addref(struct d_hash_table *thtab, d_list_t *link)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);

	tlink->tl_ref++;
}

static bool
test_gurt_hash_op_rec_decref(struct d_hash_table *thtab, d_list_t *link)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);

	tlink->tl_ref--;

	return tlink->tl_ref == 0;
}

static void
test_gurt_hash_op_rec_free(struct d_hash_table *thtab, d_list_t *link)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);

	D_FREE(tlink);
}

static d_hash_table_ops_t th_ops = {
	.hop_key_cmp    = test_gurt_hash_op_key_cmp,
};

/**
 * *arg must be an integer tracking how many times this function is expected
 * to be called
 */
int
test_gurt_hash_traverse_count_cb(d_list_t *rlink, void *arg)
{
	int *expected_count = arg;

	/* Decrement the expected number of entries - if < 0, error */
	(*expected_count)--;
	assert_true(*expected_count >= 0);

	return 0;
}


static struct test_hash_entry **
test_gurt_hash_alloc_items(int num_entries)
{
	struct test_hash_entry	**entries;
	int			  i;
	ssize_t			  j;

	D_ALLOC_ARRAY(entries, num_entries);
	assert_non_null(entries);

	/* Allocate each member of the array */
	for (i = 0; i < num_entries; i++) {
		D_ALLOC_PTR(entries[i]);
		assert_non_null(entries[i]);

		/* Generate a random key */
		j = 0;
		while (j < TEST_GURT_HASH_KEY_LEN) {
			entries[i]->tl_key[j] = rand() & 0xFF;
			j++;
		}

		/*
		 * Last four bytes are used for key index to make sure
		 * keys are unique (little-endian)
		 */
		entries[i]->tl_key[TEST_GURT_HASH_KEY_LEN - 4] = (i & 0xFF);
		entries[i]->tl_key[TEST_GURT_HASH_KEY_LEN - 3] =
			((i >> 8) & 0xFF);
		entries[i]->tl_key[TEST_GURT_HASH_KEY_LEN - 2] =
			((i >> 16) & 0xFF);
		entries[i]->tl_key[TEST_GURT_HASH_KEY_LEN - 1] =
			((i >> 24) & 0xFF);
	}

	return entries;
}

static void
test_gurt_hash_free_items(struct test_hash_entry **entries, int num_entries)
{
	int i;

	if (entries == NULL)
		return;

	for (i = 0; i < num_entries; i++)
		D_FREE(entries[i]);

	D_FREE(entries);
}

static void
test_gurt_hash_empty(void **state)
{
	/* Just test the minimum-size hash table */
	const int		  num_bits = 1;
	struct d_hash_table	 *thtab;
	int			  rc;
	struct test_hash_entry	**entries;
	d_list_t		 *test;
	int			  i;
	int			  expected_count;

	/* Allocate test entries to use */
	entries = test_gurt_hash_alloc_items(TEST_GURT_HASH_NUM_ENTRIES);
	assert_non_null(entries);

	/* Create a minimum-size hash table */
	rc = d_hash_table_create(0, num_bits, NULL, &th_ops, &thtab);
	assert_int_equal(rc, 0);

	/* Traverse the empty hash table and look for entries */
	expected_count = 0;
	rc = d_hash_table_traverse(thtab, test_gurt_hash_traverse_count_cb,
				    &expected_count);
	assert_int_equal(rc, 0);

	/* Get the first element in the table, which should be NULL */
	assert_null(d_hash_rec_first(thtab));

	/* Try to look up the random entries and make sure they fail */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		assert_null(test);
	}

	/* Destroy the hash table, force = false (should fail if not empty) */
	rc = d_hash_table_destroy(thtab, 0);
	assert_int_equal(rc, 0);

	/* Free the temporary keys */
	test_gurt_hash_free_items(entries, TEST_GURT_HASH_NUM_ENTRIES);
}

static d_hash_table_ops_t th_ops_ref = {
	.hop_key_cmp	= test_gurt_hash_op_key_cmp,
	.hop_rec_addref	= test_gurt_hash_op_rec_addref,
	.hop_rec_decref	= test_gurt_hash_op_rec_decref,
	.hop_rec_free	= test_gurt_hash_op_rec_free,
};

static void
test_gurt_hash_insert_lookup_delete(void **state)
{
	const int		  num_bits = TEST_GURT_HASH_NUM_BITS;
	struct d_hash_table	 *thtab;
	int			  rc;
	struct test_hash_entry	**entries;
	d_list_t		 *test;
	int			  i;
	int			  expected_count;
	bool			  deleted;

	/* Allocate test entries to use */
	entries = test_gurt_hash_alloc_items(TEST_GURT_HASH_NUM_ENTRIES);
	assert_non_null(entries);

	/* Create a hash table */
	rc = d_hash_table_create(0, num_bits, NULL, &th_ops, &thtab);
	assert_int_equal(rc, 0);

	/* Insert the entries and make sure they succeed - exclusive = true */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		rc = d_hash_rec_insert(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN,
				       &entries[i]->tl_link, 1);
		assert_int_equal(rc, 0);
	}

	/* Traverse the hash table and count number of entries */
	expected_count = TEST_GURT_HASH_NUM_ENTRIES;
	rc = d_hash_table_traverse(thtab, test_gurt_hash_traverse_count_cb,
				   &expected_count);
	assert_int_equal(rc, 0);
	assert_int_equal(expected_count, 0);

	/* Try to look up the random entries and make sure they succeed */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		/* Make sure the returned pointer is the right one */
		assert_int_equal(test, &entries[i]->tl_link);
	}

	/*
	 * Insert the entries again with unique = 1 and make sure they fail
	 */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		rc = d_hash_rec_insert(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN,
				       &entries[i]->tl_link, 1);
		assert_int_equal(rc, -DER_EXIST);
	}

	/* Try to destroy the hash table, which should fail (not empty) */
	rc = d_hash_table_destroy(thtab, 0);
	assert_int_not_equal(rc, -DER_EXIST);

	/* Remove all entries from the hash table */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		deleted = d_hash_rec_delete(thtab, entries[i]->tl_key,
					    TEST_GURT_HASH_KEY_LEN);
		/* Make sure something was deleted */
		assert_true(deleted);
	}

	/* Lookup test - all should fail */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		assert_null(test);
	}

	/* Traverse the hash table and check there are zero entries */
	expected_count = 0;
	rc = d_hash_table_traverse(thtab, test_gurt_hash_traverse_count_cb,
				   &expected_count);
	assert_int_equal(rc, 0);

	/* Destroy the hash table, force = false (should fail if not empty) */
	rc = d_hash_table_destroy(thtab, 0);
	assert_int_equal(rc, 0);

	/* Free the temporary keys */
	test_gurt_hash_free_items(entries, TEST_GURT_HASH_NUM_ENTRIES);
}

/* Check that addref/decref work with D_HASH_FT_EPHEMERAL
 */
static void
test_gurt_hash_decref(void **state)
{
	/* Just test the minimum-size hash table */
	const int		  num_bits = 1;
	struct d_hash_table	 *thtab;
	int			  rc;
	struct test_hash_entry	 *entry;
	d_list_t		 *test;

	D_ALLOC_PTR(entry);
	assert_non_null(entry);

	/* Create a minimum-size hash table */
	rc = d_hash_table_create(D_HASH_FT_EPHEMERAL, num_bits, NULL,
				 &th_ops_ref, &thtab);
	assert_int_equal(rc, 0);

	rc = d_hash_rec_insert(thtab, entry->tl_key, TEST_GURT_HASH_KEY_LEN,
			       &entry->tl_link, true);
	assert_int_equal(rc, 0);

	/* No ref should be taken on insert */
	assert_int_equal(entry->tl_ref, 0);

	/* This insert should fail */
	rc = d_hash_rec_insert(thtab, entry->tl_key, TEST_GURT_HASH_KEY_LEN,
			       &entry->tl_link, true);
	assert_int_equal(rc, -DER_EXIST);

	/* One ref should be taken by find */
	test = d_hash_rec_find(thtab, entry->tl_key, TEST_GURT_HASH_KEY_LEN);
	assert_non_null(test);
	assert_ptr_equal(test, &entry->tl_link);
	assert_int_equal(entry->tl_ref, 1);

	/* Take two more refs */
	d_hash_rec_addref(thtab, test);
	assert_int_equal(entry->tl_ref, 2);
	d_hash_rec_addref(thtab, test);
	assert_int_equal(entry->tl_ref, 3);

	/* Drop one ref */
	rc = d_hash_rec_ndecref(thtab, 1, test);
	assert_int_equal(rc, 0);
	assert_int_equal(entry->tl_ref, 2);

	/* Drop 20 refs, which should fail but remove and free the descriptor */
	rc = d_hash_rec_ndecref(thtab, 20, test);
	assert_int_equal(rc, -DER_INVAL);

	/* Get the first element in the table, which should be NULL */
	assert_null(d_hash_rec_first(thtab));

	/* Destroy the hash table, force = false (should fail if not empty) */
	rc = d_hash_table_destroy(thtab, 0);
	assert_int_equal(rc, 0);
}

static void
test_gurt_alloc(void **state)
{
	const char *str1 = "Hello World1";
	const char str2[] = "Hello World2";
	char *testptr;
	char *newptr;
	int *testint;
	int *testarray;
	char *path;
	int *ptr1, *ptr2;
	int nr = 10;
	int rc;

	rc = d_log_init();
	assert_int_equal(rc, 0);

	D_REALPATH(path, "//////usr/////");
	assert_non_null(path);
	assert_string_equal(path, "/usr");
	D_FREE(path);
	D_STRNDUP(testptr, str1, 32);
	assert_non_null(testptr);
	assert_string_equal(testptr, str1);
	D_FREE(testptr);
	assert_null(testptr);
	D_STRNDUP(testptr, str2, sizeof(str2));
	assert_non_null(testptr);
	assert_string_equal(testptr, str2);
	D_FREE(testptr);
	assert_null(testptr);
	D_REALLOC(newptr, testptr, 10);
	assert_non_null(newptr);
	assert_null(testptr);
	D_REALLOC(testptr, newptr, 20);
	assert_non_null(testptr);
	assert_null(newptr);
	D_FREE(testptr);
	assert_null(testptr);
	D_ASPRINTF(testptr, "%s", str2);
	assert_non_null(testptr);
	assert_string_equal(testptr, str2);
	D_FREE(testptr);
	assert_null(testptr);
	D_ALLOC_ARRAY(testarray, nr);
	assert_non_null(testarray);
	D_FREE(testarray);
	assert_null(testarray);
	D_ALLOC_PTR(testint);
	assert_non_null(testint);
	D_FREE(testint);
	assert_null(testint);

	D_ALLOC_ARRAY(ptr1, nr);
	assert_non_null(ptr1);

	D_REALLOC_ARRAY(ptr2, ptr1, nr*10);
	assert_non_null(ptr2);
	assert_null(ptr1);

	D_FREE(ptr2);
	assert_null(ptr2);

	d_log_fini();
}

struct hash_thread_arg {
	struct test_hash_entry	**entries;
	struct d_hash_table	 *thtab;
	pthread_barrier_t	 *barrier;

	/* Parallel function to test */
	void *(*fn)(struct hash_thread_arg *);

	int			  thread_idx;

	/*
	 * If the result of the operation should be checked
	 *
	 * Some tests just want to run the operation and see if it crashes
	 */
	bool			  check_result;
};

/**
 * This macro allows a pthread to assert - which would otherwise cause cmocka
 * to segfault without dumping a list of tests that pass/fail. A pthread that
 * returns non-null or an error code from pthread_join should trigger a cmocka
 * assert
 */
#define TEST_THREAD_ASSERT(cond) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "Error in test pthread at %s:%d!" \
				" Failed condition was (%s)\n", \
				__func__, __LINE__, #cond); \
			pthread_exit((void *)1); \
		} \
	} while (0) \

static void *
hash_parallel_insert(struct hash_thread_arg *arg)
{
	int i;
	int rc;

	/* Insert the entries and make sure they succeed - exclusive = true */
	for (i = (arg->thread_idx * TEST_GURT_HASH_ENTRIES_PER_THREAD);
	     i < ((arg->thread_idx + 1) * TEST_GURT_HASH_ENTRIES_PER_THREAD);
	     i++) {
		rc = d_hash_rec_insert(arg->thtab, arg->entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN,
				       &(arg->entries[i]->tl_link), 1);
		if (arg->check_result)
			TEST_THREAD_ASSERT(rc == 0);
	}

	return NULL;
}

/**
 * Parallel thread version which uses the appropriate assert
 *
 * *arg must be an integer tracking how many times this function is expected
 * to be called
 */
int
hash_parallel_traverse_cb(d_list_t *rlink, void *arg)
{
	int *expected_count = arg;

	/* Decrement the expected number of entries - if < 0, error */
	(*expected_count)--;
	TEST_THREAD_ASSERT(*expected_count >= 0);

	return 0;
}

static void *
hash_parallel_traverse(struct hash_thread_arg *arg)
{
	int rc;
	int expected_count;

	/* Traverse the hash table and count number of entries */
	expected_count = TEST_GURT_HASH_NUM_ENTRIES;
	rc = d_hash_table_traverse(arg->thtab,
				   hash_parallel_traverse_cb,
				   &expected_count);
	if (arg->check_result) {
		TEST_THREAD_ASSERT(rc == 0);
		TEST_THREAD_ASSERT(expected_count == 0);
	}

	return NULL;
}

static void *
hash_parallel_lookup(struct hash_thread_arg *arg)
{
	int i;
	d_list_t *test;

	/* Try to look up the random entries and make sure they succeed */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(arg->thtab, arg->entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		if (arg->check_result)
			/* Make sure the returned pointer is the right one */
			TEST_THREAD_ASSERT(test == &(arg->entries[i]->tl_link));
	}

	return NULL;
}

static void *
hash_parallel_addref(struct hash_thread_arg *arg)
{
	int i;

	/* Try to look up the random entries and make sure they succeed */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++)
		d_hash_rec_addref(arg->thtab, &arg->entries[i]->tl_link);

	return NULL;
}

static void *
hash_parallel_decref(struct hash_thread_arg *arg)
{
	int i;

	/* Remove references on the random entries */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++)
		d_hash_rec_decref(arg->thtab, &arg->entries[i]->tl_link);

	return NULL;
}

static void *
hash_parallel_delete(struct hash_thread_arg *arg)
{
	int i;
	bool deleted;

	/* Delete the entries and make sure they succeed */
	for (i = (arg->thread_idx * TEST_GURT_HASH_ENTRIES_PER_THREAD);
	     i < ((arg->thread_idx + 1) * TEST_GURT_HASH_ENTRIES_PER_THREAD);
	     i++) {
		deleted = d_hash_rec_delete(arg->thtab,
					    arg->entries[i]->tl_key,
					    TEST_GURT_HASH_KEY_LEN);
		if (arg->check_result)
			/* Make sure something was deleted */
			TEST_THREAD_ASSERT(deleted);
	}

	return NULL;
}

static void *
hash_parallel_wrapper(void *input)
{
	struct hash_thread_arg *arg = (struct hash_thread_arg *)input;

	/*
	 * Don't use TEST_THREAD_ASSERT here - otherwise the main thread will
	 * never call join because it never proceeds past the barrier
	 *
	 * Better to have the test fail with limited debug info than hang
	 */
	assert_non_null(arg);
	assert_true(arg->thread_idx >= 0);
	assert_non_null(arg->entries);
	assert_non_null(arg->thtab);
	assert_non_null(arg->barrier);
	assert_non_null(arg->fn);

	/* Wait for all workers to be ready to proceed */
	pthread_barrier_wait(arg->barrier);

	/* Call the parallel function under test */
	return arg->fn(arg);
}

static void
_test_gurt_hash_threaded_same_operations(void *(*fn)(struct hash_thread_arg *),
					 struct d_hash_table *thtab,
					 struct test_hash_entry **entries)
{
	pthread_t		 thread_ids[TEST_GURT_HASH_NUM_THREADS];
	struct hash_thread_arg	 thread_args[TEST_GURT_HASH_NUM_THREADS];
	pthread_barrier_t	 barrier;
	int			 i;
	int			 rc;
	int			 thread_rc;
	void			*thread_result;

	/* Use barrier to make sure all threads start at the same time */
	rc = pthread_barrier_init(&barrier, NULL,
				TEST_GURT_HASH_NUM_THREADS + 1);
	assert_int_equal(rc, 0);

	for (i = 0; i < TEST_GURT_HASH_NUM_THREADS; i++) {
		thread_args[i].thread_idx = i;
		thread_args[i].entries = entries;
		thread_args[i].thtab = thtab;
		thread_args[i].barrier = &barrier;
		thread_args[i].fn = fn;
		thread_args[i].check_result = true;
		rc = pthread_create(&thread_ids[i], NULL,
				    hash_parallel_wrapper,
				    &thread_args[i]);
		assert_int_equal(rc, 0);
	}

	/* Wait for all threads to be ready */
	pthread_barrier_wait(&barrier);

	/* Collect all threads */
	rc = 0;
	for (i = 0; i < TEST_GURT_HASH_NUM_THREADS; i++) {
		thread_rc = pthread_join(thread_ids[i], &thread_result);

		/* Accumulate any non-zero bits into rc */
		rc |= thread_rc;

		/* Thread returns non-null if an assert was tripped in-thread */
		rc = rc || (thread_result != NULL);
	}
	assert_int_equal(rc, 0);

	/* Cleanup barrier */
	rc = pthread_barrier_destroy(&barrier);
	assert_int_equal(rc, 0);
}

/**
 * Test insert/traverse/lookup/delete operations in parallel with itself and
 * check the result is correct
 *
 * Each type of operation gets TEST_GURT_HASH_NUM_THREADS threads.
 */
static void
test_gurt_hash_threaded_same_operations(uint32_t ht_feats)
{
	const int		  num_bits = TEST_GURT_HASH_NUM_BITS;
	struct d_hash_table	 *thtab;
	int			  rc;
	struct test_hash_entry	**entries;
	d_list_t		 *test;
	int			  i;
	int			  expected_count;

	/* Allocate test entries to use */
	entries = test_gurt_hash_alloc_items(TEST_GURT_HASH_NUM_ENTRIES);
	assert_non_null(entries);

	/* Create a hash table */
	rc = d_hash_table_create(ht_feats, num_bits, NULL, &th_ops, &thtab);
	assert_int_equal(rc, 0);

	/* Test each operation in parallel */
	_test_gurt_hash_threaded_same_operations(hash_parallel_insert,
						 thtab, entries);
	_test_gurt_hash_threaded_same_operations(hash_parallel_traverse,
						 thtab, entries);
	_test_gurt_hash_threaded_same_operations(hash_parallel_lookup,
						 thtab, entries);
	_test_gurt_hash_threaded_same_operations(hash_parallel_delete,
						 thtab, entries);

	/* Lookup test - all should fail */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		assert_null(test);
	}

	/* Traverse the hash table and check there are zero entries */
	expected_count = 0;
	rc = d_hash_table_traverse(thtab, test_gurt_hash_traverse_count_cb,
				   &expected_count);
	assert_int_equal(rc, 0);

	/* Destroy the hash table, force = false (should fail if not empty) */
	rc = d_hash_table_destroy(thtab, 0);
	assert_int_equal(rc, 0);

	/* Free the temporary keys */
	test_gurt_hash_free_items(entries, TEST_GURT_HASH_NUM_ENTRIES);
}

/**
 * Test insert/traverse/lookup/delete operations in parallel and check for crash
 *
 * Each type of operation gets TEST_GURT_HASH_NUM_THREADS threads.
 */
static void
test_gurt_hash_threaded_concurrent_operations(uint32_t ht_feats)
{
	const int		  num_bits = TEST_GURT_HASH_NUM_BITS;
	struct d_hash_table	 *thtab;
	struct test_hash_entry	**entries;
	pthread_t		  thread_ids[4][TEST_GURT_HASH_NUM_THREADS];
	struct hash_thread_arg	  thread_args[4][TEST_GURT_HASH_NUM_THREADS];
	pthread_barrier_t	  barrier;
	int			  thread_rc;
	void			 *thread_result;
	int			  i;
	int			  j;
	int			  rc;

	/* Allocate test entries to use */
	entries = test_gurt_hash_alloc_items(TEST_GURT_HASH_NUM_ENTRIES);
	assert_non_null(entries);

	/* Create a hash table */
	rc = d_hash_table_create(ht_feats, num_bits, NULL, &th_ops, &thtab);
	assert_int_equal(rc, 0);

	/* Use barrier to make sure all threads start at the same time */
	pthread_barrier_init(&barrier, NULL,
			     TEST_GURT_HASH_NUM_THREADS * 4 + 1);

	for (j = 0; j < 4; j++) {
		for (i = 0; i < TEST_GURT_HASH_NUM_THREADS; i++) {
			thread_args[j][i].thread_idx = i;
			thread_args[j][i].entries = entries;
			thread_args[j][i].thtab = thtab;
			thread_args[j][i].barrier = &barrier;
			switch (j) {
			case 0:
				thread_args[j][i].check_result = true;
				thread_args[j][i].fn = hash_parallel_insert;
				break;
			case 1:
				thread_args[j][i].check_result = false;
				thread_args[j][i].fn = hash_parallel_traverse;
				break;
			case 2:
				thread_args[j][i].check_result = false;
				thread_args[j][i].fn = hash_parallel_lookup;
				break;
			case 3:
				thread_args[j][i].check_result = false;
				thread_args[j][i].fn = hash_parallel_delete;
				break;
			}

			rc = pthread_create(&thread_ids[j][i], NULL,
					    hash_parallel_wrapper,
					    &thread_args[j][i]);
			assert_int_equal(rc, 0);
		}
	}

	/* Wait for all threads to be ready */
	pthread_barrier_wait(&barrier);

	/* Collect all threads */
	rc = 0;
	for (j = 0; j < 4; j++) {
		for (i = 0; i < TEST_GURT_HASH_NUM_THREADS; i++) {
			thread_rc = pthread_join(thread_ids[j][i],
						 &thread_result);

			/* Accumulate any non-zero bits into rc */
			rc |= thread_rc;

			/* Thread returns non-null if an assert was tripped */
			rc = rc || (thread_result != NULL);
		}
	}
	assert_int_equal(rc, 0);

	/* Cleanup barrier */
	pthread_barrier_destroy(&barrier);

	/* Destroy the hash table and delete any remaining entries */
	rc = d_hash_table_destroy(thtab, true);
	assert_int_equal(rc, 0);

	/* Free the temporary keys */
	test_gurt_hash_free_items(entries, TEST_GURT_HASH_NUM_ENTRIES);
}

static void
test_gurt_hash_op_rec_addref_locked(struct d_hash_table *thtab, d_list_t *link)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);

	/*
	 * Increment the reference, protected by the spinlock whose reference
	 * is stored in the hashtable's private data
	 */
	TEST_THREAD_ASSERT(thtab->ht_priv != NULL);
	D_SPIN_LOCK((pthread_spinlock_t *)thtab->ht_priv);

	tlink->tl_ref++;

	D_SPIN_UNLOCK((pthread_spinlock_t *)thtab->ht_priv);
}

static bool
test_gurt_hash_op_rec_decref_locked(struct d_hash_table *thtab, d_list_t *link)
{
	struct test_hash_entry *tlink = test_gurt_hash_link2ptr(link);
	int ref_snapshot;

	/*
	 * Decrement the reference, protected by the spinlock whose reference
	 * is stored in the hashtable's private data
	 */
	TEST_THREAD_ASSERT(thtab->ht_priv != NULL);
	D_SPIN_LOCK((pthread_spinlock_t *)thtab->ht_priv);

	tlink->tl_ref--;

	/* Get a thread-local snapshot of the ref under lock protection */
	ref_snapshot = tlink->tl_ref;

	D_SPIN_UNLOCK((pthread_spinlock_t *)thtab->ht_priv);

	/* If the reference count goes negative there is a bug */
	TEST_THREAD_ASSERT(ref_snapshot >= 0);

	return (ref_snapshot == 0);
}

static d_hash_table_ops_t th_ref_ops = {
	.hop_key_cmp    = test_gurt_hash_op_key_cmp,
	.hop_rec_addref	= test_gurt_hash_op_rec_addref_locked,
	.hop_rec_decref	= test_gurt_hash_op_rec_decref_locked,
};

/* Check the reference count for all entries is the expected value */
static void test_gurt_hash_refcount(struct test_hash_entry **entries,
				    int expected_refcount)
{
	int i;

	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++)
		assert_int_equal(entries[i]->tl_ref, expected_refcount);
}

static void
_test_gurt_hash_parallel_refcounting(uint32_t ht_feats)
{
	const int		  num_bits = TEST_GURT_HASH_NUM_BITS;
	struct d_hash_table	 *thtab;
	int			  rc;
	struct test_hash_entry	**entries;
	d_list_t		 *test;
	int			  expected_count;
	int			  i;
	pthread_spinlock_t	  ref_spin_lock;
	bool			  ephemeral = (ht_feats & D_HASH_FT_EPHEMERAL);
	int			  expected_refcount = 0;

	/* Allocate test entries to use */
	entries = test_gurt_hash_alloc_items(TEST_GURT_HASH_NUM_ENTRIES);
	assert_non_null(entries);

	/* Create a hash table */
	rc = d_hash_table_create(ht_feats, num_bits, NULL,
				 &th_ref_ops, &thtab);
	assert_int_equal(rc, 0);

	/* Create a spinlock to protect the test's reference counting */
	rc = D_SPIN_INIT(&ref_spin_lock, PTHREAD_PROCESS_PRIVATE);
	assert_int_equal(rc, 0);

	/* Stick a pointer to the spinlock in the hash table's private data */
	thtab->ht_priv = (void *)&ref_spin_lock;

	/* Insert the records in parallel */
	_test_gurt_hash_threaded_same_operations(hash_parallel_insert,
						 thtab, entries);
	expected_refcount += (ephemeral ? 0 : 1);
	test_gurt_hash_refcount(entries, expected_refcount);

	/* Look up the records in parallel */
	_test_gurt_hash_threaded_same_operations(hash_parallel_lookup,
						 thtab, entries);
	expected_refcount += TEST_GURT_HASH_NUM_THREADS;
	test_gurt_hash_refcount(entries, expected_refcount);

	/* Add a ref on the records in parallel */
	_test_gurt_hash_threaded_same_operations(hash_parallel_addref,
						 thtab, entries);
	expected_refcount += TEST_GURT_HASH_NUM_THREADS;
	test_gurt_hash_refcount(entries, expected_refcount);

	/* Remove a ref on the records in parallel */
	_test_gurt_hash_threaded_same_operations(hash_parallel_decref,
						 thtab, entries);
	expected_refcount -= TEST_GURT_HASH_NUM_THREADS;
	test_gurt_hash_refcount(entries, expected_refcount);

	/* For non-ephemeral tables, need to remove the records manually */
	if (!ephemeral) {
		_test_gurt_hash_threaded_same_operations(hash_parallel_delete,
							 thtab, entries);
		expected_refcount -= 1;
		test_gurt_hash_refcount(entries, expected_refcount);
	}

	/*
	 * Remove exactly the remaining reference count on each element
	 * For ephemeral tables, this should do the deletion instead of above
	 */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		rc = d_hash_rec_ndecref(thtab, expected_refcount,
					&entries[i]->tl_link);
		assert_int_equal(rc, 0);
	}
	test_gurt_hash_refcount(entries, 0);

	/* Lookup test - all should fail */
	for (i = 0; i < TEST_GURT_HASH_NUM_ENTRIES; i++) {
		test = d_hash_rec_find(thtab, entries[i]->tl_key,
				       TEST_GURT_HASH_KEY_LEN);
		assert_null(test);
	}

	/* Traverse the hash table and check there are zero entries */
	expected_count = 0;
	rc = d_hash_table_traverse(thtab, test_gurt_hash_traverse_count_cb,
				   &expected_count);
	assert_int_equal(rc, 0);

	/* Free the spinlock */
	D_SPIN_DESTROY(&ref_spin_lock);

	/* Destroy the hash table, force = false */
	rc = d_hash_table_destroy(thtab, false);
	assert_int_equal(rc, 0);

	/* Free the temporary keys */
	test_gurt_hash_free_items(entries, TEST_GURT_HASH_NUM_ENTRIES);
}

static void
test_gurt_hash_parallel_same_operations(void **state)
{
	test_gurt_hash_threaded_same_operations(0);
	test_gurt_hash_threaded_same_operations(D_HASH_FT_EPHEMERAL);
	test_gurt_hash_threaded_same_operations(D_HASH_FT_RWLOCK);
	test_gurt_hash_threaded_same_operations(D_HASH_FT_RWLOCK
						| D_HASH_FT_EPHEMERAL);
}

static void
test_gurt_hash_parallel_different_operations(void **state)
{
	test_gurt_hash_threaded_concurrent_operations(0);
	test_gurt_hash_threaded_concurrent_operations(D_HASH_FT_EPHEMERAL);
	test_gurt_hash_threaded_concurrent_operations(D_HASH_FT_RWLOCK);
	test_gurt_hash_threaded_concurrent_operations(D_HASH_FT_RWLOCK
						      | D_HASH_FT_EPHEMERAL);
}

static void
test_gurt_hash_parallel_refcounting(void **state)
{
	_test_gurt_hash_parallel_refcounting(0);
	_test_gurt_hash_parallel_refcounting(D_HASH_FT_EPHEMERAL);
	_test_gurt_hash_parallel_refcounting(D_HASH_FT_RWLOCK);
	_test_gurt_hash_parallel_refcounting(D_HASH_FT_RWLOCK
					     | D_HASH_FT_EPHEMERAL);
}

int
main(int argc, char **argv)
{
	const struct CMUnitTest	tests[] = {
		cmocka_unit_test(test_time),
		cmocka_unit_test(test_d_errstr),
		cmocka_unit_test(test_gurt_list),
		cmocka_unit_test(test_gurt_hlist),
		cmocka_unit_test(test_binheap),
		cmocka_unit_test(test_log),
		cmocka_unit_test(test_gurt_hash_empty),
		cmocka_unit_test(test_gurt_hash_insert_lookup_delete),
		cmocka_unit_test(test_gurt_hash_decref),
		cmocka_unit_test(test_gurt_alloc),
		cmocka_unit_test(test_gurt_hash_parallel_same_operations),
		cmocka_unit_test(test_gurt_hash_parallel_different_operations),
		cmocka_unit_test(test_gurt_hash_parallel_refcounting),
	};

	return cmocka_run_group_tests(tests, init_tests, fini_tests);
}
