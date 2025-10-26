WebSocketClient {
	var <host;
	var <port;
	var <beastConnectionPtr;
	var <>connected = false; // do not modify! needs to be public so we can modify it via static method
	var <>onMessage;
	var <>onDisconnect;

	classvar <>globalConnections;
	classvar <>ffi;

	*initClass {
		globalConnections = ();

		// do not load ffi yet...

	}

	*loadFFI {
		ffi = FFILibrary.open(
			pathToLibrary: "/Users/scheiba/Library/Application Support/SuperCollider/Extensions/sclang_websocket/sclang_websocket.gluon"
		);
	}

	*new {|host="127.0.0.1", port=8765|
		^super.newCopyArgs(host, port);
	}

	*prReceivedMessage {|ptr, message|
		var connection = globalConnections[ptr];
		if(connection.notNil, {
			connection.onMessage.value(message);
		});
	}

	*prSetConnectionStatus {|ptr, connected|
		var connection = globalConnections[ptr];
		if(connection.notNil, {
			if(connected, {
				connection.connected = true;
			}, {
				connection.onDisconnect.value();
				connection.connected = false;
			});
		});
	}

	connect {
		this.prConnect();
		globalConnections[beastConnectionPtr] = this;
	}

	*prConnected {|ptr|
		var client = globalConnections[ptr];
		if(client.notNil, {
			client.connected = true;
		});
	}

	prConnect {
		// _WebSocketClient_Connect
		// ^this.primitiveFailed;
	}

	close {
		if(beastConnectionPtr.isNil, {
			"Connection is not available - can not close.".warn;
			^this;
		});
		if(connected.not, {
			"Can only close an open connection".warn;
			^this;
		});

		this.prClose();
		connected = false;
		globalConnections[beastConnectionPtr] = nil;
	}

	prClose {
		// _WebSocketClient_Close
		// ^this.primitiveFailed;
	}

	sendMessage {|message|
		if(connected.not, {
			"Can only send a message on an open connection".warn;
			^this;
		});
		if(message.isKindOf(String), {
			^this.prSendStringMessage(message);
		});
		if(message.isKindOf(Int8Array), {
			^this.prSendRawMessage(message);
		});
		"Unknown datatype %".format(message.class).warn;
	}

	prSendStringMessage {|message|
		// _WebSocketClient_SendStringMessage
		// ^this.primitiveFailed;
	}

	prSendRawMessage {|message|
		// _WebSocketClient_SendRawMessage
	    // ^this.primitiveFailed;
	}
}
