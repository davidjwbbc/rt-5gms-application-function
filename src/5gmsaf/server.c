/*
License: 5G-MAG Public License (v1.0)
Author: Dev Audsin
Copyright: (C) 2022 British Broadcasting Corporation

For full license terms please see the LICENSE file distributed with this
program. If this file is missing then the license can be retrieved from
https://drive.google.com/file/d/1cinCiA778IErENZ3JN52VFW-1ffHpx7Z/view
*/

#include "ogs-sbi.h"
#include "server.h"
#include "utilities.h"
#include "version.h"

static bool nf_server_send_problem(
        ogs_sbi_stream_t *stream, OpenAPI_problem_details_t *problem);

static ogs_sbi_response_t *nf_build_response(
        ogs_sbi_message_t *message, int status);

static bool nf_build_content(
        ogs_sbi_http_message_t *http, ogs_sbi_message_t *message);

static char *nf_build_json(ogs_sbi_message_t *message);

ogs_sbi_response_t *nf_server_new_response(char *location, char *content_type, time_t last_modified, char *etag, int cache_control, char *interface) 
{
    ogs_sbi_response_t *response = NULL;
    char *server_api_info = NULL;
    char *server = NULL;

    response = ogs_sbi_response_new();
    ogs_expect_or_return_val(response, NULL);

    if(content_type)
    {
        ogs_sbi_header_set(response->http.headers, "Content-Type", content_type);
    }

    if(location)
    {
        ogs_sbi_header_set(response->http.headers, "Location", location);
    }

    if(last_modified)
    {
    
        ogs_sbi_header_set(response->http.headers, "Last-Modified", get_time(last_modified));
    }

    if(etag)
    {

        ogs_sbi_header_set(response->http.headers, "ETag", etag);
    }

    if(cache_control)
    {
        char *response_cache_control;
        response_cache_control = ogs_msprintf("max-age=%d", cache_control);
        ogs_sbi_header_set(response->http.headers, "Cache-Control", response_cache_control);    	    
	    ogs_free(response_cache_control);
    }

    if(!interface){
        
        server = ogs_msprintf("5GMSdAF-%s/%s %s/%s",msaf_self()->server_name, FIVE_API_RELEASE, MSAF_NAME, MSAF_VERSION);


    } else if(!strcmp(interface, "m1 provisioningSession"))
    {

        server_api_info = ogs_msprintf("(info.title=%s; info.version=%s)", _OpenAPI_provisioning_session_info_title, _OpenAPI_provisioning_session_info_version);
        server = ogs_msprintf("5GMSdAF-%s/%s %s %s/%s",msaf_self()->server_name, FIVE_API_RELEASE, server_api_info, MSAF_NAME, MSAF_VERSION);
    
    } else  if(!strcmp(interface, "m1 contentHostingConfiguration"))
    {

        server_api_info = ogs_msprintf("(info.title=%s; info.version=%s)", _OpenAPI_content_hosting_configuration_info_title, _OpenAPI_content_hosting_configuration_info_version);
        server = ogs_msprintf("5GMSdAF-%s/%s %s %s/%s",msaf_self()->server_name, FIVE_API_RELEASE, server_api_info, MSAF_NAME, MSAF_VERSION);
    
    } else if(!strcmp(interface, "m5"))
    {

        server_api_info = ogs_msprintf("(info.title=%s; info.version=%s)", _OpenAPI_service_access_information_resource_info_title, _OpenAPI_service_access_information_resource_info_version);
        server = ogs_msprintf("5GMSdAF-%s/%s %s %s/%s",msaf_self()->server_name, FIVE_API_RELEASE, server_api_info, MSAF_NAME, MSAF_VERSION);
    
    } else {

        server = ogs_msprintf("5GMSdAF-%s/%s %s/%s",msaf_self()->server_name, FIVE_API_RELEASE, MSAF_NAME, MSAF_VERSION);
    }

    ogs_sbi_header_set(response->http.headers, "Server", server);
    if(server_api_info) ogs_free(server_api_info);
    ogs_free(server);

    return response;


}

