#include <iostream>
#include <thread>

#include "sc_ffi_v1.h"
#include "sc_ffi_version.h"

#include "ws_server.h"
#include "ws_client.h"

#define SC_WEBSOCKET_DEBUG 1

// some things to make C++ side nicer
using Declaration = sc_ffi_function_declarations_v1_t;
using ReturnTag = sc_ffi_out_param_tag_v1;
using LibraryData = sc_ffi_library_data_v1_t;
using CallbackFunction = sc_ffi_do_callback_v1_f;
using ReleaseCallbackObject = sc_ffi_release_callback_object_v1_f;

// functions to implement
extern "C" {
uint32_t sc_ffi_version();
LibraryData sc_ffi_load_library(CallbackFunction doCallbackFunction,
                                sc_ffi_release_callback_object_v1_f releaseCallbackObject,
                                Declaration** const outDeclarations, uint32_t* outSize);
}
static auto* gDeclarations = new std::vector<Declaration>();

// actual code starts here code

static std::thread gWsThread;
boost::asio::io_context gIoContext;
std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> gWorkGuard;

void setupIoContext() {
    gWorkGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(gIoContext)
    );

    gWsThread = std::thread([]() {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Start WebSocket thread" << std::endl;
#endif

        gIoContext.run();
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "IO context stopped" << std::endl;
#endif
    });
    // gWsThread.detach();
}

static auto gClients = std::vector<WebSocketClient*>();

ReturnTag webSocketClientConnect(sc_ffi_library_data_v1_t library_data, sc_ffi_callable_object_v1_t callbackObject,
                                 sc_ffi_param_v1_t* inParams, uint32_t numInParams,
                                 sc_ffi_out_param_or_maybe_diagnostic_v1* outParam) {

    if (numInParams<=2) {
        outParam->maybe_diagnostic = "Wrong number of arguments";
        return sc_ffi_error_with_non_owned_diagnostic;
    }

    std::cout << "WebSocketClientConnect" << std::endl;

    auto port = std::string("8765");
    auto client = new WebSocketClient(gIoContext);
    gClients.push_back(client);
    client->run("127.0.0.1", port);

    outParam->out_param.tag = sc_ffi_bool;
    outParam->out_param.owns_data = false;
    outParam->out_param.size = 1;
    outParam->out_param.data.boolean = true;

    return sc_ffi_produced_param;
}

void setupDeclarations() {
    gDeclarations->push_back(Declaration {
        .name = "webSocketClientConnect",
        .ptr = webSocketClientConnect,
        .num_parms = 0,
        .accepts_callback = false,
    });
}

sc_ffi_out_param_tag_v1 foo(sc_ffi_library_data_v1_t library_data, sc_ffi_callable_object_v1_t maybe_callback_data,
                            sc_ffi_param_v1_t* in_params, uint32_t num_in_params,
                            sc_ffi_out_param_or_maybe_diagnostic_v1* out_param) {
    std::cout << "HELLO" << std::endl;

    if (num_in_params != 2) {
        out_param->maybe_diagnostic = "wrong number of in params";
        return sc_ffi_error_with_non_owned_diagnostic;
    }

    const sc_ffi_param_v1_t& p1 = in_params[0];
    const sc_ffi_param_v1_t& p2 = in_params[1];

    if (p1.tag != sc_ffi_f64) {
        out_param->maybe_diagnostic = "param 1 is not an f64";
        return sc_ffi_error_with_non_owned_diagnostic;
    }

    if (p2.tag != sc_ffi_f64) {
        out_param->maybe_diagnostic = "param 2 is not an f64";
        return sc_ffi_error_with_non_owned_diagnostic;
    }

    const auto r = p1.data.f64 + p2.data.f64;

    out_param->out_param.data.f64 = r;
    out_param->out_param.tag = sc_ffi_f64;
    out_param->out_param.owns_data = false;
    out_param->out_param.size = 1;

    return sc_ffi_produced_param;
}

struct CallbackPointers {
    sc_ffi_do_callback_v1_f do_callback;
    sc_ffi_release_callback_object_v1_f release_callback;
};


