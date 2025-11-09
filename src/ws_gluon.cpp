#include <iostream>
#include <thread>

#include "sc_gluon_v1_entry_points.h"
#include "sc_gluon_v1_types.h"

#include "ws_client.h"
#include "ws_server.h"

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
    std::unordered_map<uint32_t, std::shared_ptr<WebSocketListener>> listeners;
    std::unordered_map<uint32_t, std::shared_ptr<WebSocketSession>> sessions;
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
    sc_gluon_callback_object_v1_t callbackObject,
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
    sc_gluon_callback_object_v1_t callbackObject,
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
    sc_gluon_callback_object_v1_t callbackObject,
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
    sc_gluon_callback_object_v1_t callbackObject,
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
    sc_gluon_callback_object_v1_t callbackObject,
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

// server declarations

sc_gluon_out_param_tag_v1 webSocketListenerInit(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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

    beast::error_code ec;
    auto listener = std::make_shared<WebSocketListener>(
        state->ioContext,
        host,
        port,
        ec
    );

    if (ec) {
        outParam->maybe_diagnostic = ec.message().c_str();
        return sc_gluon_error_with_non_owned_diagnostic;
    }
    state->listeners.insert(std::pair<uint32_t, std::shared_ptr<WebSocketListener>>(uuid, listener));

    return returnTrue(outParam);
}


sc_gluon_out_param_tag_v1 webSocketListenerStartStop(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
    sc_gluon_param_v1_t* inParams,
    uint32_t numInParams,
    sc_gluon_out_param_or_maybe_diagnostic_v1* outParam
) {
    if (numInParams != 2) {
        outParam->maybe_diagnostic = "Wrong number of arguments";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    auto const state = static_cast<WebSocketState*>(libraryData);

    auto uuid = inParams[0].data.i32;
    auto startConnection = inParams[1].data.boolean;

    if (auto it = state->listeners.find(uuid); it != state->listeners.end()) {
        auto listener = it->second;

        if (startConnection) {
            listener->run();
        } else {
            listener->stop();
        }
    } else {
        outParam->maybe_diagnostic = "Provided listener uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}

sc_gluon_out_param_tag_v1 webSocketListenerNewConnectionCallback(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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

    if (auto it = state->listeners.find(uuid); it != state->listeners.end()) {
        auto listener = it->second;

        listener->mNewSessionCallback = [=](std::shared_ptr<WebSocketSession> session) {
            state->sessions.insert(std::make_pair(session->getSessionId(), session));
            auto callbackData = sc_gluon_param_v1_t{
                .data = { .i32 = session->getSessionId() },
                .size = 1,
                .tag = sc_gluon_i32,
                .owns_data = false,
            };
            state->doCallback(callbackObject, &callbackData, 1);
        };
    } else {
        outParam->maybe_diagnostic = "Provided listener uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}

sc_gluon_out_param_tag_v1 webSocketSessionSendMessage(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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
    auto isString = inParams[1].data.boolean;

    if (auto it = state->sessions.find(uuid); it != state->sessions.end()) {
        auto session = it->second;

        WebSocketData message;
        if (isString) {
            message = WebSocketData(std::string(inParams[2].data.character_array, inParams[2].size));
        } else {
            message = WebSocketData(std::vector<u_int8_t>(inParams[2].data.u8_array, inParams[2].data.u8_array + inParams[2].size));
        }
        session->enqueueMessage(message);
    } else {
        outParam->maybe_diagnostic = "Provided session uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}

sc_gluon_out_param_tag_v1 webSocketSessionConnectionStateCallback(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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

    if (auto it = state->sessions.find(uuid); it != state->sessions.end()) {
        auto session = it->second;

        session->mConnectionStateCallback = [=](bool isConnected) {
            auto callbackData = sc_gluon_param_v1_t{
                .data = { .boolean = isConnected },
                .size = 1,
                .tag = sc_gluon_bool,
                .owns_data = false,
            };
            state->doCallback(callbackObject, &callbackData, 1);
        };
    } else {
        outParam->maybe_diagnostic = "Provided session uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}

sc_gluon_out_param_tag_v1 webSocketSessionMessageCallback(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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

    if (auto it = state->sessions.find(uuid); it != state->sessions.end()) {
        auto session = it->second;

        session->mMessageReceivedCallback = [=](WebSocketData& message) {
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
    } else {
        outParam->maybe_diagnostic = "Provided session uuid does not exist";
        return sc_gluon_error_with_non_owned_diagnostic;
    }

    return returnTrue(outParam);
}

sc_gluon_out_param_tag_v1 webSocketSessionClose(
    sc_gluon_library_data_v1_t libraryData,
    sc_gluon_callback_object_v1_t callbackObject,
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

    if (auto it = state->sessions.find(uuid); it != state->sessions.end()) {
        auto session = it->second;

        session->close();
    } else {
        outParam->maybe_diagnostic = "Provided session uuid does not exist";
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

    // server/session declarations
    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "listenerInit",
        .ptr=webSocketListenerInit,
        .num_parms = 3,  // uuid, host, port
        .accepts_callback = false,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "listenerStartStop",
        .ptr=webSocketListenerStartStop,
        .num_parms = 2,  // uuid, start/stop bool
        .accepts_callback = false,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "listenerNewConnectionCallback",
        .ptr=webSocketListenerNewConnectionCallback,
        .num_parms = 1,  // uuid
        .accepts_callback = true,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "sessionSendMessage",
        .ptr=webSocketSessionSendMessage,
        .num_parms = 3,  // uuid, message kind bool, message data
        .accepts_callback = false,
    });


    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "sessionConnectionStateCallback",
        .ptr=webSocketSessionConnectionStateCallback,
        .num_parms = 1,  // uuid
        .accepts_callback = true,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "sessionMessageCallback",
        .ptr=webSocketSessionMessageCallback,
        .num_parms = 1,  // uuid
        .accepts_callback = true,
    });

    gDeclarations->push_back(sc_gluon_function_declarations_v1_t {
        .name = "sessionClose",
        .ptr=webSocketSessionClose,
        .num_parms = 1,  // uuid
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

uint8_t sc_gluon_load_library(
    sc_gluon_param_v1_t* in_params,
    uint32_t num_in_params,
    sc_gluon_do_callback_v1_f do_callback,
    sc_gluon_release_callback_object_v1_f release_callback,
    sc_gluon_function_declarations_v1_t** const out_decls,
    uint32_t* out_decls_size,
    sc_gluon_library_data_v1_t* out_library_data)
{
    setupDeclarations();

    auto state = new WebSocketState{
        .doCallback = do_callback,
        .releaseCallback = release_callback,
    };
    setupIoContext(state);

    *out_decls = &gDeclarations->front();
    *out_decls_size = gDeclarations->size();

    return 0;
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