ogs_sbi_response_t *nf_server_populate_response(ogs_sbi_response_t *response, int content_length, char *content, int status)
{
    response->http.content_length = content_length;
    response->http.content = content;
    response->status = status;
    return response;

}





static bool nf_server_send_problem(
        ogs_sbi_stream_t *stream, OpenAPI_problem_details_t *problem)
{
    ogs_sbi_message_t message;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(problem);

    memset(&message, 0, sizeof(message));

    message.http.content_type = (char*)"application/problem+json";
    message.ProblemDetails = problem;

    response = nf_build_response(&message, problem->status);
    ogs_assert(response);

    ogs_sbi_server_send_response(stream, response);

    return true;
}


bool nf_server_send_error(ogs_sbi_stream_t *stream,
        int status, int number_of_components, ogs_sbi_message_t *message,
        const char *title, const char *detail, cJSON * problem_detail)
{
    OpenAPI_problem_details_t problem;
    OpenAPI_problem_details_t *problem_details = NULL;

    ogs_assert(stream);

    memset(&problem, 0, sizeof(problem));

    if(problem_detail) {
        problem_details = OpenAPI_problem_details_parseFromJSON(problem_detail);
        problem.invalid_params = problem_details->invalid_params;    
    
    }
	    

    if (message) {
	    int i; 
	    problem.type = ogs_msprintf("/%s/%s",
                message->h.service.name, message->h.api.version);
        ogs_expect_or_return_val(problem.type, false);

        problem.instance = ogs_msprintf("/%s", message->h.resource.component[0]);

	    for (i = 1; i <= number_of_components; i++)
	    {
            ogs_free(problem.instance);
 	        problem.instance = ogs_msprintf("%s/%s", problem.instance, message->h.resource.component[i]);

	    }
        ogs_expect_or_return_val(problem.instance, NULL);
    }
    if (status) {
        problem.is_status = true;
        problem.status = status;
    }
    problem.title = (char*)title;
    problem.detail = (char*)detail;

    nf_server_send_problem(stream, &problem);

    if (problem.type)
        ogs_free(problem.type);
    if (problem.instance)
        ogs_free(problem.instance);
    if (problem.invalid_params)    
        OpenAPI_problem_details_free(problem_details);

    return true;
}

ogs_sbi_response_t *nf_build_response(
        ogs_sbi_message_t *message, int status)
{
    ogs_sbi_response_t *response = NULL;

    ogs_assert(message);

    response = nf_server_new_response(NULL, NULL, 0, NULL, 0, NULL);

    //response = ogs_sbi_response_new();
    ogs_expect_or_return_val(response, NULL);

    response->status = status;

    if (response->status != OGS_SBI_HTTP_STATUS_NO_CONTENT) {
        ogs_expect_or_return_val(true ==
            nf_build_content(&response->http, message), NULL);
    }

    if (message->http.location) {
        ogs_sbi_header_set(response->http.headers, "Location",
                message->http.location);
    }
    if (message->http.cache_control)
        ogs_sbi_header_set(response->http.headers, "Cache-Control",
                message->http.cache_control);

    return response;
}

static bool nf_build_content(
        ogs_sbi_http_message_t *http, ogs_sbi_message_t *message)
{
    ogs_assert(message);
    ogs_assert(http);

        http->content = nf_build_json(message);
        if (http->content) {
            http->content_length = strlen(http->content);
            if (message->http.content_type) {
                ogs_sbi_header_set(http->headers,
                        OGS_SBI_CONTENT_TYPE, message->http.content_type);
            } else {
                ogs_sbi_header_set(http->headers,
                        OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_JSON_TYPE);
            }
        }

    return true;
}

static char *nf_build_json(ogs_sbi_message_t *message)
{
    char *content = NULL;
    cJSON *item = NULL;

    ogs_assert(message);

    if (message->ProblemDetails) {
        item = OpenAPI_problem_details_convertToJSON(message->ProblemDetails);
        ogs_assert(item);
    }
    if (item) {
        content = cJSON_Print(item);
        ogs_assert(content);
        ogs_log_print(OGS_LOG_TRACE, "%s", content);
        cJSON_Delete(item);
    }

    return content;
}
