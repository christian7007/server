/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2016, 2022, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/ibuf0ibuf.h
Insert buffer

Created 7/19/1997 Heikki Tuuri
*******************************************************/

#ifndef ibuf0ibuf_h
#define ibuf0ibuf_h

#include "mtr0mtr.h"
#include "dict0mem.h"
#include "fsp0fsp.h"

/* Possible operations buffered in the insert/whatever buffer.
DO NOT CHANGE THE VALUES OF THESE, THEY ARE STORED ON DISK. */
typedef enum {
	IBUF_OP_INSERT = 0,
	IBUF_OP_DELETE_MARK = 1,
	IBUF_OP_DELETE = 2,

	/* Number of different operation types. */
	IBUF_OP_COUNT = 3
} ibuf_op_t;

/** Insert buffer struct */
struct ibuf_t{
	Atomic_relaxed<ulint> size;	/*!< current size of the ibuf index
					tree, in pages */
	ulint		seg_size;	/*!< allocated pages of the file
					segment containing ibuf header and
					tree */
	bool		empty;		/*!< Protected by the page
					latch of the root page of the
					insert buffer tree
					(FSP_IBUF_TREE_ROOT_PAGE_NO). true
					if and only if the insert
					buffer tree is empty. */
	ulint		free_list_len;	/*!< length of the free list */
	ulint		height;		/*!< tree height */
	dict_index_t*	index;		/*!< insert buffer index */
};

/** The insert buffer control structure */
extern ibuf_t		ibuf;

/* The purpose of the insert buffer is to reduce random disk access.
When we wish to insert a record into a non-unique secondary index and
the B-tree leaf page where the record belongs to is not in the buffer
pool, we insert the record into the insert buffer B-tree, indexed by
(space_id, page_no).  When the page is eventually read into the buffer
pool, we look up the insert buffer B-tree for any modifications to the
page, and apply these upon the completion of the read operation.  This
is called the insert buffer merge. */

/* The insert buffer merge must always succeed.  To guarantee this,
the insert buffer subsystem keeps track of the free space in pages for
which it can buffer operations.  Two bits per page in the insert
buffer bitmap indicate the available space in coarse increments.  The
free bits in the insert buffer bitmap must never exceed the free space
on a page.  It is safe to decrement or reset the bits in the bitmap in
a mini-transaction that is committed before the mini-transaction that
affects the free space.  It is unsafe to increment the bits in a
separately committed mini-transaction, because in crash recovery, the
free bits could momentarily be set too high. */

/******************************************************************//**
Creates the insert buffer data structure at a database startup.
@return DB_SUCCESS or failure */
dberr_t
ibuf_init_at_db_start(void);
/*=======================*/
/***************************************************************//**
Starts an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_start(
/*===========*/
	mtr_t*	mtr)	/*!< out: mini-transaction */
	MY_ATTRIBUTE((nonnull));
/***************************************************************//**
Commits an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_commit(
/*============*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
	MY_ATTRIBUTE((nonnull));
/************************************************************************//**
Resets the free bits of the page in the ibuf bitmap. This is done in a
separate mini-transaction, hence this operation does not restrict
further work to only ibuf bitmap operations, which would result if the
latch to the bitmap page were kept.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to decrement or reset the bits in the bitmap in a mini-transaction
that is committed before the mini-transaction that affects the free
space. */
void
ibuf_reset_free_bits(
/*=================*/
	buf_block_t*	block);	/*!< in: index page; free bits are set to 0
				if the index is a non-clustered
				non-unique, and page level is 0 */
/** Reset the change buffer bitmap free bits of a page.
@param block  possibly a secondary index leaf page
@param mtr    mini-transaction */
void ibuf_reset_free_bits_low(const buf_block_t &block, mtr_t *mtr);
/**********************************************************************//**
Updates the free bits for the two pages to reflect the present state.
Does this in the mtr given, which means that the latching order rules
virtually prevent any further operations until mtr is committed.
NOTE: The free bits in the insert buffer bitmap must never exceed the
free space on a page.  It is safe to set the free bits in the same
mini-transaction that updated the pages. */
void
ibuf_update_free_bits_for_two_pages_low(
/*====================================*/
	buf_block_t*	block1,	/*!< in: index page */
	buf_block_t*	block2,	/*!< in: index page */
	mtr_t*		mtr);	/*!< in: mtr */
/******************************************************************//**
Returns TRUE if the current OS thread is performing an insert buffer
routine.

For instance, a read-ahead of non-ibuf pages is forbidden by threads
that are executing an insert buffer routine.
@return TRUE if inside an insert buffer routine */
UNIV_INLINE
ibool
ibuf_inside(
/*========*/
	const mtr_t*	mtr)	/*!< in: mini-transaction */
	MY_ATTRIBUTE((warn_unused_result));

