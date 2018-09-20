/*! \file */ //Copyright 2011-2016 Tyler Gilbert; All Rights Reserved

#include "son.h"

#if !defined SOS_GIT_HASH
#define SOS_GIT_HASH "??"
#endif

const son_api_t m_son_api = {
    .version = SON_VERSION,
    .git_hash = SOS_GIT_HASH,
    .get_error = son_get_error,
    .create = son_create,
    .create_message = son_create_message,
    .append = son_append,
    .open = son_open,
    .open_message = son_open_message,
    .close = son_close,
    .to_json = son_to_json,
    .open_object = son_open_object,
    .close_object = son_close_object,
    .open_array = son_open_array,
    .close_array = son_close_array,
    .open_data = son_open_data,
    .close_data = son_close_data,
    .write_str = son_write_str,
    .write_num = son_write_num,
    .write_unum = son_write_unum,
    .write_float = son_write_float,
    .write_true = son_write_true,
    .write_false = son_write_false,
    .write_null = son_write_null,
    .write_data = son_write_data,
    .write_open_data = son_write_open_data,
    .read_str = son_read_str,
    .read_num = son_read_num,
    .read_unum = son_read_unum,
    .read_float = son_read_float,
    .read_data = son_read_data,
    .read_bool = son_read_bool,
    .seek = son_seek,
    .edit = son_edit,
    .edit_message = son_edit_message,
    .edit_float = son_edit_float,
    .edit_data = son_edit_data,
    .edit_str = son_edit_str,
    .edit_num = son_edit_num,
    .edit_unum = son_edit_unum,
    .edit_bool = son_edit_bool,
    .send_message = son_send_message,
    .recv_message = son_recv_message,
    .get_message_size = son_get_message_size,
    .seek_next = son_seek_next
};

const son_api_t * son_api(){ return &m_son_api; }
