#include <iostream>
#include <thread>

#include "sc_gluon_v1_entry_points.h"
#include "sc_gluon_v1_types.h"

#include "ws_client.h"

#define SC_WEBSOCKET_DEBUG 1

static auto* gDeclarations = new std::vector<sc_gluon_function_declarations_v1_t>();

// an instance of this gets passed into the ffi functions by gluon
struct WebSocketState {
    // gluon defined
    sc_gluon_do_callback_v1_f doCallback;
    sc_gluon_release_callback_object_v1_f releaseCallback;

    // ws state
    std::thread thread;
    boost::asio::io_context ioContext;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard;
    std::unordered_map<uint32_t, std::shared_ptr<WebSocketClient>> clients;
};

sc_gluon_out_param_tag_v1 returnTrue(sc_gluon_out_param_or_maybe_diagnostic_v1* &outParam) {
    outParam->out_param.tag = sc_gluon_bool;
    outParam->out_param.data = {true};
    outParam->out_param.owns_data = true;
    outParam->out_param.size = 1;

    return sc_gluon_produced_param;
}

sc_gluon_out_param_tag_v1 webSocketClientInit(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callable_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 3) {
        outParam->maybe_diagnostic = "Wrong number of arguments";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    auto host = std::string(inParams[1].data.character_array, inParams[1].size);
    auto port = inParams[2].data.i32;

    auto client = std::make_shared<WebSocketClient>(state->ioContext, host, port);
    state->clients.insert(std::pair<uint32_t, std::shared_ptr<WebSocketClient>>(uuid, client));

    return returnTrue(outParam);
}


sc_gluon_out_param_tag_v1 webSocketClientConnect(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callable_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 1 || callbackObject == nullptr) {
        outParam->maybe_diagnostic = "Wrong number of arguments or missing callback";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    if (auto it = state->clients.find(uuid); it != state->clients.end()) {
        auto client = it->second;
        client->mSclangConnectionChangeCallback = [=](bool isConnected) {
            auto callbackData = sc_gluon_param_v1_t{
                .data = {isConnected},
                .size = 1,
                .tag = sc_gluon_bool,
                .owns_data = false,
            };
            state->doCallback(callbackObject, &callbackData, 1);
        };
        client->connect();

        return returnTrue(outParam);
    } else {
        outParam->maybe_diagnostic = "Provided client uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
}

sc_gluon_out_param_tag_v1 webSocketClientRegisterMessageCallback(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callable_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 1 || callbackObject == nullptr) {
        outParam->maybe_diagnostic = "Wrong number of arguments or missing callback";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    if (auto it = state->clients.find(uuid); it != state->clients.end()) {
        auto client = it->second;
        client->mSclangOnMessageCallback = [=](WebSocketData& message) {

            if (std::holds_alternative<std::string>(message)) {
                auto stringMessage = std::get<std::string>(message);
                auto callbackData = sc_gluon_param_v1_t{
                    .data = { .character_array = stringMessage.data() },
                    .size = static_cast<uint32_t>(stringMessage.size()),
                    .tag = sc_gluon_char_array,
                    .owns_data = false,
                };
                state->doCallback(callbackObject, &callbackData, 1);
            } else {
                auto bitMessage = std::get<std::vector<uint8_t>>(message);
                auto callbackData = sc_gluon_param_v1_t{
                    .data = { .u8_array = bitMessage.data() },
                    .size = static_cast<uint32_t>(bitMessage.size()),
                    .tag = sc_gluon_u8_array,
                    .owns_data = false,
                };
                state->doCallback(callbackObject, &callbackData, 1);
            }
        };
        return returnTrue(outParam);
    } else {
        outParam->maybe_diagnostic = "Provided client uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
}

sc_gluon_out_param_tag_v1 webSocketClientSendMessage(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callable_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 3) {
        outParam->maybe_diagnostic = "Wrong number of arguments";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    if (auto it = state->clients.find(uuid); it != state->clients.end()) {
        auto client = it->second;

        if (inParams[1].data.boolean) {
            auto messageString = std::string(inParams[2].data.character_array, inParams[2].size);
            auto message = WebSocketData{messageString};
            client->enqueueMessage(message);
        } else {
            auto u8Data = std::vector<u_int8_t>(inParams[2].data.u8_array, inParams[2].data.u8_array + inParams[2].size);
            auto message = WebSocketData{u8Data};
            client->enqueueMessage(message);
        }
        return returnTrue(outParam);
    } else {
        outParam->maybe_diagnostic = "Provided client uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }
}

sc_gluon_out_param_tag_v1 webSocketClientCloseConnection(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callable_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 1) {
        outParam->maybe_diagnostic = "Wrong number of arguments";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    if (auto it = state->clients.find(uuid); it != state->clients.end()) {
        auto client = it->second;
        client->closeConnection();
    } else {
        outParam->maybe_diagnostic = "Provided client uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}


void setupDeclarations() {
    // client declarations
    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "clientInit",
        .ptr=webSocketClientInit,
        .num_parms = 3,  // uuid, host, port
        .accepts_callback = false,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "clientConnect",
        .ptr = webSocketClientConnect,
        .num_parms = 1, // uuid
        .accepts_callback = true,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "clientMessageReceivedCallback",
        .ptr = webSocketClientRegisterMessageCallback,
        .num_parms = 1, // uuid
        .accepts_callback = true,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "clientSendMessage",
        .ptr = webSocketClientSendMessage,
        .num_parms = 3, // uuid, string/uint8 bool, data
        .accepts_callback = false,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "clientCloseConnection",
        .ptr = webSocketClientCloseConnection,
        .num_parms = 1, // uuid
        .accepts_callback = false,
    });
}

void setupIoContext(WebSocketState* state) {
    state->workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(state->ioContext)
    );

    state->thread = std::thread([=]() {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Start WebSocket thread" << std::endl;
#endif

        state->ioContext.run();
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "IO context stopped" << std::endl;
#endif
    });
}

extern "C" {

uint32_t sc_gluon_version() {return 1;}

sc_gluon_library_data_v1_t sc_gluon_load_library(
    sc_gluon_do_callback_v1_f doCallbackFunction,
    sc_gluon_release_callback_object_v1_f releaseCallbackObject,
    sc_gluon_function_declarations_v1_t** const outDeclarations,
    uint32_t* outSize
) {
    setupDeclarations();

    auto state = new WebSocketState{
        .doCallback = doCallbackFunction,
        .releaseCallback = releaseCallbackObject,
    };
    setupIoContext(state);

    *outDeclarations = &gDeclarations->front();
    *outSize = gDeclarations->size();

    return state;
}

void sc_gluon_unload_library(sc_gluon_library_data_v1_t data) {
    std::cout << "sc_gluon_unload_library" << std::endl;

    auto state = reinterpret_cast<WebSocketState*>(data);

    if (state->workGuard) {
        state->workGuard->reset();
        state->workGuard.reset();
    }

    state->ioContext.stop();
    if (state->thread.joinable()) {
        state->thread.join();
    }
    state->ioContext.reset();

    std::destroy_at(&state);
}
}