/** Checks if a page address is an ibuf bitmap page (level 3 page) address.
@param[in]	page_id		page id
@param[in]	zip_size	ROW_FORMAT=COMPRESSED page size, or 0
@return TRUE if a bitmap page */
inline bool ibuf_bitmap_page(const page_id_t page_id, ulint zip_size)
{
	ut_ad(ut_is_2pow(zip_size));
	ulint size = zip_size ? zip_size : srv_page_size;
	return (page_id.page_no() & (size - 1)) == FSP_IBUF_BITMAP_OFFSET;
}

/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]	page_id		page id
@param[in]	zip_size	ROW_FORMAT=COMPRESSED page size, or 0
@param[in]	x_latch		FALSE if relaxed check (avoid latching the
bitmap page)
@param[in,out]	mtr		mtr which will contain an x-latch to the
bitmap page if the page is not one of the fixed address ibuf pages, or NULL,
in which case a new transaction is created.
@return true if level 2 or level 3 page */
bool
ibuf_page_low(
	const page_id_t		page_id,
	ulint			zip_size,
#ifdef UNIV_DEBUG
	bool			x_latch,
#endif /* UNIV_DEBUG */
	mtr_t*			mtr)
	MY_ATTRIBUTE((warn_unused_result));

#ifdef UNIV_DEBUG
/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]	page_id		tablespace/page identifier
@param[in]	zip_size	ROW_FORMAT=COMPRESSED page size, or 0
@param[in,out]	mtr		mini-transaction or NULL
@return TRUE if level 2 or level 3 page */
# define ibuf_page(page_id, zip_size, mtr)	\
	ibuf_page_low(page_id, zip_size, true, mtr)

#else /* UNIV_DEBUG */

/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]	page_id		tablespace/page identifier
@param[in]	zip_size	ROW_FORMAT=COMPRESSED page size, or 0
@param[in,out]	mtr		mini-transaction or NULL
@return TRUE if level 2 or level 3 page */
# define ibuf_page(page_id, zip_size, mtr)	\
	ibuf_page_low(page_id, zip_size, mtr)

#endif /* UNIV_DEBUG */

/** Check whether buffered changes exist for a page.
@param[in]	id		page identifier
@param[in]	zip_size	ROW_FORMAT=COMPRESSED page size, or 0
@return whether buffered changes exist */
bool ibuf_page_exists(const page_id_t id, ulint zip_size);

/** When an index page is read from a disk to the buffer pool, this function
applies any buffered operations to the page and deletes the entries from the
insert buffer. If the page is not read, but created in the buffer pool, this
function deletes its buffered entries from the insert buffer; there can
exist entries for such a page if the page belonged to an index which
subsequently was dropped.
@param block    X-latched page to try to apply changes to, or NULL to discard
@param page_id  page identifier
@param zip_size ROW_FORMAT=COMPRESSED page size, or 0
@return error code */
dberr_t ibuf_merge_or_delete_for_page(buf_block_t *block,
                                      const page_id_t page_id,
                                      ulint zip_size);

/** Delete all change buffer entries for a tablespace,
in DISCARD TABLESPACE, IMPORT TABLESPACE, or read-ahead.
@param[in]	space		missing or to-be-discarded tablespace */
void ibuf_delete_for_discarded_space(uint32_t space);

/** Contract the change buffer by reading pages to the buffer pool.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read
@retval 0 if ibuf.empty */
ulint ibuf_contract();

/** Contracts insert buffer trees by reading pages referring to space_id
to the buffer pool.
@returns number of pages merged.*/
ulint
ibuf_merge_space(
/*=============*/
	ulint	space);	/*!< in: space id */

/********************************************************************
Read the first two bytes from a record's fourth field (counter field in new
records; something else in older records).
@return "counter" field, or ULINT_UNDEFINED if for some reason it can't be read */
ulint
ibuf_rec_get_counter(
/*=================*/
	const rec_t*	rec);	/*!< in: ibuf record */
/******************************************************************//**
Closes insert buffer and frees the data structures. */
void
ibuf_close(void);
/*============*/

/** Check the insert buffer bitmaps on IMPORT TABLESPACE.
@param[in]	trx	transaction
@param[in,out]	space	tablespace being imported
@return DB_SUCCESS or error code */
dberr_t ibuf_check_bitmap_on_import(const trx_t* trx, fil_space_t* space)
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/** Update free bits and buffered bits for bulk loaded page.
@param block   secondary index leaf page
@param mtr     mini-transaction */
void ibuf_set_bitmap_for_bulk_load(buf_block_t *block, mtr_t *mtr);

#define IBUF_HEADER_PAGE_NO	FSP_IBUF_HEADER_PAGE_NO
#define IBUF_TREE_ROOT_PAGE_NO	FSP_IBUF_TREE_ROOT_PAGE_NO

/* The ibuf header page currently contains only the file segment header
for the file segment from which the pages for the ibuf tree are allocated */
#define IBUF_HEADER		PAGE_DATA
#define	IBUF_TREE_SEG_HEADER	0	/* fseg header for ibuf tree */

/* The insert buffer tree itself is always located in space 0. */
#define IBUF_SPACE_ID		static_cast<ulint>(0)

#include "ibuf0ibuf.inl"

#endif