sc_ffi_out_param_tag_v1 helloWebSocket(sc_ffi_library_data_v1_t library_data,
                                       sc_ffi_callable_object_v1_t maybe_callback_data,
                                       struct sc_ffi_param_v1_t* in_params, uint32_t num_in_params,
                                       union sc_ffi_out_param_or_maybe_diagnostic_v1* out_param) {
    std::cout << "HELLO WEBSOCKET <3" << std::endl;

    auto library = reinterpret_cast<CallbackPointers*>(library_data);
    // = captures automatically into thread context
    std::thread([=]() {
        std::cout << "WebSocket is going to sleep now" << std::endl;
        std::this_thread::sleep_for(std::chrono::duration<double>(2.0));
        std::cout << "WebSocket slept enough" << std::endl;
        if (maybe_callback_data != nullptr) {
            // (sc_ffi_callable_object_v1_t, struct sc_ffi_param_v1_t* params, uint32_t num_params
            library->do_callback(maybe_callback_data, nullptr, 0);
            library->release_callback(maybe_callback_data);
        } else {
            std::cout << "Hey - you forgot me to pass the callback data :(" << std::endl;
        }
    }).detach();

    out_param->out_param.tag = sc_ffi_nil;
    out_param->out_param.owns_data = false;
    out_param->out_param.size = 1;
    out_param->out_param.data.nil_ = {};

    return sc_ffi_produced_param;
}

// stores our declarations we want to expose to sclang
// sc_ffi_function_declarations_v1_t gDeclarations[2];

// the actual C-function we want to expose
sc_ffi_out_param_tag_v1
helloWorld(sc_ffi_library_data_v1_t libraryData, // use this to pass along a state into the function
           sc_ffi_callable_object_v1_t maybeCallbackData, sc_ffi_param_v1_t* inParameters, uint32_t numInParameters,
           sc_ffi_out_param_or_maybe_diagnostic_v1* outParam // the data that we return to sclang
) {
    std::cout << "HELLO WORLD <3" << std::endl;

    // we are running inside the sclang vm here, so no blocking operations
    // but you can spawn a thread and use the callback functionality :)

    outParam->out_param.tag = sc_ffi_nil; // choose appropriate type from enum
    outParam->out_param.owns_data = false;
    outParam->out_param.size = 1;
    outParam->out_param.data.nil_ = {}; // this is how we represent nil

    return sc_ffi_produced_param; // choose enum to tell the language if the op succeeded or not
}


// since we want to expose the functions we need to use the C context
extern "C" {

// this function needs to be implemented
uint32_t sc_ffi_version() { return 1; }

// the sc_ffi_load_library function we need to implement
LibraryData sc_ffi_load_library(CallbackFunction doCallbackFunction,
                                sc_ffi_release_callback_object_v1_f releaseCallbackObject,
                                Declaration** const outDeclarations, uint32_t* outSize) {
    setupDeclarations();
    setupIoContext();

    // attach the declarations to our local copy
    *outDeclarations = &gDeclarations->front();
    *outSize = gDeclarations->size();

    // specify the first declaration
    // sc_ffi_function_declarations_v1_t& declaration = gDeclarations[0];
    // declaration.accepts_callback = false; // simple for now
    // declaration.name = "helloWorld"; // name of the function that will be exposed on sclang side
    // declaration.num_parms = 0; // number of parameters our function will consume
    // declaration.ptr = helloWorld; // pointer to our c function

    std::cout << "FFI loaded our lib" << std::endl;

    // return type is actually of type sc_ffi_library_data_v1_t
    // return new CallbackPointers{
    //     .do_callback = doCallbackFunction,
    //     .release_callback = releaseCallbackObject,
    // };
    return nullptr;
}

void sc_ffi_unload_library(sc_ffi_library_data_v1_t data) {
    // keep this reinterpret_cast instead of static_cast for now
    auto foo = reinterpret_cast<CallbackPointers*>(data);
    // the c++ equivalent of delete
    std::destroy_at(&foo);
}
}
