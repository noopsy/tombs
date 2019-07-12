/*
  +----------------------------------------------------------------------+
  | tombs                                                                |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */

#ifndef ZEND_TOMBS_GRAVEYARD
# define ZEND_TOMBS_GRAVEYARD

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "zend.h"
#include "zend_API.h"
#include "zend_tombs.h"
#include "zend_tombs_graveyard.h"

static zend_tomb_t zend_tomb_empty = {{0, 0}, NULL, NULL, {NULL, {0, 0}}};

static zend_always_inline void __zend_tomb_create(zend_tomb_t *tomb, zend_op_array *ops) {
    if (ops->scope) {
        tomb->scope = ops->scope->name;
    }

    tomb->function = ops->function_name;

    tomb->location.file       = ops->filename;
    tomb->location.line.start = ops->line_start;
    tomb->location.line.end   = ops->line_end;

    __atomic_store_n(&tomb->state.populated, 1, __ATOMIC_SEQ_CST);
}

static void __zend_tomb_destroy(zend_tomb_t *tomb) {
    if (1 != __atomic_exchange_n(&tomb->state.populated, 0, __ATOMIC_ACQ_REL)) {
        return;
    }
    
    
}

zend_tombs_graveyard_t* zend_tombs_graveyard_create(uint32_t size) {
    size_t zend_tombs_graveyard_size = 
        sizeof(zend_tombs_graveyard_t) + 
        (sizeof(zend_tomb_t) * size);

    zend_tombs_graveyard_t *graveyard = zend_tombs_map(zend_tombs_graveyard_size);

    if (!graveyard) {
        return NULL;
    }

    memset(graveyard, 0, zend_tombs_graveyard_size);

    graveyard->tombs = 
        (zend_tomb_t*) (((char*) graveyard) + sizeof(zend_tombs_graveyard_t));
    graveyard->stat.size = size;
    graveyard->stat.used = 0;
    graveyard->size = zend_tombs_graveyard_size;

    return graveyard;
}

void zend_tombs_graveyard_insert(zend_tombs_graveyard_t *graveyard, zend_ulong index, zend_op_array *ops) {
    zend_tomb_t *tomb = 
        &graveyard->tombs[index];

    if (SUCCESS != __atomic_exchange_n(&tomb->state.inserted, 1, __ATOMIC_ACQ_REL)) {
        return;
    }

    __zend_tomb_create(tomb, ops);
}

void zend_tombs_graveyard_delete(zend_tombs_graveyard_t *graveyard, zend_ulong index) {
    zend_tomb_t *tomb =
        &graveyard->tombs[index];

    if (SUCCESS != __atomic_exchange_n(&tomb->state.deleted, 1, __ATOMIC_ACQ_REL)) {
        return;
    }

    __zend_tomb_destroy(tomb);
}

void zend_tombs_graveyard_dump(zend_tombs_graveyard_t *graveyard, int fd) {
    zend_tomb_t *tomb = graveyard->tombs,
                *end  = tomb + graveyard->stat.size;

    while (tomb < end) {
        if (__atomic_load_n(&tomb->state.populated, __ATOMIC_SEQ_CST)) {
            if (tomb->scope) {
                write(fd, ZSTR_VAL(tomb->scope), ZSTR_LEN(tomb->scope));
                write(fd, ZEND_STRL("::"));
            }
            write(fd, ZSTR_VAL(tomb->function), ZSTR_LEN(tomb->function));
            write(fd, ZEND_STRL("\n"));
        }

        tomb++;
    }
}

void zend_tombs_graveyard_destroy(zend_tombs_graveyard_t *graveyard) {
    zend_tomb_t *tomb = graveyard->tombs,
                *end  = tomb + graveyard->stat.size;

    while (tomb < end) {
        __zend_tomb_destroy(tomb);
        tomb++;
    }
    
    zend_tombs_unmap(graveyard, graveyard->size);
}

#endif	/* ZEND_TOMBS_GRAVEYARD */