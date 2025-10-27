WebSocketServer {
	var <host;
	var <port;
	var <uuid;

	var <running = false;
	var <>onConnection;
	var <>connections;

	// shared w/ WebSocketClient
	classvar <ffi;
	classvar <all;

	*initClass {
		Class.initClassTree(Gluon);
		all = IdentityDictionary();
		ffi = Gluon.open(
			pathToLibrary: PathName.new("".resolveRelative).parentPath +/+ "sclang_websocket.gluon",
		);
	}

	*new {|port=8080, host="0.0.0.0"|
		var res;
		var uuid = UniqueID.next;

		// in boost beast vocabulary a server is a listener and a connection a session
		WebSocketServer.ffi.listenerInit(uuid, host, port);
		res = super.newCopyArgs(
			host,
			port,
			uuid,
		).init;
		all[uuid] = res;
		^res.init;
	}

	init {
		WebSocketServer.ffi.listenerNewConnectionCallback(uuid, callback: {|connectionUuid| this.prNewConnection(connectionUuid)});
	}

	start {|onStart|
		WebSocketServer.ffi.listenerStartStop(uuid, true);
	}

	stop {|onStop|
		WebSocketServer.ffi.listenerStartStop(uuid, false);
	}

	broadcast {|message|
		// @todo only send to open connections
		connections.do({|connection|
			connection.send(message);
		});
	}

	prNewConnection {|connectionUuid|
		var connection = WebSocketConnection(
			this,
			connectionUuid,
		);
		"New connection %".format(connectionUuid).postln;
		connections = connections.add(connection);
		onConnection.value(connection);
	}
}

WebSocketConnection {
	var <server;
	var <uuid;

	var <connected = false;
	var <>onMessage;
	var <>onDisconnect;

	classvar <>all;

	*initClass {
		all = IdentityDictionary.new();
	}

	*new {|server, uuid|
		var res = super.newCopyArgs(server, uuid).init;
		all[uuid] = res;
		^res;
	}

	init {
		WebSocketServer.ffi.sessionConnectionStateCallback(uuid, callback: {|isConnected| this.prConnectionStateChanged(isConnected)});
		WebSocketServer.ffi.sessionMessageCallback(uuid, callback: {|message| this.prMessageReceived(message)});
	}

	send {|message|
		WebSocketServer.ffi.sessionSendMessage(uuid, message.isKindOf(String), message);
	}

	close {
		WebSocketServer.ffi.sessionClose(uuid);
	}

	prMessageReceived {|message|
		"Received message %".format(message).postln;
		onMessage.value(message);
	}

	prConnectionStateChanged {|isConnected|
		connected = isConnected;
		if(isConnected.not, {
			onDisconnect.value();
		});
	}
}


WebSocketClient {
	var <host;
	var <port;
	// we identify resources of the ffi using uuids
	var <uuid;

	var <connected = false;
	var <>onMessage;

	*new {|host="127.0.0.1", port=8765|
		var uuid = UniqueID.next;
		WebSocketServer.ffi.clientInit(uuid, host, port);
		^super.newCopyArgs(host, port, uuid).init;
	}

	init {
		WebSocketServer.ffi.clientMessageReceivedCallback(uuid, callback: {|message| this.prMessageReceived(message)});
	}

	connect {|onConnectionChange|
		WebSocketServer.ffi.clientConnect(uuid, callback: {|connectionStatus|
			"Connection status is now %".format(connectionStatus).postln;
			connected = connectionStatus;
			onConnectionChange.value(connectionStatus);
		});
	}

	send {|message|
		if(connected.not, {
			"Can only send a message on an open connection".warn;
			^this;
		});
		WebSocketServer.ffi.clientSendMessage(uuid, message.isKindOf(String), message);
	}

	prMessageReceived {|message|
		"Client has received a message %".format(message).postln;
		onMessage.value(message);
	}

	close {
		if(connected.not, {
			"Can only close an open connection".warn;
			^this;
		});

		WebSocketServer.ffi.clientCloseConnection(uuid);
		connected = false;
	}
}
