/*****************************************************************************

Copyright (c) 1997, 2015, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2021, 2022, MariaDB Corporation.

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
@file include/ibuf0ibuf.ic
Insert buffer

Created 7/19/1997 Heikki Tuuri
*******************************************************/

/***************************************************************//**
Starts an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_start(
/*===========*/
	mtr_t*	mtr)	/*!< out: mini-transaction */
{
	mtr_start(mtr);
	mtr->enter_ibuf();

	if (high_level_read_only || srv_read_only_mode) {
		mtr_set_log_mode(mtr, MTR_LOG_NO_REDO);
	}

}
/***************************************************************//**
Commits an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_commit(
/*============*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(mtr->is_inside_ibuf());
	ut_d(mtr->exit_ibuf());

	mtr_commit(mtr);
}

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
{
	return(mtr->is_inside_ibuf());
}
