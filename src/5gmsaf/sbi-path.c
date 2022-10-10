/*
License: 5G-MAG Public License (v1.0)
Author: Dev Audsin
Copyright: (C) 2022 British Broadcasting Corporation

For full license terms please see the LICENSE file distributed with this
program. If this file is missing then the license can be retrieved from
https://drive.google.com/file/d/1cinCiA778IErENZ3JN52VFW-1ffHpx7Z/view
*/

#include "sbi-path.h"

static int server_cb(ogs_sbi_request_t *request, void *data)
{
    msaf_event_t *e = NULL;
    int rv;

    ogs_assert(request);
    ogs_assert(data);

    e = msaf_event_new(OGS_EVENT_SBI_SERVER);
    ogs_assert(e);

    e->h.sbi.request = request;
    e->h.sbi.data = data;

    rv = ogs_queue_push(ogs_app()->queue, e);
    if (rv != OGS_OK) {
        ogs_error("ogs_queue_push() failed:%d", (int)rv);
        ogs_sbi_request_free(request);
        ogs_event_free(e);
        return OGS_ERROR;
    }

    return OGS_OK;
}

static int client_cb(int status, ogs_sbi_response_t *response, void *data)
{
    msaf_event_t *e = NULL;
    int rv;

    if (status != OGS_OK) {
        ogs_log_message(
                status == OGS_DONE ? OGS_LOG_DEBUG : OGS_LOG_WARN, 0,
                "client_cb() failed [%d]", status);
        return OGS_ERROR;
    }

    ogs_assert(response);

    e = msaf_event_new(OGS_EVENT_SBI_CLIENT);
    ogs_assert(e);
    e->h.sbi.response = response;
    e->h.sbi.data = data;

    rv = ogs_queue_push(ogs_app()->queue, e);
    if (rv != OGS_OK) {
        ogs_error("ogs_queue_push() failed:%d", (int)rv);
        ogs_sbi_response_free(response);
        ogs_event_free(e);
        return OGS_ERROR;
    }

    return OGS_OK;
}

int msaf_sbi_open(void)
{
    ogs_sbi_nf_instance_t *nf_instance = NULL;

    nf_instance = ogs_sbi_self()->nf_instance;
    ogs_assert(nf_instance);
    ogs_sbi_nf_fsm_init(nf_instance);

    ogs_sbi_nf_instance_build_default(nf_instance, OpenAPI_nf_type_AF);

    nf_instance = ogs_sbi_self()->nrf_instance;
    if (nf_instance) {
        ogs_sbi_client_t *client = NULL; 

        client = nf_instance->client;
        ogs_assert(client);
        client->cb = client_cb;

         ogs_sbi_nf_fsm_init(nf_instance);
    } 

     ogs_sbi_subscription_data_build_default(
            OpenAPI_nf_type_BSF, OGS_SBI_SERVICE_NAME_NBSF_MANAGEMENT);

    if (ogs_sbi_server_start_all(server_cb) != OGS_OK)
        return OGS_ERROR;

    return OGS_OK;
}

void msaf_sbi_close(void)
{
    ogs_sbi_client_stop_all();
    ogs_sbi_server_stop_all();
}

bool msaf_sbi_send_request(ogs_sbi_nf_instance_t *nf_instance, void *data)
{
    ogs_assert(nf_instance);

    return ogs_sbi_send_request(nf_instance, client_cb, data);
}


